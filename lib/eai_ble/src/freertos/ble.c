/*
 * eai_ble NimBLE backend (ESP-IDF / FreeRTOS)
 *
 * Builds ble_gatt_svc_def/ble_gatt_chr_def from eai_ble_service descriptor.
 * NimBLE handles CCC management automatically for notify characteristics.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include <eai_ble/eai_ble.h>
#include <string.h>

static const char *TAG = "eai_ble";

/* ── Static storage ─────────────────────────────────────────────────────── */

static const struct eai_ble_callbacks *user_cbs;
static const struct eai_ble_service *reg_svc;

static uint16_t conn_handle;
static bool connected;

/* NimBLE UUID storage (service + chars) */
static ble_uuid128_t uuid_store[1 + EAI_BLE_MAX_CHARS];

/* Characteristic definitions (+1 for null terminator) */
static struct ble_gatt_chr_def chr_defs[EAI_BLE_MAX_CHARS + 1];

/* Service definition (1 service + null terminator) */
static struct ble_gatt_svc_def svc_defs[2];

/* Value handles for notify-capable characteristics */
static uint16_t val_handles[EAI_BLE_MAX_CHARS];

/* Device name for advertising */
static char adv_name[30];

/* ── UUID conversion ────────────────────────────────────────────────────── */

static void uuid_to_nimble(const eai_ble_uuid128_t *src, ble_uuid128_t *dst)
{
	dst->u.type = BLE_UUID_TYPE_128;
	memcpy(dst->value, src->bytes, 16);
}

/* ── Find char_index from attr_handle ───────────────────────────────────── */

static int find_char_index(uint16_t attr_handle)
{
	if (!reg_svc) {
		return -1;
	}

	for (uint8_t i = 0; i < reg_svc->char_count; i++) {
		if (val_handles[i] == attr_handle) {
			return i;
		}
	}
	return -1;
}

/* ── Unified GATT access callback ───────────────────────────────────────── */

static int gatt_access_cb(uint16_t chn, uint16_t attr_handle,
			  struct ble_gatt_access_ctxt *ctxt, void *arg)
{
	uint8_t char_idx = (uint8_t)(uintptr_t)arg;

	if (!reg_svc || char_idx >= reg_svc->char_count) {
		return BLE_ATT_ERR_UNLIKELY;
	}

	const struct eai_ble_char *c = &reg_svc->chars[char_idx];

	switch (ctxt->op) {
	case BLE_GATT_ACCESS_OP_WRITE_CHR: {
		struct os_mbuf *om = ctxt->om;
		uint16_t len = OS_MBUF_PKTLEN(om);
		uint8_t buf[244];

		if (len > sizeof(buf)) {
			return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
		}
		if (ble_hs_mbuf_to_flat(om, buf, len, NULL) != 0) {
			return BLE_ATT_ERR_UNLIKELY;
		}
		if (c->on_write) {
			c->on_write(char_idx, buf, len);
		}
		return 0;
	}

	case BLE_GATT_ACCESS_OP_READ_CHR: {
		if (!c->on_read) {
			return BLE_ATT_ERR_READ_NOT_PERMITTED;
		}
		uint8_t buf[244];
		uint16_t len = sizeof(buf);
		int rc = c->on_read(char_idx, buf, &len);

		if (rc < 0) {
			return BLE_ATT_ERR_UNLIKELY;
		}
		rc = os_mbuf_append(ctxt->om, buf, len);
		return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
	}

	default:
		return BLE_ATT_ERR_UNLIKELY;
	}
}

/* ── Build NimBLE service from descriptor ───────────────────────────────── */

static void build_svc_defs(const struct eai_ble_service *svc)
{
	/* Service UUID */
	uuid_to_nimble(&svc->uuid, &uuid_store[0]);

	/* Characteristics */
	for (uint8_t i = 0; i < svc->char_count; i++) {
		const struct eai_ble_char *c = &svc->chars[i];

		uuid_to_nimble(&c->uuid, &uuid_store[1 + i]);

		ble_gatt_chr_flags flags = 0;

		if (c->properties & EAI_BLE_PROP_READ) {
			flags |= BLE_GATT_CHR_F_READ;
		}
		if (c->properties & EAI_BLE_PROP_WRITE) {
			flags |= BLE_GATT_CHR_F_WRITE;
		}
		if (c->properties & EAI_BLE_PROP_NOTIFY) {
			flags |= BLE_GATT_CHR_F_NOTIFY;
		}

		chr_defs[i] = (struct ble_gatt_chr_def){
			.uuid = &uuid_store[1 + i].u,
			.access_cb = gatt_access_cb,
			.arg = (void *)(uintptr_t)i,
			.val_handle = &val_handles[i],
			.flags = flags,
		};
	}

	/* Null terminator */
	memset(&chr_defs[svc->char_count], 0, sizeof(chr_defs[0]));

	/* Service definition */
	svc_defs[0] = (struct ble_gatt_svc_def){
		.type = BLE_GATT_SVC_TYPE_PRIMARY,
		.uuid = &uuid_store[0].u,
		.characteristics = chr_defs,
	};
	memset(&svc_defs[1], 0, sizeof(svc_defs[0]));
}

