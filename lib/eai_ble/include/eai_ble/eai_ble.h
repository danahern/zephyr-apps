/*
 * eai_ble — Portable BLE GATT abstraction
 *
 * Declarative service definition with backends for Zephyr BT, NimBLE,
 * and a POSIX stub for testing. Consumer describes a service once;
 * backend handles platform-specific registration.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef EAI_BLE_H
#define EAI_BLE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── UUID ────────────────────────────────────────────────────────────────── */

/** 128-bit UUID in little-endian (BLE wire format). */
typedef struct {
	uint8_t bytes[16];
} eai_ble_uuid128_t;

/**
 * Initialize a 128-bit UUID from standard format components.
 * Produces little-endian byte order (BLE wire format).
 *
 * Usage: EAI_BLE_UUID128_INIT(0xa0e4f2b0, 0x0001, 0x1000, 0x8000,
 *                              0x00805f9b34fbULL)
 */
#define EAI_BLE_UUID128_INIT(w32, w16a, w16b, w16c, w48)                      \
	{ { (uint8_t)((w48)&0xff),                                             \
	    (uint8_t)(((w48) >> 8) & 0xff),                                    \
	    (uint8_t)(((w48) >> 16) & 0xff),                                   \
	    (uint8_t)(((w48) >> 24) & 0xff),                                   \
	    (uint8_t)(((w48) >> 32) & 0xff),                                   \
	    (uint8_t)(((w48) >> 40) & 0xff),                                   \
	    (uint8_t)((w16c)&0xff),                                            \
	    (uint8_t)(((w16c) >> 8) & 0xff),                                   \
	    (uint8_t)((w16b)&0xff),                                            \
	    (uint8_t)(((w16b) >> 8) & 0xff),                                   \
	    (uint8_t)((w16a)&0xff),                                            \
	    (uint8_t)(((w16a) >> 8) & 0xff),                                   \
	    (uint8_t)((w32)&0xff),                                             \
	    (uint8_t)(((w32) >> 8) & 0xff),                                    \
	    (uint8_t)(((w32) >> 16) & 0xff),                                   \
	    (uint8_t)(((w32) >> 24) & 0xff) } }

/* ── Properties (BLE spec values) ────────────────────────────────────────── */

#define EAI_BLE_PROP_READ   0x02
#define EAI_BLE_PROP_WRITE  0x08
#define EAI_BLE_PROP_NOTIFY 0x10

/* ── Callbacks ───────────────────────────────────────────────────────────── */

typedef void (*eai_ble_write_cb_t)(uint8_t char_index, const uint8_t *data,
				   uint16_t len);
typedef int (*eai_ble_read_cb_t)(uint8_t char_index, uint8_t *buf,
				 uint16_t *len);
typedef void (*eai_ble_connect_cb_t)(void);
typedef void (*eai_ble_disconnect_cb_t)(void);

/* ── Characteristic definition ───────────────────────────────────────────── */

#define EAI_BLE_MAX_CHARS 8

struct eai_ble_char {
	eai_ble_uuid128_t uuid;
	uint8_t properties;
	eai_ble_write_cb_t on_write;  /* NULL if not writable */
	eai_ble_read_cb_t on_read;    /* NULL if not readable */
};

/* ── Service definition ──────────────────────────────────────────────────── */

struct eai_ble_service {
	eai_ble_uuid128_t uuid;
	const struct eai_ble_char *chars;
	uint8_t char_count; /* max EAI_BLE_MAX_CHARS */
};

/* ── Connection event callbacks ──────────────────────────────────────────── */

struct eai_ble_callbacks {
	eai_ble_connect_cb_t on_connect;
	eai_ble_disconnect_cb_t on_disconnect;
};

/* ── API ─────────────────────────────────────────────────────────────────── */

/**
 * Initialize the BLE subsystem.
 *
 * @param cbs  Connection event callbacks (may be NULL).
 * @return 0 on success, negative errno on error.
 */
int eai_ble_init(const struct eai_ble_callbacks *cbs);

/**
 * Register a GATT service. Must be called after init, before adv_start.
 * Only one service may be registered (v1 limitation).
 *
 * @param svc  Service definition with characteristics.
 * @return 0 on success, -EINVAL if svc is NULL or char_count > 8.
 */
int eai_ble_gatt_register(const struct eai_ble_service *svc);

/**
 * Start BLE advertising.
 *
 * @param device_name  Name to advertise (may be NULL for default).
 * @return 0 on success, negative errno on error.
 */
int eai_ble_adv_start(const char *device_name);

/**
 * Stop BLE advertising.
 *
 * @return 0 on success, negative errno on error.
 */
int eai_ble_adv_stop(void);

/**
 * Send a notification on a characteristic.
 *
 * @param char_index  Characteristic index (0..char_count-1).
 * @param data        Data to send.
 * @param len         Data length.
 * @return 0 on success, -ENOTCONN if no peer, -EINVAL if bad args.
 */
int eai_ble_notify(uint8_t char_index, const uint8_t *data, uint16_t len);

/**
 * Check if a BLE peer is connected.
 *
 * @return true if connected, false otherwise.
 */
bool eai_ble_is_connected(void);

/* ── Test helpers (POSIX backend only) ───────────────────────────────────── */

#if defined(CONFIG_EAI_BLE_BACKEND_POSIX) || defined(EAI_BLE_TEST)

/**
 * Simulate a BLE connection state change (POSIX stub only).
 * Fires connect/disconnect callbacks if registered.
 */
void eai_ble_test_set_connected(bool connected);

#endif

#ifdef __cplusplus
}
#endif

#endif /* EAI_BLE_H */
