/*
 * WiFi Provisioning BLE — NimBLE implementation for ESP-IDF.
 * Same service/characteristic UUIDs and wire protocol as the Zephyr version.
 */

#include "esp_log.h"
#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include <wifi_prov/wifi_prov.h>
#include <wifi_prov/wifi_prov_msg.h>

static const char *TAG = "wifi_prov_ble";

#define DEVICE_NAME "WiFi Prov"

/* UUID base: a0e4f2b0-XXXX-4c9a-b000-d0e6a7b8c9d0 (little-endian for NimBLE) */
static const ble_uuid128_t svc_uuid = BLE_UUID128_INIT(
	0xd0, 0xc9, 0xb8, 0xa7, 0xe6, 0xd0, 0x00, 0xb0,
	0x9a, 0x4c, 0x01, 0x00, 0xb0, 0xf2, 0xe4, 0xa0);

static const ble_uuid128_t chr_scan_trig_uuid = BLE_UUID128_INIT(
	0xd0, 0xc9, 0xb8, 0xa7, 0xe6, 0xd0, 0x00, 0xb0,
	0x9a, 0x4c, 0x02, 0x00, 0xb0, 0xf2, 0xe4, 0xa0);

static const ble_uuid128_t chr_scan_res_uuid = BLE_UUID128_INIT(
	0xd0, 0xc9, 0xb8, 0xa7, 0xe6, 0xd0, 0x00, 0xb0,
	0x9a, 0x4c, 0x03, 0x00, 0xb0, 0xf2, 0xe4, 0xa0);

static const ble_uuid128_t chr_cred_uuid = BLE_UUID128_INIT(
	0xd0, 0xc9, 0xb8, 0xa7, 0xe6, 0xd0, 0x00, 0xb0,
	0x9a, 0x4c, 0x04, 0x00, 0xb0, 0xf2, 0xe4, 0xa0);

static const ble_uuid128_t chr_status_uuid = BLE_UUID128_INIT(
	0xd0, 0xc9, 0xb8, 0xa7, 0xe6, 0xd0, 0x00, 0xb0,
	0x9a, 0x4c, 0x05, 0x00, 0xb0, 0xf2, 0xe4, 0xa0);

static const ble_uuid128_t chr_reset_uuid = BLE_UUID128_INIT(
	0xd0, 0xc9, 0xb8, 0xa7, 0xe6, 0xd0, 0x00, 0xb0,
	0x9a, 0x4c, 0x06, 0x00, 0xb0, 0xf2, 0xe4, 0xa0);

/* Callbacks set by orchestrator */
static void (*scan_trigger_cb)(void);
static void (*credentials_rx_cb)(const struct wifi_prov_cred *cred);
static void (*factory_reset_cb)(void);

/* Connection and attribute handles */
static uint16_t conn_handle;
static bool is_connected;
static uint16_t scan_res_attr_handle;
static uint16_t status_attr_handle;

/* --- GATT access callbacks --- */

static int gatt_scan_trigger_cb(uint16_t chn, uint16_t attr_handle,
				struct ble_gatt_access_ctxt *ctxt, void *arg)
{
	ESP_LOGI(TAG, "BLE: scan trigger received");
	if (scan_trigger_cb) {
		scan_trigger_cb();
	}
	return 0;
}

static int gatt_credentials_cb(uint16_t chn, uint16_t attr_handle,
			       struct ble_gatt_access_ctxt *ctxt, void *arg)
{
	struct os_mbuf *om = ctxt->om;
	uint16_t len = OS_MBUF_PKTLEN(om);
	uint8_t buf[128];

	if (len > sizeof(buf)) {
		return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
	}

	int rc = ble_hs_mbuf_to_flat(om, buf, len, NULL);
	if (rc != 0) {
		return BLE_ATT_ERR_UNLIKELY;
	}

	struct wifi_prov_cred cred = {0};
	if (wifi_prov_msg_decode_credentials(buf, len, &cred) < 0) {
		ESP_LOGE(TAG, "BLE: invalid credentials message");
		return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
	}

