/*
 * eai_wifi — Portable WiFi Connection Manager
 *
 * Scan, connect, disconnect with backends for Zephyr net_mgmt,
 * ESP-IDF esp_wifi, and a POSIX stub for testing.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef EAI_WIFI_H
#define EAI_WIFI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Constants ───────────────────────────────────────────────────────────── */

#define EAI_WIFI_SSID_MAX_LEN 32
#define EAI_WIFI_PSK_MAX_LEN  64

/* ── Enums ───────────────────────────────────────────────────────────────── */

enum eai_wifi_security {
	EAI_WIFI_SEC_OPEN,
	EAI_WIFI_SEC_WPA_PSK,
	EAI_WIFI_SEC_WPA2_PSK,
	EAI_WIFI_SEC_WPA3_SAE,
};

enum eai_wifi_state {
	EAI_WIFI_STATE_DISCONNECTED,
	EAI_WIFI_STATE_SCANNING,
	EAI_WIFI_STATE_CONNECTING,
	EAI_WIFI_STATE_CONNECTED,
};

enum eai_wifi_event {
	EAI_WIFI_EVT_CONNECTED,
	EAI_WIFI_EVT_DISCONNECTED,
	EAI_WIFI_EVT_CONNECT_FAILED,
};

/* ── Types ───────────────────────────────────────────────────────────────── */

struct eai_wifi_scan_result {
	uint8_t ssid[EAI_WIFI_SSID_MAX_LEN];
	uint8_t ssid_len;
	int8_t rssi;
	enum eai_wifi_security security;
	uint8_t channel;
};

typedef void (*eai_wifi_scan_result_cb_t)(const struct eai_wifi_scan_result *result);
typedef void (*eai_wifi_scan_done_cb_t)(int status);
typedef void (*eai_wifi_event_cb_t)(enum eai_wifi_event event);

/* ── API ─────────────────────────────────────────────────────────────────── */

/**
 * Initialize the WiFi subsystem.
 *
 * @return 0 on success, negative errno on error.
 */
int eai_wifi_init(void);

/**
 * Register an event callback for connect/disconnect/failure events.
 *
 * @param cb  Event callback (NULL to unregister).
 */
void eai_wifi_set_event_callback(eai_wifi_event_cb_t cb);

/**
 * Start an AP scan. Results delivered via on_result per AP, then on_done.
 *
 * @param on_result  Called once per discovered AP.
 * @param on_done    Called when scan completes (status 0 = success).
 * @return 0 on success, -EINVAL if on_result is NULL, negative errno on error.
 */
int eai_wifi_scan(eai_wifi_scan_result_cb_t on_result,
		  eai_wifi_scan_done_cb_t on_done);

/**
 * Connect to an AP. EVT_CONNECTED fires after DHCP completes.
 *
 * @param ssid      SSID bytes (not null-terminated).
 * @param ssid_len  SSID length (1..EAI_WIFI_SSID_MAX_LEN).
 * @param psk       PSK bytes (not null-terminated, NULL for open networks).
 * @param psk_len   PSK length (0..EAI_WIFI_PSK_MAX_LEN).
 * @param sec       Security type.
 * @return 0 on success, -EINVAL if ssid is NULL or ssid_len is 0.
 */
int eai_wifi_connect(const uint8_t *ssid, uint8_t ssid_len,
		     const uint8_t *psk, uint8_t psk_len,
		     enum eai_wifi_security sec);

/**
 * Disconnect from the current AP.
 *
 * @return 0 on success, negative errno on error.
 */
int eai_wifi_disconnect(void);

/**
 * Get the current WiFi state.
 *
 * @return Current state.
 */
enum eai_wifi_state eai_wifi_get_state(void);

/**
 * Get the current IPv4 address.
 *
 * @param ip  Output buffer for 4-byte IPv4 address.
 * @return 0 on success, -ENOTCONN if not connected.
 */
int eai_wifi_get_ip(uint8_t ip[4]);

/* ── Test helpers (POSIX backend only) ───────────────────────────────────── */

#if defined(CONFIG_EAI_WIFI_BACKEND_POSIX) || defined(EAI_WIFI_TEST)

/** Reset all state (call in setUp). */
void eai_wifi_test_reset(void);

/** Forward a scan result to the stored on_result callback. */
void eai_wifi_test_inject_scan_result(const struct eai_wifi_scan_result *result);

/** Call scan_done_cb with status, reset scanning state. */
void eai_wifi_test_complete_scan(int status);

/** Set state=CONNECTED, store IP, fire EVT_CONNECTED. */
void eai_wifi_test_set_connected(const uint8_t ip[4]);

/** Set state=DISCONNECTED, fire EVT_DISCONNECTED. */
void eai_wifi_test_set_disconnected(void);

/** Set state=DISCONNECTED, fire EVT_CONNECT_FAILED. */
void eai_wifi_test_set_connect_failed(void);

#endif

#ifdef __cplusplus
}
#endif

#endif /* EAI_WIFI_H */
