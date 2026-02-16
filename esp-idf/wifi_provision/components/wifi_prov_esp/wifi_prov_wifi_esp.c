/*
 * WiFi Provisioning WiFi — esp_wifi implementation for ESP-IDF.
 * Replaces Zephyr net_mgmt WiFi management.
 */

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"

#include <string.h>

#include <wifi_prov/wifi_prov.h>

static const char *TAG = "wifi_prov_wifi";

static wifi_prov_wifi_state_cb_t state_callback;
static wifi_prov_scan_result_cb_t scan_result_callback;
static void (*scan_done_callback)(void);
static bool wifi_connected;
static bool ip_obtained;
static uint8_t current_ip[4];
static esp_netif_t *sta_netif;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
			       int32_t event_id, void *event_data)
{
	if (event_base == WIFI_EVENT) {
		switch (event_id) {
		case WIFI_EVENT_STA_CONNECTED:
			ESP_LOGI(TAG, "WiFi connected");
			wifi_connected = true;
			break;

		case WIFI_EVENT_STA_DISCONNECTED:
			ESP_LOGI(TAG, "WiFi disconnected");
			wifi_connected = false;
			ip_obtained = false;
			memset(current_ip, 0, sizeof(current_ip));
			if (state_callback) {
				state_callback(false);
			}
			break;

		case WIFI_EVENT_SCAN_DONE: {
			wifi_event_sta_scan_done_t *scan = event_data;
			uint16_t count = scan->number;
			wifi_ap_record_t *records = NULL;

			ESP_LOGI(TAG, "WiFi scan done (%u APs)", count);

			if (count > 0 && scan_result_callback) {
				records = malloc(count * sizeof(wifi_ap_record_t));
				if (records == NULL) {
					ESP_LOGE(TAG, "Failed to alloc scan results");
					break;
				}

				esp_wifi_scan_get_ap_records(&count, records);

				for (uint16_t i = 0; i < count; i++) {
					struct wifi_prov_scan_result result = {0};
					size_t ssid_len = strlen((char *)records[i].ssid);

					if (ssid_len > WIFI_PROV_SSID_MAX_LEN) {
						ssid_len = WIFI_PROV_SSID_MAX_LEN;
					}
					result.ssid_len = ssid_len;
					memcpy(result.ssid, records[i].ssid, ssid_len);
					result.rssi = records[i].rssi;
					result.channel = records[i].primary;

					switch (records[i].authmode) {
					case WIFI_AUTH_OPEN:
						result.security = WIFI_PROV_SEC_NONE;
						break;
					case WIFI_AUTH_WPA_PSK:
						result.security = WIFI_PROV_SEC_WPA_PSK;
						break;
					case WIFI_AUTH_WPA3_PSK:
						result.security = WIFI_PROV_SEC_WPA3_SAE;
						break;
					default:
						result.security = WIFI_PROV_SEC_WPA2_PSK;
						break;
					}

					scan_result_callback(&result);
				}

				free(records);
			}

			scan_result_callback = NULL;
			if (scan_done_callback) {
				scan_done_callback();
				scan_done_callback = NULL;
			}
			break;
		}
		default:
			break;
		}
	} else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
		ip_event_got_ip_t *event = event_data;
		esp_ip4_addr_t addr = event->ip_info.ip;

		current_ip[0] = (addr.addr >> 0) & 0xFF;
		current_ip[1] = (addr.addr >> 8) & 0xFF;
		current_ip[2] = (addr.addr >> 16) & 0xFF;
		current_ip[3] = (addr.addr >> 24) & 0xFF;

		ESP_LOGI(TAG, "IPv4 address: %u.%u.%u.%u",
			 current_ip[0], current_ip[1],
			 current_ip[2], current_ip[3]);

		ip_obtained = true;

		if (state_callback) {
			state_callback(true);
		}
	}
}

int wifi_prov_wifi_init(wifi_prov_wifi_state_cb_t state_cb)
{
	state_callback = state_cb;

	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());

	sta_netif = esp_netif_create_default_wifi_sta();
	if (sta_netif == NULL) {
		ESP_LOGE(TAG, "Failed to create default WiFi STA netif");
		return -1;
	}

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
						   wifi_event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
						   wifi_event_handler, NULL));

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_start());
	ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

	ESP_LOGI(TAG, "WiFi manager initialized");
	return 0;
}

void wifi_prov_wifi_set_scan_done_cb(void (*done_cb)(void))
{
	scan_done_callback = done_cb;
}

int wifi_prov_wifi_scan(wifi_prov_scan_result_cb_t result_cb)
{
	scan_result_callback = result_cb;

	wifi_scan_config_t scan_config = {
		.show_hidden = false,
		.scan_type = WIFI_SCAN_TYPE_ACTIVE,
	};

	esp_err_t err = esp_wifi_scan_start(&scan_config, false);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "WiFi scan request failed: %s", esp_err_to_name(err));
		scan_result_callback = NULL;
		return -1;
	}

	ESP_LOGI(TAG, "WiFi scan started");
	return 0;
}

int wifi_prov_wifi_connect(const struct wifi_prov_cred *cred)
{
	if (!cred) {
		return -1;
	}

	wifi_config_t wifi_config = {0};

	memcpy(wifi_config.sta.ssid, cred->ssid, cred->ssid_len);
	memcpy(wifi_config.sta.password, cred->psk, cred->psk_len);

	switch (cred->security) {
	case WIFI_PROV_SEC_NONE:
		wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
		break;
	case WIFI_PROV_SEC_WPA_PSK:
		wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA_PSK;
		break;
	case WIFI_PROV_SEC_WPA3_SAE:
		wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA3_PSK;
		break;
	default:
		wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
		break;
	}

	ESP_LOGI(TAG, "Connecting to WiFi (SSID len=%u)", cred->ssid_len);

	esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "WiFi set_config failed: %s", esp_err_to_name(err));
		return -1;
	}

	err = esp_wifi_connect();
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "WiFi connect failed: %s", esp_err_to_name(err));
		return -1;
	}

	return 0;
}

int wifi_prov_wifi_disconnect(void)
{
	return esp_wifi_disconnect() == ESP_OK ? 0 : -1;
}

int wifi_prov_wifi_get_ip(uint8_t ip_addr[4])
{
	if (!ip_obtained) {
		return -1;
	}
	memcpy(ip_addr, current_ip, 4);
	return 0;
}

bool wifi_prov_wifi_is_connected(void)
{
	return wifi_connected && ip_obtained;
}
