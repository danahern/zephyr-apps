/*
 * eai_ble Zephyr BT backend
 *
 * Uses runtime GATT service registration (CONFIG_BT_GATT_DYNAMIC_DB).
 * Builds bt_gatt_attr[] from eai_ble_service descriptor at register time.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <string.h>

#include <eai_ble/eai_ble.h>

LOG_MODULE_REGISTER(eai_ble, LOG_LEVEL_INF);

/* Max attrs: 1 service + 8 * (char_decl + char_value + CCC) = 25 */
#define MAX_ATTRS 25

/* ── Static storage ─────────────────────────────────────────────────────── */

static const struct eai_ble_callbacks *user_cbs;
static const struct eai_ble_service *reg_svc;
static struct bt_conn *current_conn;
static bool bt_enabled;

static struct bt_gatt_attr attrs[MAX_ATTRS];
static struct bt_uuid_128 uuid_store[1 + EAI_BLE_MAX_CHARS]; /* svc + chars */
static struct bt_gatt_chrc chrc_store[EAI_BLE_MAX_CHARS];
static struct bt_gatt_ccc_managed_user_data ccc_store[EAI_BLE_MAX_CHARS];
static uint8_t char_value_attr_idx[EAI_BLE_MAX_CHARS];
static uint8_t attr_count;

static struct bt_gatt_service gatt_svc = {
	.attrs = attrs,
};

static char adv_name[30]; /* BLE ad name, practical max ~29 bytes */
static uint8_t svc_uuid_ad[16]; /* UUID for scan response */

/* ── UUID conversion ────────────────────────────────────────────────────── */

static void uuid_to_zephyr(const eai_ble_uuid128_t *src,
			    struct bt_uuid_128 *dst)
{
	dst->uuid.type = BT_UUID_TYPE_128;
	memcpy(dst->val, src->bytes, 16);
}

/* ── Wrapper callbacks ──────────────────────────────────────────────────── */

static ssize_t zephyr_write_cb(struct bt_conn *conn,
			       const struct bt_gatt_attr *attr,
			       const void *buf, uint16_t len,
			       uint16_t offset, uint8_t flags)
{
	uint8_t idx = (uint8_t)(uintptr_t)attr->user_data;

	if (reg_svc && idx < reg_svc->char_count &&
	    reg_svc->chars[idx].on_write) {
		reg_svc->chars[idx].on_write(idx, buf, len);
	}
	return len;
}

static ssize_t zephyr_read_cb(struct bt_conn *conn,
			      const struct bt_gatt_attr *attr,
			      void *buf, uint16_t len, uint16_t offset)
{
	uint8_t idx = (uint8_t)(uintptr_t)attr->user_data;
	uint8_t read_buf[244];
	uint16_t read_len = sizeof(read_buf);
	int rc;

	if (!reg_svc || idx >= reg_svc->char_count ||
	    !reg_svc->chars[idx].on_read) {
		return BT_GATT_ERR(BT_ATT_ERR_READ_NOT_PERMITTED);
	}

	rc = reg_svc->chars[idx].on_read(idx, read_buf, &read_len);
	if (rc < 0) {
		return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
	}

	return bt_gatt_attr_read(conn, attr, buf, len, offset,
				 read_buf, read_len);
}

/* ── Build GATT attrs from service descriptor ───────────────────────────── */

static int build_gatt_attrs(const struct eai_ble_service *svc)
{
	uint8_t ai = 0; /* attr index */
	uint8_t ui = 0; /* uuid store index */
	uint8_t ci = 0; /* ccc store index */

	/* Primary service attribute */
	uuid_to_zephyr(&svc->uuid, &uuid_store[ui]);
	attrs[ai++] = (struct bt_gatt_attr)
		BT_GATT_ATTRIBUTE(BT_UUID_GATT_PRIMARY, BT_GATT_PERM_READ,
				  bt_gatt_attr_read_service, NULL,
				  &uuid_store[ui]);
	ui++;

	for (uint8_t i = 0; i < svc->char_count; i++) {
		const struct eai_ble_char *c = &svc->chars[i];
		uint8_t bt_props = 0;
		uint16_t perms = 0;

		uuid_to_zephyr(&c->uuid, &uuid_store[ui]);

		/* Map eai_ble properties to Zephyr BT properties */
		if (c->properties & EAI_BLE_PROP_READ) {
			bt_props |= BT_GATT_CHRC_READ;
			perms |= BT_GATT_PERM_READ;
		}
		if (c->properties & EAI_BLE_PROP_WRITE) {
			bt_props |= BT_GATT_CHRC_WRITE;
			perms |= BT_GATT_PERM_WRITE;
		}
		if (c->properties & EAI_BLE_PROP_NOTIFY) {
			bt_props |= BT_GATT_CHRC_NOTIFY;
		}

		/* Characteristic declaration */
		chrc_store[i] = (struct bt_gatt_chrc){
			.uuid = &uuid_store[ui].uuid,
			.value_handle = 0,
			.properties = bt_props,
		};
		attrs[ai++] = (struct bt_gatt_attr)
			BT_GATT_ATTRIBUTE(BT_UUID_GATT_CHRC,
					  BT_GATT_PERM_READ,
					  bt_gatt_attr_read_chrc, NULL,
					  &chrc_store[i]);

		/* Characteristic value */
		attrs[ai] = (struct bt_gatt_attr){
			.uuid = &uuid_store[ui].uuid,
			.read = c->on_read ? zephyr_read_cb : NULL,
			.write = c->on_write ? zephyr_write_cb : NULL,
			.user_data = (void *)(uintptr_t)i,
			.handle = 0,
			.perm = perms,
		};
		char_value_attr_idx[i] = ai;
		ai++;
		ui++;

		/* CCC descriptor for notify-capable characteristics */
		if (c->properties & EAI_BLE_PROP_NOTIFY) {
			ccc_store[ci] =
				(struct bt_gatt_ccc_managed_user_data)
				BT_GATT_CCC_MANAGED_USER_DATA_INIT(
					NULL, NULL, NULL);

			attrs[ai++] = (struct bt_gatt_attr)
				BT_GATT_ATTRIBUTE(BT_UUID_GATT_CCC,
					BT_GATT_PERM_READ |
					BT_GATT_PERM_WRITE,
					bt_gatt_attr_read_ccc,
					bt_gatt_attr_write_ccc,
					&ccc_store[ci]);
			ci++;
		}
	}

	attr_count = ai;
	return 0;
}

