/*
 * BLE WiFi Bridge Application
 *
 * Bridges data between BLE NUS and WiFi TCP socket.
 * Target: ESP32-C3 DevKitC
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "ble_nus.h"
#include "wifi_manager.h"
#include "tcp_socket.h"
#include "bridge.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* Semaphore to signal WiFi is ready */
K_SEM_DEFINE(wifi_ready_sem, 0, 1);

/* Callback for BLE data received - queue for TCP */
static void on_ble_rx(const uint8_t *data, uint16_t len)
{
	bridge_queue_ble_to_tcp(data, len);
}

/* Callback for TCP data received - queue for BLE */
static void on_tcp_rx(const uint8_t *data, uint16_t len)
{
	bridge_queue_tcp_to_ble(data, len);
}

/* Callback for WiFi state changes */
static void on_wifi_state(bool connected)
{
	if (connected) {
		LOG_INF("WiFi ready with IP address");
		k_sem_give(&wifi_ready_sem);
	} else {
		LOG_WRN("WiFi connection lost");
		bridge_stop();
		tcp_socket_disconnect();
	}
}

int main(void)
{
	int ret;

	LOG_INF("BLE WiFi Bridge starting");

	/* Initialize bridge module */
	ret = bridge_init();
	if (ret) {
		LOG_ERR("Bridge init failed: %d", ret);
		return ret;
	}

	/* Initialize BLE NUS */
	ret = ble_nus_init(on_ble_rx);
	if (ret) {
		LOG_ERR("BLE NUS init failed: %d", ret);
		return ret;
	}

	/* Start BLE advertising */
	ret = ble_nus_start_advertising();
	if (ret) {
		LOG_ERR("BLE advertising failed: %d", ret);
		return ret;
	}

	/* Initialize WiFi manager */
	ret = wifi_manager_init(on_wifi_state);
	if (ret) {
		LOG_ERR("WiFi manager init failed: %d", ret);
		return ret;
	}

	/* Initialize TCP socket */
	ret = tcp_socket_init(on_tcp_rx);
	if (ret) {
		LOG_ERR("TCP socket init failed: %d", ret);
		return ret;
	}

	LOG_INF("All modules initialized");
	LOG_INF("Connecting to WiFi...");

	/* Connect to WiFi */
	ret = wifi_manager_connect();
	if (ret) {
		LOG_ERR("WiFi connect failed: %d", ret);
		LOG_INF("Continuing with BLE only mode");
	} else {
		/* Wait for IP address */
		ret = k_sem_take(&wifi_ready_sem, K_SECONDS(30));
		if (ret) {
			LOG_WRN("Timeout waiting for IP address");
		} else {
			/* Connect to TCP server */
			ret = tcp_socket_connect();
			if (ret) {
				LOG_ERR("TCP connect failed: %d", ret);
				LOG_INF("Continuing with BLE only mode");
			} else {
				/* Start TCP receive thread */
				tcp_socket_start_rx();

				/* Start the bridge */
				bridge_start();

				LOG_INF("Bridge active: BLE <-> TCP");
			}
		}
	}

	/* Main loop - monitor and reconnect as needed */
	while (1) {
		k_sleep(K_SECONDS(5));

		/* Log status periodically */
		LOG_INF("Status: BLE=%s, WiFi=%s, TCP=%s",
			ble_nus_is_connected() ? "connected" : "advertising",
			wifi_manager_is_connected() ? "connected" : "disconnected",
			tcp_socket_is_connected() ? "connected" : "disconnected");

		/* If WiFi connected but TCP not, try to reconnect */
		if (wifi_manager_is_connected() && !tcp_socket_is_connected()) {
			LOG_INF("Attempting TCP reconnection...");
			ret = tcp_socket_connect();
			if (ret == 0) {
				tcp_socket_start_rx();
				bridge_start();
				LOG_INF("TCP reconnected");
			}
		}
	}

	return 0;
}