	ESP_LOGI(TAG, "BLE: credentials received (SSID len=%u)", cred.ssid_len);
	if (credentials_rx_cb) {
		credentials_rx_cb(&cred);
	}
	return 0;
}

static int gatt_status_cb(uint16_t chn, uint16_t attr_handle,
			  struct ble_gatt_access_ctxt *ctxt, void *arg)
{
	uint8_t buf[5];
	uint8_t ip[4] = {0};

	wifi_prov_get_ip(ip);
	wifi_prov_msg_encode_status(wifi_prov_get_state(), ip, buf, sizeof(buf));

	int rc = os_mbuf_append(ctxt->om, buf, sizeof(buf));
	return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static int gatt_factory_reset_cb(uint16_t chn, uint16_t attr_handle,
				 struct ble_gatt_access_ctxt *ctxt, void *arg)
{
	struct os_mbuf *om = ctxt->om;
	uint8_t val;

	if (OS_MBUF_PKTLEN(om) < 1) {
		return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
	}

	ble_hs_mbuf_to_flat(om, &val, 1, NULL);
	if (val != 0xFF) {
		return BLE_ATT_ERR_REQ_NOT_SUPPORTED;
	}

	ESP_LOGI(TAG, "BLE: factory reset triggered");
	if (factory_reset_cb) {
		factory_reset_cb();
	}
	return 0;
}

/* --- GATT service definition --- */

static int gatt_noop_cb(uint16_t chn, uint16_t attr_handle,
			struct ble_gatt_access_ctxt *ctxt, void *arg)
{
	return 0;
}

static const struct ble_gatt_chr_def wifi_prov_chars[] = {
	{
		/* Scan Trigger: Write */
		.uuid = &chr_scan_trig_uuid.u,
		.access_cb = gatt_scan_trigger_cb,
		.flags = BLE_GATT_CHR_F_WRITE,
	},
	{
		/* Scan Results: Notify */
		.uuid = &chr_scan_res_uuid.u,
		.access_cb = gatt_noop_cb,
		.val_handle = &scan_res_attr_handle,
		.flags = BLE_GATT_CHR_F_NOTIFY,
	},
	{
		/* Credentials: Write */
		.uuid = &chr_cred_uuid.u,
		.access_cb = gatt_credentials_cb,
		.flags = BLE_GATT_CHR_F_WRITE,
	},
	{
		/* Status: Read + Notify */
		.uuid = &chr_status_uuid.u,
		.access_cb = gatt_status_cb,
		.val_handle = &status_attr_handle,
		.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
	},
	{
		/* Factory Reset: Write */
		.uuid = &chr_reset_uuid.u,
		.access_cb = gatt_factory_reset_cb,
		.flags = BLE_GATT_CHR_F_WRITE,
	},
	{ 0 },
};

static const struct ble_gatt_svc_def gatt_svcs[] = {
	{
		.type = BLE_GATT_SVC_TYPE_PRIMARY,
		.uuid = &svc_uuid.u,
		.characteristics = wifi_prov_chars,
	},
	{ 0 },
};

/* --- GAP event handler --- */

static int gap_event_handler(struct ble_gap_event *event, void *arg)
{
	switch (event->type) {
	case BLE_GAP_EVENT_CONNECT:
		if (event->connect.status == 0) {
			ESP_LOGI(TAG, "BLE connected");
			conn_handle = event->connect.conn_handle;
			is_connected = true;
		} else {
			ESP_LOGE(TAG, "BLE connection failed: %d",
				 event->connect.status);
			wifi_prov_ble_start_advertising();
		}
		break;

	case BLE_GAP_EVENT_DISCONNECT:
		ESP_LOGI(TAG, "BLE disconnected (reason 0x%02x)",
			 event->disconnect.reason);
		is_connected = false;
		wifi_prov_ble_start_advertising();
		break;

	case BLE_GAP_EVENT_SUBSCRIBE:
		ESP_LOGI(TAG, "Subscribe: attr=%u, notify=%u",
			 event->subscribe.attr_handle,
			 event->subscribe.cur_notify);
		break;

	case BLE_GAP_EVENT_MTU:
		ESP_LOGI(TAG, "MTU updated: %u", event->mtu.value);
		break;

	default:
		break;
	}
	return 0;
}

/* --- NimBLE host task --- */

static void ble_host_task(void *param)
{
	nimble_port_run();
	nimble_port_freertos_deinit();
}

static void on_sync(void)
{
	/* Use NRPA (non-resolvable private address) */
	ble_hs_id_infer_auto(0, &(uint8_t){0});

	ESP_LOGI(TAG, "BLE host synced, starting advertising");
	wifi_prov_ble_start_advertising();
}

static void on_reset(int reason)
{
	ESP_LOGW(TAG, "BLE host reset: %d", reason);
}

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
	int rc;

