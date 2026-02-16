#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>

#include <wifi_prov/wifi_prov.h>
#include <wifi_prov/wifi_prov_msg.h>

LOG_MODULE_DECLARE(wifi_prov, LOG_LEVEL_INF);

static uint8_t cached_ip[4];

/* --- Internal callbacks --- */

static void on_scan_result_received(const struct wifi_prov_scan_result *result)
{
	wifi_prov_ble_notify_scan_result(result);
}

static void on_scan_trigger(void)
{
	wifi_prov_sm_process_event(WIFI_PROV_EVT_SCAN_TRIGGER);
	wifi_prov_wifi_scan(on_scan_result_received);
}

static void on_credentials_received(const struct wifi_prov_cred *cred)
{
	int ret;

	wifi_prov_sm_process_event(WIFI_PROV_EVT_CREDENTIALS_RX);
	wifi_prov_cred_store(cred);
	wifi_prov_sm_process_event(WIFI_PROV_EVT_WIFI_CONNECTING);

	ret = wifi_prov_wifi_connect(cred);
	if (ret) {
		LOG_ERR("WiFi connect failed: %d", ret);
		wifi_prov_sm_process_event(WIFI_PROV_EVT_WIFI_FAILED);

		uint8_t ip[4] = {0};

		wifi_prov_ble_notify_status(wifi_prov_sm_get_state(), ip);
	}
}

static void on_factory_reset_triggered(void);

static void on_wifi_state_changed(bool connected)
{
	if (connected) {
		wifi_prov_sm_process_event(WIFI_PROV_EVT_WIFI_CONNECTED);
		wifi_prov_wifi_get_ip(cached_ip);
	} else {
		wifi_prov_sm_process_event(WIFI_PROV_EVT_WIFI_DISCONNECTED);
		memset(cached_ip, 0, sizeof(cached_ip));
	}

	wifi_prov_ble_notify_status(wifi_prov_sm_get_state(), cached_ip);
}

static void on_state_changed(enum wifi_prov_state old_state,
			     enum wifi_prov_state new_state)
{
	LOG_INF("State: %d -> %d", old_state, new_state);
}

/* --- Public API --- */

int wifi_prov_init(void)
{
	int ret;

	wifi_prov_sm_init(on_state_changed);

	ret = wifi_prov_wifi_init(on_wifi_state_changed);
	if (ret) {
		LOG_ERR("WiFi init failed: %d", ret);
		return ret;
	}

	wifi_prov_ble_set_callbacks(on_scan_trigger,
				    on_credentials_received,
				    on_factory_reset_triggered);

	ret = wifi_prov_ble_init();
	if (ret) {
		LOG_ERR("BLE init failed: %d", ret);
		return ret;
	}

	if (IS_ENABLED(CONFIG_WIFI_PROV_AUTO_CONNECT) &&
	    wifi_prov_cred_exists()) {
		struct wifi_prov_cred cred;

		ret = wifi_prov_cred_load(&cred);
		if (ret == 0) {
			LOG_INF("Auto-connecting from stored credentials");
			wifi_prov_sm_process_event(WIFI_PROV_EVT_CREDENTIALS_RX);
			wifi_prov_sm_process_event(WIFI_PROV_EVT_WIFI_CONNECTING);
			ret = wifi_prov_wifi_connect(&cred);
			if (ret) {
				LOG_WRN("Auto-connect failed: %d", ret);
				wifi_prov_sm_process_event(WIFI_PROV_EVT_WIFI_FAILED);
			}
		}
	}

	LOG_INF("WiFi provisioning initialized");
	return 0;
}

int wifi_prov_start(void)
{
	return wifi_prov_ble_start_advertising();
}

int wifi_prov_factory_reset(void)
{
	wifi_prov_sm_process_event(WIFI_PROV_EVT_FACTORY_RESET);
	wifi_prov_wifi_disconnect();
	wifi_prov_cred_erase();
	memset(cached_ip, 0, sizeof(cached_ip));

	wifi_prov_ble_notify_status(WIFI_PROV_STATE_IDLE, cached_ip);

	LOG_INF("Factory reset complete");
	return 0;
}

static void on_factory_reset_triggered(void)
{
	wifi_prov_factory_reset();
}

enum wifi_prov_state wifi_prov_get_state(void)
{
	return wifi_prov_sm_get_state();
}

int wifi_prov_get_ip(uint8_t ip_addr[4])
{
	return wifi_prov_wifi_get_ip(ip_addr);
}