/* ── Connection callbacks ───────────────────────────────────────────────── */

static void on_connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		LOG_ERR("BLE connection failed (err %u)", err);
		return;
	}

	LOG_INF("BLE connected");
	current_conn = bt_conn_ref(conn);

	if (user_cbs && user_cbs->on_connect) {
		user_cbs->on_connect();
	}
}

static int start_advertising(void);

static void on_disconnected(struct bt_conn *conn, uint8_t reason)
{
	LOG_INF("BLE disconnected (reason 0x%02x)", reason);

	if (current_conn) {
		bt_conn_unref(current_conn);
		current_conn = NULL;
	}

	if (user_cbs && user_cbs->on_disconnect) {
		user_cbs->on_disconnect();
	}

	/* Auto-restart advertising */
	start_advertising();
}

BT_CONN_CB_DEFINE(eai_ble_conn_cbs) = {
	.connected = on_connected,
	.disconnected = on_disconnected,
};

/* ── Advertising ────────────────────────────────────────────────────────── */

static int start_advertising(void)
{
	struct bt_data ad[] = {
		BT_DATA_BYTES(BT_DATA_FLAGS,
			      BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
		BT_DATA(BT_DATA_NAME_COMPLETE, adv_name, strlen(adv_name)),
	};
	struct bt_data sd[] = {
		BT_DATA(BT_DATA_UUID128_ALL, svc_uuid_ad,
			sizeof(svc_uuid_ad)),
	};
	int err;

	err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad),
			      sd, ARRAY_SIZE(sd));
	if (err) {
		LOG_WRN("Advertising start failed (err %d)", err);
		return err;
	}

	LOG_INF("Advertising as \"%s\"", adv_name);
	return 0;
}

/* ── Public API ─────────────────────────────────────────────────────────── */

int eai_ble_init(const struct eai_ble_callbacks *cbs)
{
	int err;

	user_cbs = cbs;

	if (!bt_enabled) {
		err = bt_enable(NULL);
		if (err) {
			LOG_ERR("BT init failed (err %d)", err);
			return err;
		}
		bt_enabled = true;
	}

	LOG_INF("BLE initialized");
	return 0;
}

int eai_ble_gatt_register(const struct eai_ble_service *svc)
{
	int err;

	if (!svc || svc->char_count > EAI_BLE_MAX_CHARS) {
		return -EINVAL;
	}

	reg_svc = svc;
	build_gatt_attrs(svc);

	gatt_svc.attr_count = attr_count;
	err = bt_gatt_service_register(&gatt_svc);
	if (err) {
		LOG_ERR("GATT service register failed (err %d)", err);
		return err;
	}

	/* Cache UUID for scan response advertising data */
	memcpy(svc_uuid_ad, svc->uuid.bytes, 16);

	LOG_INF("GATT service registered (%u chars, %u attrs)",
		svc->char_count, attr_count);
	return 0;
}

int eai_ble_adv_start(const char *device_name)
{
	const char *name = device_name ? device_name : CONFIG_BT_DEVICE_NAME;
	size_t len = strlen(name);

	if (len > sizeof(adv_name) - 1) {
		len = sizeof(adv_name) - 1;
	}
	memcpy(adv_name, name, len);
	adv_name[len] = '\0';

	return start_advertising();
}

int eai_ble_adv_stop(void)
{
	return bt_le_adv_stop();
}

int eai_ble_notify(uint8_t char_index, const uint8_t *data, uint16_t len)
{
	if (!reg_svc || char_index >= reg_svc->char_count) {
		return -EINVAL;
	}
	if (!data || len == 0) {
		return -EINVAL;
	}
	if (!current_conn) {
		return -ENOTCONN;
	}

	return bt_gatt_notify(current_conn,
			      &attrs[char_value_attr_idx[char_index]],
			      data, len);
}

bool eai_ble_is_connected(void)
{
	return current_conn != NULL;
}