	rc = nimble_port_init();
	if (rc != ESP_OK) {
		ESP_LOGE(TAG, "nimble_port_init failed: %d", rc);
		return -1;
	}

	ble_hs_cfg.sync_cb = on_sync;
	ble_hs_cfg.reset_cb = on_reset;

	/* Set device name */
	ble_svc_gap_device_name_set(DEVICE_NAME);

	/* Register GATT services */
	ble_svc_gap_init();
	ble_svc_gatt_init();

	rc = ble_gatts_count_cfg(gatt_svcs);
	if (rc != 0) {
		ESP_LOGE(TAG, "ble_gatts_count_cfg failed: %d", rc);
		return -1;
	}

	rc = ble_gatts_add_svcs(gatt_svcs);
	if (rc != 0) {
		ESP_LOGE(TAG, "ble_gatts_add_svcs failed: %d", rc);
		return -1;
	}

	nimble_port_freertos_init(ble_host_task);

	ESP_LOGI(TAG, "BLE initialized");
	return 0;
}

int wifi_prov_ble_start_advertising(void)
{
	struct ble_gap_adv_params adv_params = {0};
	struct ble_hs_adv_fields fields = {0};
	struct ble_hs_adv_fields rsp_fields = {0};

	/* Advertising data: flags + name */
	fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
	fields.name = (uint8_t *)DEVICE_NAME;
	fields.name_len = strlen(DEVICE_NAME);
	fields.name_is_complete = 1;

	int rc = ble_gap_adv_set_fields(&fields);
	if (rc != 0) {
		ESP_LOGW(TAG, "ble_gap_adv_set_fields failed: %d", rc);
		return -1;
	}

	/* Scan response: service UUID */
	rsp_fields.uuids128 = (ble_uuid128_t[]){ svc_uuid };
	rsp_fields.num_uuids128 = 1;
	rsp_fields.uuids128_is_complete = 1;

	rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
	if (rc != 0) {
		ESP_LOGW(TAG, "ble_gap_adv_rsp_set_fields failed: %d", rc);
		return -1;
	}

	/* Connectable undirected advertising */
	adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
	adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

	rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
			       &adv_params, gap_event_handler, NULL);
	if (rc != 0 && rc != BLE_HS_EALREADY) {
		ESP_LOGW(TAG, "ble_gap_adv_start failed: %d", rc);
		return -1;
	}

	ESP_LOGI(TAG, "BLE advertising as \"%s\"", DEVICE_NAME);
	return 0;
}

int wifi_prov_ble_notify_scan_result(const struct wifi_prov_scan_result *result)
{
	uint8_t buf[64];
	int len;

	if (!is_connected) {
		return -1;
	}

	len = wifi_prov_msg_encode_scan_result(result, buf, sizeof(buf));
	if (len < 0) {
		return len;
	}

	struct os_mbuf *om = ble_hs_mbuf_from_flat(buf, len);
	if (om == NULL) {
		return -1;
	}

	return ble_gatts_notify_custom(conn_handle, scan_res_attr_handle, om);
}

int wifi_prov_ble_notify_status(enum wifi_prov_state state,
				const uint8_t ip[4])
{
	uint8_t buf[5];

	if (!is_connected) {
		return -1;
	}

	wifi_prov_msg_encode_status(state, ip, buf, sizeof(buf));

	struct os_mbuf *om = ble_hs_mbuf_from_flat(buf, sizeof(buf));
	if (om == NULL) {
		return -1;
	}

	return ble_gatts_notify_custom(conn_handle, status_attr_handle, om);
}
