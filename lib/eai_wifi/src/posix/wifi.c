/*
 * eai_wifi POSIX stub backend
 *
 * Tracks state for API contract testing. No actual WiFi operations.
 * Use eai_wifi_test_*() helpers to simulate events.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <eai_wifi/eai_wifi.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>

static bool initialized;
static enum eai_wifi_state current_state;
static eai_wifi_event_cb_t event_cb;
static eai_wifi_scan_result_cb_t scan_result_cb;
static eai_wifi_scan_done_cb_t scan_done_cb;
static uint8_t stored_ip[4];

int eai_wifi_init(void)
{
	initialized = true;
	current_state = EAI_WIFI_STATE_DISCONNECTED;
	event_cb = NULL;
	scan_result_cb = NULL;
	scan_done_cb = NULL;
	memset(stored_ip, 0, sizeof(stored_ip));
	return 0;
}

void eai_wifi_set_event_callback(eai_wifi_event_cb_t cb)
{
	event_cb = cb;
}

int eai_wifi_scan(eai_wifi_scan_result_cb_t on_result,
		  eai_wifi_scan_done_cb_t on_done)
{
	if (!initialized) {
		return -EINVAL;
	}
	if (!on_result) {
		return -EINVAL;
	}

	scan_result_cb = on_result;
	scan_done_cb = on_done;
	current_state = EAI_WIFI_STATE_SCANNING;
	return 0;
}

int eai_wifi_connect(const uint8_t *ssid, uint8_t ssid_len,
		     const uint8_t *psk, uint8_t psk_len,
		     enum eai_wifi_security sec)
{
	(void)psk;
	(void)psk_len;
	(void)sec;

	if (!initialized) {
		return -EINVAL;
	}
	if (!ssid || ssid_len == 0) {
		return -EINVAL;
	}

	current_state = EAI_WIFI_STATE_CONNECTING;
	return 0;
}

int eai_wifi_disconnect(void)
{
	if (!initialized) {
		return -EINVAL;
	}

	current_state = EAI_WIFI_STATE_DISCONNECTED;
	memset(stored_ip, 0, sizeof(stored_ip));
	return 0;
}

enum eai_wifi_state eai_wifi_get_state(void)
{
	return current_state;
}

int eai_wifi_get_ip(uint8_t ip[4])
{
	if (current_state != EAI_WIFI_STATE_CONNECTED) {
		return -ENOTCONN;
	}

	memcpy(ip, stored_ip, 4);
	return 0;
}

/* ── Test helpers ────────────────────────────────────────────────────────── */

void eai_wifi_test_reset(void)
{
	initialized = false;
	current_state = EAI_WIFI_STATE_DISCONNECTED;
	event_cb = NULL;
	scan_result_cb = NULL;
	scan_done_cb = NULL;
	memset(stored_ip, 0, sizeof(stored_ip));
}

void eai_wifi_test_inject_scan_result(const struct eai_wifi_scan_result *result)
{
	if (scan_result_cb && result) {
		scan_result_cb(result);
	}
}

void eai_wifi_test_complete_scan(int status)
{
	eai_wifi_scan_done_cb_t cb = scan_done_cb;

	scan_result_cb = NULL;
	scan_done_cb = NULL;

	if (current_state == EAI_WIFI_STATE_SCANNING) {
		current_state = EAI_WIFI_STATE_DISCONNECTED;
	}

	if (cb) {
		cb(status);
	}
}

void eai_wifi_test_set_connected(const uint8_t ip[4])
{
	current_state = EAI_WIFI_STATE_CONNECTED;
	memcpy(stored_ip, ip, 4);

	if (event_cb) {
		event_cb(EAI_WIFI_EVT_CONNECTED);
	}
}

void eai_wifi_test_set_disconnected(void)
{
	current_state = EAI_WIFI_STATE_DISCONNECTED;
	memset(stored_ip, 0, sizeof(stored_ip));

	if (event_cb) {
		event_cb(EAI_WIFI_EVT_DISCONNECTED);
	}
}

void eai_wifi_test_set_connect_failed(void)
{
	current_state = EAI_WIFI_STATE_DISCONNECTED;

	if (event_cb) {
		event_cb(EAI_WIFI_EVT_CONNECT_FAILED);
	}
}
