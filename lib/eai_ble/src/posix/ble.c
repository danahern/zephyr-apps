/*
 * eai_ble POSIX stub backend
 *
 * Tracks state for API contract testing. No actual BLE operations.
 * Use eai_ble_test_set_connected() to simulate connection events.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <eai_ble/eai_ble.h>
#include <errno.h>
#include <stddef.h>

static bool initialized;
static bool registered;
static bool advertising;
static bool connected;

static const struct eai_ble_callbacks *user_cbs;
static const struct eai_ble_service *reg_svc;

int eai_ble_init(const struct eai_ble_callbacks *cbs)
{
	user_cbs = cbs;
	initialized = true;
	registered = false;
	advertising = false;
	connected = false;
	return 0;
}

int eai_ble_gatt_register(const struct eai_ble_service *svc)
{
	if (!initialized) {
		return -EINVAL;
	}
	if (!svc) {
		return -EINVAL;
	}
	if (svc->char_count > EAI_BLE_MAX_CHARS) {
		return -EINVAL;
	}

	reg_svc = svc;
	registered = true;
	return 0;
}

int eai_ble_adv_start(const char *device_name)
{
	(void)device_name;

	if (!initialized || !registered) {
		return -EINVAL;
	}

	advertising = true;
	return 0;
}

int eai_ble_adv_stop(void)
{
	if (!initialized) {
		return -EINVAL;
	}

	advertising = false;
	return 0;
}

int eai_ble_notify(uint8_t char_index, const uint8_t *data, uint16_t len)
{
	if (!initialized || !registered) {
		return -EINVAL;
	}
	if (!data || len == 0) {
		return -EINVAL;
	}
	if (!connected) {
		return -ENOTCONN;
	}
	if (reg_svc && char_index >= reg_svc->char_count) {
		return -EINVAL;
	}

	/* Stub: data accepted but not sent */
	return 0;
}

bool eai_ble_is_connected(void)
{
	return connected;
}

void eai_ble_test_set_connected(bool state)
{
	bool was_connected = connected;

	connected = state;

	if (user_cbs) {
		if (state && !was_connected && user_cbs->on_connect) {
			user_cbs->on_connect();
		} else if (!state && was_connected && user_cbs->on_disconnect) {
			user_cbs->on_disconnect();
		}
	}
}
