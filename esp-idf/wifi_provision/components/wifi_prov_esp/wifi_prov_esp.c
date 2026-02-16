/*
 * WiFi Provisioning Orchestrator — ESP-IDF implementation.
 * Replaces Zephyr k_work with OSAL work queues, and Zephyr settings
 * with NVS (via wifi_prov_cred_esp.c).
 */

#include "esp_log.h"
#include "nvs_flash.h"

#include <string.h>

#include <wifi_prov/wifi_prov.h>
#include <wifi_prov/wifi_prov_msg.h>
#include <eai_osal/eai_osal.h>

static const char *TAG = "wifi_prov";

static uint8_t cached_ip[4];

/* Work items for deferred processing */
static eai_osal_work_t cred_work;
static eai_osal_work_t factory_reset_work;
static eai_osal_dwork_t auto_connect_work;

static struct wifi_prov_cred pending_cred;

#ifndef CONFIG_WIFI_PROV_AUTO_CONNECT
#define CONFIG_WIFI_PROV_AUTO_CONNECT 1
#endif

static void factory_reset_handler(void *arg)
{
	wifi_prov_factory_reset();
}

static void auto_connect_handler(void *arg)
{
	struct wifi_prov_cred cred;
	int ret;

	ret = wifi_prov_cred_load(&cred);
	if (ret) {
		ESP_LOGW(TAG, "Auto-connect: failed to load credentials: %d", ret);
		return;
	}

	ESP_LOGI(TAG, "Auto-connecting from stored credentials");
	wifi_prov_sm_process_event(WIFI_PROV_EVT_CREDENTIALS_RX);
	wifi_prov_sm_process_event(WIFI_PROV_EVT_WIFI_CONNECTING);
	ret = wifi_prov_wifi_connect(&cred);
	if (ret) {
		ESP_LOGW(TAG, "Auto-connect request failed: %d", ret);
		wifi_prov_sm_process_event(WIFI_PROV_EVT_WIFI_FAILED);
	}
}

static void cred_work_handler(void *arg)
{
	int ret;

	wifi_prov_cred_store(&pending_cred);
	wifi_prov_sm_process_event(WIFI_PROV_EVT_WIFI_CONNECTING);

	ret = wifi_prov_wifi_connect(&pending_cred);
	if (ret) {
		ESP_LOGE(TAG, "WiFi connect request failed: %d", ret);
		wifi_prov_sm_process_event(WIFI_PROV_EVT_WIFI_FAILED);

		uint8_t ip[4] = {0};
		wifi_prov_ble_notify_status(wifi_prov_sm_get_state(), ip);
	}
}

/* --- Internal callbacks --- */

static void on_scan_result_received(const struct wifi_prov_scan_result *result)
{
	wifi_prov_ble_notify_scan_result(result);
}

static void on_scan_done(void)
{
	wifi_prov_sm_process_event(WIFI_PROV_EVT_SCAN_DONE);
}

static void on_scan_trigger(void)
{
	wifi_prov_sm_process_event(WIFI_PROV_EVT_SCAN_TRIGGER);
	wifi_prov_wifi_scan(on_scan_result_received);
}

static void on_credentials_received(const struct wifi_prov_cred *cred)
{
	wifi_prov_sm_process_event(WIFI_PROV_EVT_CREDENTIALS_RX);
	pending_cred = *cred;
	eai_osal_work_submit(&cred_work);
}

static void on_factory_reset_triggered(void)
{
	eai_osal_work_submit(&factory_reset_work);
}

static void on_wifi_state_changed(bool connected)
{
	if (connected) {
		wifi_prov_sm_process_event(WIFI_PROV_EVT_WIFI_CONNECTED);
		wifi_prov_wifi_get_ip(cached_ip);
	} else {
		enum wifi_prov_state state = wifi_prov_sm_get_state();

		if (state == WIFI_PROV_STATE_CONNECTING ||
		    state == WIFI_PROV_STATE_PROVISIONING) {
			wifi_prov_sm_process_event(WIFI_PROV_EVT_WIFI_FAILED);
		} else {
			wifi_prov_sm_process_event(WIFI_PROV_EVT_WIFI_DISCONNECTED);
		}
		memset(cached_ip, 0, sizeof(cached_ip));
	}

	wifi_prov_ble_notify_status(wifi_prov_sm_get_state(), cached_ip);
}

static void on_state_changed(enum wifi_prov_state old_state,
			     enum wifi_prov_state new_state)
{
	ESP_LOGI(TAG, "State: %d -> %d", old_state, new_state);
}

/* --- Public API --- */

int wifi_prov_init(void)
{
	int ret;

	/* Initialize NVS (required for WiFi and credentials) */
	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
	    err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
	}
	ESP_ERROR_CHECK(err);

	wifi_prov_sm_init(on_state_changed);

	eai_osal_work_init(&cred_work, cred_work_handler, NULL);
	eai_osal_work_init(&factory_reset_work, factory_reset_handler, NULL);
	eai_osal_dwork_init(&auto_connect_work, auto_connect_handler, NULL);

	ret = wifi_prov_wifi_init(on_wifi_state_changed);
	if (ret) {
		ESP_LOGE(TAG, "WiFi init failed: %d", ret);
		return ret;
	}

	/* Register scan done callback (ESP-IDF specific) */
	extern void wifi_prov_wifi_set_scan_done_cb(void (*)(void));
	wifi_prov_wifi_set_scan_done_cb(on_scan_done);

	wifi_prov_ble_set_callbacks(on_scan_trigger,
				    on_credentials_received,
				    on_factory_reset_triggered);

	ret = wifi_prov_ble_init();
	if (ret) {
		ESP_LOGE(TAG, "BLE init failed: %d", ret);
		return ret;
	}

	if (CONFIG_WIFI_PROV_AUTO_CONNECT && wifi_prov_cred_exists()) {
		eai_osal_dwork_submit(&auto_connect_work, 2000);
	}

	ESP_LOGI(TAG, "WiFi provisioning initialized");
	return 0;
}

int wifi_prov_start(void)
{
	/* Advertising is started by BLE on_sync callback after init.
	 * This is a no-op if already advertising. */
	return 0;
}

int wifi_prov_factory_reset(void)
{
	wifi_prov_sm_process_event(WIFI_PROV_EVT_FACTORY_RESET);
	wifi_prov_wifi_disconnect();
	wifi_prov_cred_erase();
	memset(cached_ip, 0, sizeof(cached_ip));

	wifi_prov_ble_notify_status(WIFI_PROV_STATE_IDLE, cached_ip);

	ESP_LOGI(TAG, "Factory reset complete");
	return 0;
}

enum wifi_prov_state wifi_prov_get_state(void)
{
	return wifi_prov_sm_get_state();
}

int wifi_prov_get_ip(uint8_t ip_addr[4])
{
	return wifi_prov_wifi_get_ip(ip_addr);
}
