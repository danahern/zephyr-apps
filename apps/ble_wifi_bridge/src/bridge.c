/*
 * Bridge Module Implementation
 *
 * Uses message queues to buffer data between BLE and TCP.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>

#include "bridge.h"
#include "ble_nus.h"
#include "tcp_socket.h"

LOG_MODULE_REGISTER(bridge, LOG_LEVEL_INF);

#define BRIDGE_THREAD_STACK_SIZE 2048
#define BRIDGE_THREAD_PRIORITY 6

/* Message structure for queued data */
struct bridge_msg {
	uint8_t data[CONFIG_BRIDGE_MSG_MAX_SIZE];
	uint16_t len;
};

/* Message queues */
K_MSGQ_DEFINE(ble_to_tcp_queue, sizeof(struct bridge_msg),
	      CONFIG_BRIDGE_QUEUE_SIZE, 4);
K_MSGQ_DEFINE(tcp_to_ble_queue, sizeof(struct bridge_msg),
	      CONFIG_BRIDGE_QUEUE_SIZE, 4);

static bool bridge_running;

K_THREAD_STACK_DEFINE(bridge_stack, BRIDGE_THREAD_STACK_SIZE);
static struct k_thread bridge_thread;

/* Process BLE to TCP queue */
static void process_ble_to_tcp(void)
{
	struct bridge_msg msg;

	while (k_msgq_get(&ble_to_tcp_queue, &msg, K_NO_WAIT) == 0) {
		if (tcp_socket_is_connected()) {
			int ret = tcp_socket_send(msg.data, msg.len);
			if (ret < 0) {
				LOG_ERR("Failed to send to TCP: %d", ret);
			} else {
				LOG_INF("Bridge: BLE->TCP %d bytes", msg.len);
			}
		} else {
			LOG_WRN("TCP not connected, dropping message");
		}
	}
}

/* Process TCP to BLE queue */
static void process_tcp_to_ble(void)
{
	struct bridge_msg msg;

	while (k_msgq_get(&tcp_to_ble_queue, &msg, K_NO_WAIT) == 0) {
		if (ble_nus_is_connected()) {
			int ret = ble_nus_send(msg.data, msg.len);
			if (ret < 0) {
				LOG_ERR("Failed to send to BLE: %d", ret);
			} else {
				LOG_INF("Bridge: TCP->BLE %d bytes", msg.len);
			}
		} else {
			LOG_WRN("BLE not connected, dropping message");
		}
	}
}

/* Bridge thread function */
static void bridge_thread_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	LOG_INF("Bridge thread started");

	while (bridge_running) {
		process_ble_to_tcp();
		process_tcp_to_ble();

		/* Small sleep to avoid busy-waiting */
		k_sleep(K_MSEC(10));
	}

	LOG_INF("Bridge thread exiting");
}

int bridge_init(void)
{
	/* Clear any stale messages */
	k_msgq_purge(&ble_to_tcp_queue);
	k_msgq_purge(&tcp_to_ble_queue);

	LOG_INF("Bridge module initialized");
	return 0;
}

void bridge_start(void)
{
	if (bridge_running) {
		LOG_WRN("Bridge already running");
		return;
	}

	bridge_running = true;

	k_thread_create(&bridge_thread, bridge_stack,
			K_THREAD_STACK_SIZEOF(bridge_stack),
			bridge_thread_fn, NULL, NULL, NULL,
			BRIDGE_THREAD_PRIORITY, 0, K_NO_WAIT);

	k_thread_name_set(&bridge_thread, "bridge");

	LOG_INF("Bridge started");
}

void bridge_stop(void)
{
	bridge_running = false;
	LOG_INF("Bridge stopped");
}

int bridge_queue_ble_to_tcp(const uint8_t *data, uint16_t len)
{
	struct bridge_msg msg;

	if (len > CONFIG_BRIDGE_MSG_MAX_SIZE) {
		LOG_ERR("Message too large: %u > %u", len, CONFIG_BRIDGE_MSG_MAX_SIZE);
		return -EMSGSIZE;
	}

	memcpy(msg.data, data, len);
	msg.len = len;

	int ret = k_msgq_put(&ble_to_tcp_queue, &msg, K_NO_WAIT);
	if (ret) {
		LOG_WRN("BLE->TCP queue full, dropping message");
		return -ENOMEM;
	}

	return 0;
}

int bridge_queue_tcp_to_ble(const uint8_t *data, uint16_t len)
{
	struct bridge_msg msg;

	if (len > CONFIG_BRIDGE_MSG_MAX_SIZE) {
		LOG_ERR("Message too large: %u > %u", len, CONFIG_BRIDGE_MSG_MAX_SIZE);
		return -EMSGSIZE;
	}

	memcpy(msg.data, data, len);
	msg.len = len;

	int ret = k_msgq_put(&tcp_to_ble_queue, &msg, K_NO_WAIT);
	if (ret) {
		LOG_WRN("TCP->BLE queue full, dropping message");
		return -ENOMEM;
	}

	return 0;
}
