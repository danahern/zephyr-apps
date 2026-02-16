#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <string.h>

#include <wifi_prov/wifi_prov.h>
#include <wifi_prov/wifi_prov_msg.h>

LOG_MODULE_DECLARE(wifi_prov, LOG_LEVEL_INF);

/* Custom UUID base: a0e4f2b0-XXXX-4c9a-b000-d0e6a7b8c9d0 */
#define BT_UUID_WIFI_PROV_BASE \
	BT_UUID_128_ENCODE(0xa0e4f2b0, 0x0001, 0x4c9a, 0xb000, 0xd0e6a7b8c9d0)

#define BT_UUID_WIFI_PROV_SVC        BT_UUID_DECLARE_128(BT_UUID_WIFI_PROV_BASE)
#define BT_UUID_WIFI_PROV_SCAN_TRIG  BT_UUID_DECLARE_128( \
	BT_UUID_128_ENCODE(0xa0e4f2b0, 0x0002, 0x4c9a, 0xb000, 0xd0e6a7b8c9d0))
#define BT_UUID_WIFI_PROV_SCAN_RES   BT_UUID_DECLARE_128( \
	BT_UUID_128_ENCODE(0xa0e4f2b0, 0x0003, 0x4c9a, 0xb000, 0xd0e6a7b8c9d0))
#define BT_UUID_WIFI_PROV_CRED       BT_UUID_DECLARE_128( \
	BT_UUID_128_ENCODE(0xa0e4f2b0, 0x0004, 0x4c9a, 0xb000, 0xd0e6a7b8c9d0))
#define BT_UUID_WIFI_PROV_STATUS     BT_UUID_DECLARE_128( \
	BT_UUID_128_ENCODE(0xa0e4f2b0, 0x0005, 0x4c9a, 0xb000, 0xd0e6a7b8c9d0))
#define BT_UUID_WIFI_PROV_RESET      BT_UUID_DECLARE_128( \
	BT_UUID_128_ENCODE(0xa0e4f2b0, 0x0006, 0x4c9a, 0xb000, 0xd0e6a7b8c9d0))

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

/* Callbacks set by wifi_prov.c */
static void (*scan_trigger_cb)(void);
static void (*credentials_rx_cb)(const struct wifi_prov_cred *cred);
static void (*factory_reset_cb)(void);

static struct bt_conn *current_conn;
static bool scan_res_notif_enabled;
static bool status_notif_enabled;

/* Advertising data */
static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

/* Scan response with service UUID */
static const uint8_t svc_uuid[] = { BT_UUID_WIFI_PROV_BASE };
static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_UUID128_ALL, svc_uuid, sizeof(svc_uuid)),
};

/* --- GATT Write handlers --- */

static ssize_t write_scan_trigger(struct bt_conn *conn,
				  const struct bt_gatt_attr *attr,
				  const void *buf, uint16_t len,
				  uint16_t offset, uint8_t flags)
{
	LOG_INF("BLE: scan trigger received");
	if (scan_trigger_cb) {
		scan_trigger_cb();
	}
	return len;
}

static ssize_t write_credentials(struct bt_conn *conn,
				 const struct bt_gatt_attr *attr,
				 const void *buf, uint16_t len,
				 uint16_t offset, uint8_t flags)
{
	/* Prepare write validation — accept without processing */
	if (flags & BT_GATT_WRITE_FLAG_PREPARE) {
		return 0;
	}

	struct wifi_prov_cred cred = {0};

	if (wifi_prov_msg_decode_credentials(buf, len, &cred) < 0) {
		LOG_ERR("BLE: invalid credentials message");
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	LOG_INF("BLE: credentials received (SSID len=%u)", cred.ssid_len);
	if (credentials_rx_cb) {
		credentials_rx_cb(&cred);
	}
	return len;
}

static ssize_t write_factory_reset(struct bt_conn *conn,
				   const struct bt_gatt_attr *attr,
				   const void *buf, uint16_t len,
				   uint16_t offset, uint8_t flags)
{
	const uint8_t *data = buf;

	if (len < 1 || data[0] != 0xFF) {
		return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
	}

	LOG_INF("BLE: factory reset triggered");
	if (factory_reset_cb) {
		factory_reset_cb();
	}
	return len;
}

/* --- GATT Read handler for status --- */

static ssize_t read_status(struct bt_conn *conn,
			   const struct bt_gatt_attr *attr,
			   void *buf, uint16_t len, uint16_t offset)
{
	uint8_t status_buf[5];
	uint8_t ip[4] = {0};

	wifi_prov_get_ip(ip);
	wifi_prov_msg_encode_status(wifi_prov_get_state(), ip,
				    status_buf, sizeof(status_buf));

	return bt_gatt_attr_read(conn, attr, buf, len, offset,
				 status_buf, sizeof(status_buf));
}

/* --- CCC changed callbacks --- */

static void scan_res_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	scan_res_notif_enabled = (value == BT_GATT_CCC_NOTIFY);
	LOG_INF("Scan results notifications %s",
		scan_res_notif_enabled ? "enabled" : "disabled");
}

