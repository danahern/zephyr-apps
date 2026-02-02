/*
 * BLE NUS Module Implementation
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/services/nus.h>

#include "ble_nus.h"

LOG_MODULE_REGISTER(ble_nus, LOG_LEVEL_INF);

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

static struct bt_conn *current_conn;
static ble_nus_rx_cb_t rx_callback;

/* Advertising data */
static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

/* Scan response data */
static const struct bt_data sd[] = {
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_NUS_SRV_VAL),
};

static void nus_notif_enabled(bool enabled, void *ctx)
{
	ARG_UNUSED(ctx);
	LOG_INF("NUS notifications %s", enabled ? "enabled" : "disabled");
}

static void nus_received(struct bt_conn *conn, const void *data, uint16_t len,
			 void *ctx)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(ctx);

	LOG_INF("BLE RX: %u bytes", len);

	if (rx_callback) {
		rx_callback((const uint8_t *)data, len);
	}
}

static struct bt_nus_cb nus_callbacks = {
	.notif_enabled = nus_notif_enabled,
	.received = nus_received,
};

static void connected(struct bt_conn *conn, uint8_t err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (err) {
		LOG_ERR("BLE connection failed: %s (err %u)", addr, err);
		return;
	}

	LOG_INF("BLE connected: %s", addr);
	current_conn = bt_conn_ref(conn);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	LOG_INF("BLE disconnected: %s (reason 0x%02x)", addr, reason);

	if (current_conn) {
		bt_conn_unref(current_conn);
		current_conn = NULL;
	}

	/* Restart advertising */
	int ret = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad),
				  sd, ARRAY_SIZE(sd));
	if (ret) {
		LOG_ERR("Failed to restart advertising (err %d)", ret);
	}
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

int ble_nus_init(ble_nus_rx_cb_t callback)
{
	int err;

	rx_callback = callback;

	err = bt_nus_cb_register(&nus_callbacks, NULL);
	if (err) {
		LOG_ERR("Failed to register NUS callbacks (err %d)", err);
		return err;
	}

	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("Bluetooth init failed (err %d)", err);
		return err;
	}

	LOG_INF("BLE NUS initialized");
	return 0;
}

int ble_nus_start_advertising(void)
{
	int err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad),
				  sd, ARRAY_SIZE(sd));
	if (err) {
		LOG_ERR("Advertising failed to start (err %d)", err);
		return err;
	}

	LOG_INF("BLE advertising started as \"%s\"", DEVICE_NAME);
	return 0;
}

int ble_nus_send(const uint8_t *data, uint16_t len)
{
	if (!current_conn) {
		return -ENOTCONN;
	}

	int ret = bt_nus_send(current_conn, data, len);
	if (ret < 0) {
		LOG_ERR("BLE TX failed: %d", ret);
	} else {
		LOG_DBG("BLE TX: %d bytes", ret);
	}

	return ret;
}

bool ble_nus_is_connected(void)
{
	return current_conn != NULL;
}
