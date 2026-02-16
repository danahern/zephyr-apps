/*
 * WiFi Provisioning App — ESP-IDF version.
 * Same functionality as the Zephyr wifi_provision app:
 * BLE GATT service for provisioning, WiFi scan/connect,
 * credential persistence in NVS, TCP throughput server.
 */

#include "esp_log.h"

#include <wifi_prov/wifi_prov.h>
#include "throughput_server.h"

static const char *TAG = "app";

void app_main(void)
{
	int ret;

	ESP_LOGI(TAG, "WiFi Provisioning App");

	ret = wifi_prov_init();
	if (ret) {
		ESP_LOGE(TAG, "wifi_prov_init failed: %d", ret);
		return;
	}

	ret = wifi_prov_start();
	if (ret) {
		ESP_LOGE(TAG, "wifi_prov_start failed: %d", ret);
		return;
	}

	throughput_server_start();
}