static void status_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	status_notif_enabled = (value == BT_GATT_CCC_NOTIFY);
	LOG_INF("Status notifications %s",
		status_notif_enabled ? "enabled" : "disabled");
}

/* --- GATT Service Definition --- */

BT_GATT_SERVICE_DEFINE(wifi_prov_svc,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_WIFI_PROV_SVC),

	/* Scan Trigger: Write */
	BT_GATT_CHARACTERISTIC(BT_UUID_WIFI_PROV_SCAN_TRIG,
			       BT_GATT_CHRC_WRITE,
			       BT_GATT_PERM_WRITE,
			       NULL, write_scan_trigger, NULL),

	/* Scan Results: Notify */
	BT_GATT_CHARACTERISTIC(BT_UUID_WIFI_PROV_SCAN_RES,
			       BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_NONE,
			       NULL, NULL, NULL),
	BT_GATT_CCC(scan_res_ccc_changed,
		     BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

	/* Credentials: Write (prepare write needed for payloads > ATT MTU) */
	BT_GATT_CHARACTERISTIC(BT_UUID_WIFI_PROV_CRED,
			       BT_GATT_CHRC_WRITE,
			       BT_GATT_PERM_WRITE | BT_GATT_PERM_PREPARE_WRITE,
			       NULL, write_credentials, NULL),

	/* Status: Read + Notify */
	BT_GATT_CHARACTERISTIC(BT_UUID_WIFI_PROV_STATUS,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_READ,
			       read_status, NULL, NULL),
	BT_GATT_CCC(status_ccc_changed,
		     BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

	/* Factory Reset: Write */
	BT_GATT_CHARACTERISTIC(BT_UUID_WIFI_PROV_RESET,
			       BT_GATT_CHRC_WRITE,
			       BT_GATT_PERM_WRITE,
			       NULL, write_factory_reset, NULL),
);

/* --- Connection callbacks --- */

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		LOG_ERR("BLE connection failed (err %u)", err);
		return;
	}

	LOG_INF("BLE connected");
	current_conn = bt_conn_ref(conn);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	LOG_INF("BLE disconnected (reason 0x%02x)", reason);

	if (current_conn) {
		bt_conn_unref(current_conn);
		current_conn = NULL;
	}

	/* Restart advertising */
	wifi_prov_ble_start_advertising();
}

BT_CONN_CB_DEFINE(wifi_prov_conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

/* --- Public API --- */

void wifi_prov_ble_set_callbacks(void (*on_scan_trigger)(void),
				 void (*on_credentials)(const struct wifi_prov_cred *),
				 void (*on_factory_reset)(void))
{
	scan_trigger_cb = on_scan_trigger;
	credentials_rx_cb = on_credentials;
	factory_reset_cb = on_factory_reset;
}

int wifi_prov_ble_init(void)
{
	int err = bt_enable(NULL);

	if (err) {
		LOG_ERR("Bluetooth init failed (err %d)", err);
		return err;
	}

	LOG_INF("BLE initialized");
	return 0;
}

int wifi_prov_ble_start_advertising(void)
{
	int err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad),
				  sd, ARRAY_SIZE(sd));
	if (err) {
		LOG_WRN("Advertising start skipped (err %d)", err);
		return err;
	}

	LOG_INF("BLE advertising as \"%s\"", DEVICE_NAME);
	return 0;
}

int wifi_prov_ble_notify_scan_result(const struct wifi_prov_scan_result *result)
{
	uint8_t buf[64];
	int len;

	if (!current_conn || !scan_res_notif_enabled) {
		return -ENOTCONN;
	}

	len = wifi_prov_msg_encode_scan_result(result, buf, sizeof(buf));
	if (len < 0) {
		return len;
	}

	return bt_gatt_notify(current_conn, &wifi_prov_svc.attrs[3],
			      buf, len);
}

int wifi_prov_ble_notify_status(enum wifi_prov_state state,
				const uint8_t ip[4])
{
	uint8_t buf[5];

	if (!current_conn || !status_notif_enabled) {
		return -ENOTCONN;
	}

	wifi_prov_msg_encode_status(state, ip, buf, sizeof(buf));

	return bt_gatt_notify(current_conn, &wifi_prov_svc.attrs[8],
			      buf, sizeof(buf));
}