/* ── GAP event handler ──────────────────────────────────────────────────── */

static int gap_event_handler(struct ble_gap_event *event, void *arg);

static int start_advertising(void)
{
	struct ble_gap_adv_params adv_params = {0};
	struct ble_hs_adv_fields fields = {0};
	struct ble_hs_adv_fields rsp_fields = {0};
	int rc;

	/* Advertising data: flags + name */
	fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
	fields.name = (uint8_t *)adv_name;
	fields.name_len = strlen(adv_name);
	fields.name_is_complete = 1;

	rc = ble_gap_adv_set_fields(&fields);
	if (rc != 0) {
		ESP_LOGW(TAG, "ble_gap_adv_set_fields failed: %d", rc);
		return -1;
	}

	/* Scan response: service UUID */
	rsp_fields.uuids128 = &uuid_store[0];
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

	ESP_LOGI(TAG, "Advertising as \"%s\"", adv_name);
	return 0;
}

static int gap_event_handler(struct ble_gap_event *event, void *arg)
{
	switch (event->type) {
	case BLE_GAP_EVENT_CONNECT:
		if (event->connect.status == 0) {
			ESP_LOGI(TAG, "BLE connected");
			conn_handle = event->connect.conn_handle;
			connected = true;
			if (user_cbs && user_cbs->on_connect) {
				user_cbs->on_connect();
			}
		} else {
			ESP_LOGE(TAG, "BLE connection failed: %d",
				 event->connect.status);
			start_advertising();
		}
		break;

	case BLE_GAP_EVENT_DISCONNECT:
		ESP_LOGI(TAG, "BLE disconnected (reason 0x%02x)",
			 event->disconnect.reason);
		connected = false;
		if (user_cbs && user_cbs->on_disconnect) {
			user_cbs->on_disconnect();
		}
		/* Auto-restart advertising */
		start_advertising();
		break;

	case BLE_GAP_EVENT_MTU:
		ESP_LOGI(TAG, "MTU updated: %u", event->mtu.value);
		break;

	default:
		break;
	}
	return 0;
}

/* ── NimBLE host task ───────────────────────────────────────────────────── */

static void ble_host_task(void *param)
{
	nimble_port_run();
	nimble_port_freertos_deinit();
}

static void on_sync(void)
{
	ble_hs_id_infer_auto(0, &(uint8_t){0});
	ESP_LOGI(TAG, "BLE host synced");
}

static void on_reset(int reason)
{
	ESP_LOGW(TAG, "BLE host reset: %d", reason);
}

/* ── Public API ─────────────────────────────────────────────────────────── */

int eai_ble_init(const struct eai_ble_callbacks *cbs)
{
	int rc;

	user_cbs = cbs;
	connected = false;

	rc = nimble_port_init();
	if (rc != ESP_OK) {
		ESP_LOGE(TAG, "nimble_port_init failed: %d", rc);
		return -1;
	}

	ble_hs_cfg.sync_cb = on_sync;
	ble_hs_cfg.reset_cb = on_reset;

	ble_svc_gap_init();
	ble_svc_gatt_init();

	ESP_LOGI(TAG, "BLE initialized");
	return 0;
}

int eai_ble_gatt_register(const struct eai_ble_service *svc)
{
	int rc;

	if (!svc || svc->char_count > EAI_BLE_MAX_CHARS) {
		return -1;
	}

	reg_svc = svc;
	build_svc_defs(svc);

	rc = ble_gatts_count_cfg(svc_defs);
	if (rc != 0) {
		ESP_LOGE(TAG, "ble_gatts_count_cfg failed: %d", rc);
		return -1;
	}

	rc = ble_gatts_add_svcs(svc_defs);
	if (rc != 0) {
		ESP_LOGE(TAG, "ble_gatts_add_svcs failed: %d", rc);
		return -1;
	}

	/* Start NimBLE host task */
	nimble_port_freertos_init(ble_host_task);

	ESP_LOGI(TAG, "GATT service registered (%u chars)", svc->char_count);
	return 0;
}

int eai_ble_adv_start(const char *device_name)
{
	const char *name = device_name ? device_name : "eai_ble";
	size_t len = strlen(name);

	if (len > sizeof(adv_name) - 1) {
		len = sizeof(adv_name) - 1;
	}
	memcpy(adv_name, name, len);
	adv_name[len] = '\0';

	ble_svc_gap_device_name_set(adv_name);
	return start_advertising();
}

int eai_ble_adv_stop(void)
{
	return ble_gap_adv_stop();
}

int eai_ble_notify(uint8_t char_index, const uint8_t *data, uint16_t len)
{
	struct os_mbuf *om;

	if (!reg_svc || char_index >= reg_svc->char_count) {
		return -1;
	}
	if (!data || len == 0) {
		return -1;
	}
	if (!connected) {
		return -1;
	}

	om = ble_hs_mbuf_from_flat(data, len);
	if (!om) {
		return -1;
	}

	return ble_gatts_notify_custom(conn_handle, val_handles[char_index],
				       om);
}

bool eai_ble_is_connected(void)
{
	return connected;
}
