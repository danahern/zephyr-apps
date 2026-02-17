/*
 * eai_settings Zephyr backend — uses Zephyr Settings subsystem.
 *
 * All keys are stored under the "eai" prefix:
 *   eai_settings_set("wifi_prov/ssid", ...) → settings_save_one("eai/wifi_prov/ssid", ...)
 *
 * get/exists use settings_load_subtree("eai") with a transient capture context
 * that the handler callback uses to match the target key and copy data.
 *
 * Thread safety: K_MUTEX protects the static load context during get/exists.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <eai_settings/eai_settings.h>

#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include <string.h>

#define EAI_PREFIX "eai"
#define MAX_FULL_KEY_LEN 128

static K_MUTEX_DEFINE(load_mutex);

/* Transient context for settings_load_subtree callback */
static struct {
	const char *target_key; /* key to match (e.g., "wifi_prov/ssid") */
	void *buf;
	size_t buf_len;
	size_t actual_len;
	bool found;
} load_ctx;

static int eai_settings_handler_set(const char *name, size_t len,
				    settings_read_cb read_cb, void *cb_arg)
{
	if (!load_ctx.target_key) {
		return 0;
	}

	if (strcmp(name, load_ctx.target_key) != 0) {
		return 0;
	}

	load_ctx.actual_len = len;
	load_ctx.found = true;

	if (load_ctx.buf && load_ctx.buf_len > 0) {
		size_t to_read = (len < load_ctx.buf_len) ? len : load_ctx.buf_len;
		int rc = read_cb(cb_arg, load_ctx.buf, to_read);

		if (rc < 0) {
			load_ctx.found = false;
			return rc;
		}
	}

	return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(eai, EAI_PREFIX, NULL, eai_settings_handler_set,
			       NULL, NULL);

static int build_full_key(const char *key, char *buf, size_t buf_len)
{
	int n = snprintf(buf, buf_len, "%s/%s", EAI_PREFIX, key);

	if (n < 0 || (size_t)n >= buf_len) {
		return -EINVAL;
	}
	return 0;
}

int eai_settings_init(void)
{
	return settings_subsys_init();
}

int eai_settings_set(const char *key, const void *data, size_t len)
{
	if (!key || !data || len == 0) {
		return -EINVAL;
	}

	char full_key[MAX_FULL_KEY_LEN];
	int ret = build_full_key(key, full_key, sizeof(full_key));

	if (ret) {
		return ret;
	}

	return settings_save_one(full_key, data, len);
}

int eai_settings_get(const char *key, void *buf, size_t buf_len,
		     size_t *actual_len)
{
	if (!key) {
		return -EINVAL;
	}

	k_mutex_lock(&load_mutex, K_FOREVER);

	load_ctx.target_key = key;
	load_ctx.buf = buf;
	load_ctx.buf_len = buf_len;
	load_ctx.actual_len = 0;
	load_ctx.found = false;

	int ret = settings_load_subtree(EAI_PREFIX);

	if (ret) {
		k_mutex_unlock(&load_mutex);
		return -EIO;
	}

	if (!load_ctx.found) {
		load_ctx.target_key = NULL;
		k_mutex_unlock(&load_mutex);
		return -ENOENT;
	}

	if (actual_len) {
		*actual_len = load_ctx.actual_len;
	}

	load_ctx.target_key = NULL;
	k_mutex_unlock(&load_mutex);
	return 0;
}

int eai_settings_delete(const char *key)
{
	if (!key) {
		return -EINVAL;
	}

	/* Check existence first */
	if (!eai_settings_exists(key)) {
		return -ENOENT;
	}

	char full_key[MAX_FULL_KEY_LEN];
	int ret = build_full_key(key, full_key, sizeof(full_key));

	if (ret) {
		return ret;
	}

	return settings_delete(full_key);
}

bool eai_settings_exists(const char *key)
{
	if (!key) {
		return false;
	}

	k_mutex_lock(&load_mutex, K_FOREVER);

	load_ctx.target_key = key;
	load_ctx.buf = NULL;
	load_ctx.buf_len = 0;
	load_ctx.actual_len = 0;
	load_ctx.found = false;

	settings_load_subtree(EAI_PREFIX);

	bool found = load_ctx.found;

	load_ctx.target_key = NULL;
	k_mutex_unlock(&load_mutex);

	return found;
}
