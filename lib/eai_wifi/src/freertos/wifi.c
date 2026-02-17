/*
 * eai_wifi ESP-IDF esp_wifi backend
 *
 * WiFi scan/connect/disconnect via esp_wifi and esp_event.
 * DHCP handled automatically by esp_netif.
 * Power save disabled for reliable incoming connections.
 * EVT_CONNECTED fires only after IP obtained.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"

#include <string.h>
#include <stdlib.h>

#include <eai_wifi/eai_wifi.h>

static const char *TAG = "eai_wifi";

static eai_wifi_event_cb_t event_cb;
static eai_wifi_scan_result_cb_t scan_result_cb;
static eai_wifi_scan_done_cb_t scan_done_cb;
static enum eai_wifi_state current_state;
static uint8_t current_ip[4];
static esp_netif_t *sta_netif;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
			       int32_t event_id, void *event_data)
{
	if (event_base == WIFI_EVENT) {
		switch (event_id) {
		case WIFI_EVENT_STA_CONNECTED:
			ESP_LOGI(TAG, "WiFi connected, waiting for IP");
			break;

		case WIFI_EVENT_STA_DISCONNECTED:
			ESP_LOGI(TAG, "WiFi disconnected");
			memset(current_ip, 0, sizeof(current_ip));

			if (current_state == EAI_WIFI_STATE_CONNECTING) {
				current_state = EAI_WIFI_STATE_DISCONNECTED;
				if (event_cb) {
					event_cb(EAI_WIFI_EVT_CONNECT_FAILED);
				}
			} else if (current_state == EAI_WIFI_STATE_CONNECTED) {
				current_state = EAI_WIFI_STATE_DISCONNECTED;
				if (event_cb) {
					event_cb(EAI_WIFI_EVT_DISCONNECTED);
				}
			} else {
				current_state = EAI_WIFI_STATE_DISCONNECTED;
			}
			break;

		case WIFI_EVENT_SCAN_DONE: {
			wifi_event_sta_scan_done_t *scan = event_data;
			uint16_t count = scan->number;
			wifi_ap_record_t *records = NULL;

			ESP_LOGI(TAG, "WiFi scan done (%u APs)", count);

			if (count > 0 && scan_result_cb) {
				records = malloc(count * sizeof(wifi_ap_record_t));
				if (records == NULL) {
					ESP_LOGE(TAG, "Failed to alloc scan results");
					goto scan_cleanup;
				}

				esp_wifi_scan_get_ap_records(&count, records);

				for (uint16_t i = 0; i < count; i++) {
					struct eai_wifi_scan_result result = {0};
					size_t ssid_len = strlen((char *)records[i].ssid);

					if (ssid_len > EAI_WIFI_SSID_MAX_LEN) {
						ssid_len = EAI_WIFI_SSID_MAX_LEN;
					}
					result.ssid_len = ssid_len;
					memcpy(result.ssid, records[i].ssid, ssid_len);
					result.rssi = records[i].rssi;
					result.channel = records[i].primary;

					switch (records[i].authmode) {
					case WIFI_AUTH_OPEN:
						result.security = EAI_WIFI_SEC_OPEN;
						break;
					case WIFI_AUTH_WPA_PSK:
						result.security = EAI_WIFI_SEC_WPA_PSK;
						break;
					case WIFI_AUTH_WPA3_PSK:
						result.security = EAI_WIFI_SEC_WPA3_SAE;
						break;
					default:
						result.security = EAI_WIFI_SEC_WPA2_PSK;
						break;
					}

					scan_result_cb(&result);
				}

				free(records);
			}

scan_cleanup:
			{
				eai_wifi_scan_done_cb_t cb = scan_done_cb;

				scan_result_cb = NULL;
				scan_done_cb = NULL;

				if (current_state == EAI_WIFI_STATE_SCANNING) {
					current_state = EAI_WIFI_STATE_DISCONNECTED;
				}

				if (cb) {
					cb(0);
				}
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

		current_state = EAI_WIFI_STATE_CONNECTED;

		if (event_cb) {
			event_cb(EAI_WIFI_EVT_CONNECTED);
		}
	}
}

int eai_wifi_init(void)
{
	current_state = EAI_WIFI_STATE_DISCONNECTED;
	memset(current_ip, 0, sizeof(current_ip));

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

void eai_wifi_set_event_callback(eai_wifi_event_cb_t cb)
{
	event_cb = cb;
}

int eai_wifi_scan(eai_wifi_scan_result_cb_t on_result,
		  eai_wifi_scan_done_cb_t on_done)
{
	if (!on_result) {
		return -1;
	}

	scan_result_cb = on_result;
	scan_done_cb = on_done;
	current_state = EAI_WIFI_STATE_SCANNING;

	wifi_scan_config_t scan_config = {
		.show_hidden = false,
		.scan_type = WIFI_SCAN_TYPE_ACTIVE,
	};

	esp_err_t err = esp_wifi_scan_start(&scan_config, false);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "WiFi scan request failed: %s",
			 esp_err_to_name(err));
		scan_result_cb = NULL;
		scan_done_cb = NULL;
		current_state = EAI_WIFI_STATE_DISCONNECTED;
		return -1;
	}

	ESP_LOGI(TAG, "WiFi scan started");
	return 0;
}

int eai_wifi_connect(const uint8_t *ssid, uint8_t ssid_len,
		     const uint8_t *psk, uint8_t psk_len,
		     enum eai_wifi_security sec)
{
	if (!ssid || ssid_len == 0) {
		return -1;
	}

	wifi_config_t wifi_config = {0};

	memcpy(wifi_config.sta.ssid, ssid, ssid_len);
	if (psk && psk_len > 0) {
		memcpy(wifi_config.sta.password, psk, psk_len);
	}

	switch (sec) {
	case EAI_WIFI_SEC_OPEN:
		wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
		break;
	case EAI_WIFI_SEC_WPA_PSK:
		wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA_PSK;
		break;
	case EAI_WIFI_SEC_WPA3_SAE:
		wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA3_PSK;
		break;
	default:
		wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
		break;
	}

	current_state = EAI_WIFI_STATE_CONNECTING;

	ESP_LOGI(TAG, "Connecting to WiFi (SSID len=%u)", ssid_len);

	esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "WiFi set_config failed: %s",
			 esp_err_to_name(err));
		current_state = EAI_WIFI_STATE_DISCONNECTED;
		return -1;
	}

	err = esp_wifi_connect();
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "WiFi connect failed: %s", esp_err_to_name(err));
		current_state = EAI_WIFI_STATE_DISCONNECTED;
		return -1;
	}

	return 0;
}

int eai_wifi_disconnect(void)
{
	return esp_wifi_disconnect() == ESP_OK ? 0 : -1;
}

enum eai_wifi_state eai_wifi_get_state(void)
{
	return current_state;
}

int eai_wifi_get_ip(uint8_t ip[4])
{
	if (current_state != EAI_WIFI_STATE_CONNECTED) {
		return -1;
	}

	memcpy(ip, current_ip, 4);
	return 0;
}
