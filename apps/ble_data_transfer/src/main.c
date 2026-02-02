/*
 * BLE Data Transfer Application
 *
 * Uses Nordic UART Service (NUS) for bidirectional BLE data transfer.
 * Target: nRF54L15DK
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/services/nus.h>

LOG_MODULE_REGISTER(ble_data_transfer, LOG_LEVEL_INF);

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

/* Current BLE connection */
static struct bt_conn *current_conn;

/* Advertising data */
static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

/* Scan response data - includes NUS UUID */
static const struct bt_data sd[] = {
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_NUS_SRV_VAL),
};

/* NUS notification enabled callback */
static void nus_notif_enabled(bool enabled, void *ctx)
{
	ARG_UNUSED(ctx);

	if (enabled) {
		LOG_INF("NUS notifications enabled");
	} else {
		LOG_INF("NUS notifications disabled");
	}
}

/* NUS data received callback */
static void nus_received(struct bt_conn *conn, const void *data, uint16_t len,
			 void *ctx)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(ctx);

	char buf[CONFIG_BT_L2CAP_TX_MTU + 1];
	uint16_t copy_len = MIN(len, sizeof(buf) - 1);

	memcpy(buf, data, copy_len);
	buf[copy_len] = '\0';

	LOG_INF("Received %u bytes: %s", len, buf);

	/* Echo back the received data with a prefix */
	char response[CONFIG_BT_L2CAP_TX_MTU];
	int resp_len = snprintf(response, sizeof(response), "Echo: %s", buf);

	if (resp_len > 0) {
		int err = bt_nus_send(conn, response, resp_len);
		if (err < 0 && err != -ENOTCONN) {
			LOG_ERR("Failed to send NUS data: %d", err);
		} else if (err > 0) {
			LOG_INF("Sent %d bytes", err);
		}
	}
}

/* NUS callbacks */
static struct bt_nus_cb nus_callbacks = {
	.notif_enabled = nus_notif_enabled,
	.received = nus_received,
};

/* Connection callbacks */
static void connected(struct bt_conn *conn, uint8_t err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (err) {
		LOG_ERR("Failed to connect to %s (err %u)", addr, err);
		return;
	}

	LOG_INF("Connected: %s", addr);
	current_conn = bt_conn_ref(conn);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	LOG_INF("Disconnected: %s (reason 0x%02x)", addr, reason);

	if (current_conn) {
		bt_conn_unref(current_conn);
		current_conn = NULL;
	}

	/* Restart advertising */
	int err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad),
				  sd, ARRAY_SIZE(sd));
	if (err) {
		LOG_ERR("Advertising failed to restart (err %d)", err);
	} else {
		LOG_INF("Advertising restarted");
	}
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

int main(void)
{
	int err;

	LOG_INF("BLE Data Transfer starting");

	/* Register NUS callbacks before enabling Bluetooth */
	err = bt_nus_cb_register(&nus_callbacks, NULL);
	if (err) {
		LOG_ERR("Failed to register NUS callbacks (err %d)", err);
		return err;
	}

	/* Enable Bluetooth */
	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("Bluetooth init failed (err %d)", err);
		return err;
	}

	LOG_INF("Bluetooth initialized");

	/* Start advertising */
	err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad),
			      sd, ARRAY_SIZE(sd));
	if (err) {
		LOG_ERR("Advertising failed to start (err %d)", err);
		return err;
	}

	LOG_INF("Advertising started as \"%s\"", DEVICE_NAME);
	LOG_INF("Ready to accept connections");

	/* Main loop - send periodic status */
	uint32_t count = 0;
	while (1) {
		k_sleep(K_SECONDS(10));

		if (current_conn) {
			char status[64];
			int len = snprintf(status, sizeof(status),
					   "Status: uptime=%u sec, msgs=%u\n",
					   k_uptime_get_32() / 1000, count);

			int ret = bt_nus_send(current_conn, status, len);
			if (ret > 0) {
				count++;
				LOG_DBG("Sent status update");
			}
		}
	}

	return 0;
}
