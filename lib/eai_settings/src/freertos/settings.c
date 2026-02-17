/*
 * eai_settings FreeRTOS/ESP-IDF backend — NVS-based key-value store.
 *
 * Key format: "namespace/key" → NVS namespace + NVS key.
 * NVS namespace and key are each limited to 15 characters.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <eai_settings/eai_settings.h>

#include "nvs_flash.h"
#include "nvs.h"
#include "esp_err.h"
#include <errno.h>
#include <string.h>

#define NVS_MAX_NAME_LEN 15

/**
 * Parse "namespace/key" into NVS namespace and key.
 * NVS limits both to 15 chars.
 */
static int parse_key(const char *key, char *ns, size_t ns_len,
		     char *name, size_t name_len)
{
	const char *sep = strchr(key, '/');

	if (!sep || sep == key || *(sep + 1) == '\0') {
		return -EINVAL;
	}

	size_t ns_sz = (size_t)(sep - key);
	size_t name_sz = strlen(sep + 1);

	if (ns_sz >= ns_len || name_sz >= name_len) {
		return -EINVAL;
	}

	if (ns_sz > NVS_MAX_NAME_LEN || name_sz > NVS_MAX_NAME_LEN) {
		return -EINVAL;
	}

	memcpy(ns, key, ns_sz);
	ns[ns_sz] = '\0';
	memcpy(name, sep + 1, name_sz);
	name[name_sz] = '\0';

	return 0;
}

int eai_settings_init(void)
{
	esp_err_t err = nvs_flash_init();

	if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
	    err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		/* NVS partition corrupt — erase and retry */
		esp_err_t erase_err = nvs_flash_erase();

		if (erase_err != ESP_OK) {
			return -EIO;
		}
		err = nvs_flash_init();
	}

	return (err == ESP_OK) ? 0 : -EIO;
}

int eai_settings_set(const char *key, const void *data, size_t len)
{
	if (!key || !data || len == 0) {
		return -EINVAL;
	}

	char ns[NVS_MAX_NAME_LEN + 1];
	char name[NVS_MAX_NAME_LEN + 1];
	int ret = parse_key(key, ns, sizeof(ns), name, sizeof(name));

	if (ret) {
		return ret;
	}

	nvs_handle_t handle;
	esp_err_t err = nvs_open(ns, NVS_READWRITE, &handle);

	if (err != ESP_OK) {
		return -EIO;
	}

	err = nvs_set_blob(handle, name, data, len);
	if (err != ESP_OK) {
		nvs_close(handle);
		return -EIO;
	}

	err = nvs_commit(handle);
	nvs_close(handle);

	return (err == ESP_OK) ? 0 : -EIO;
}

int eai_settings_get(const char *key, void *buf, size_t buf_len,
		     size_t *actual_len)
{
	if (!key) {
		return -EINVAL;
	}

	char ns[NVS_MAX_NAME_LEN + 1];
	char name[NVS_MAX_NAME_LEN + 1];
	int ret = parse_key(key, ns, sizeof(ns), name, sizeof(name));

	if (ret) {
		return ret;
	}

	nvs_handle_t handle;
	esp_err_t err = nvs_open(ns, NVS_READONLY, &handle);

	if (err != ESP_OK) {
		return (err == ESP_ERR_NVS_NOT_FOUND) ? -ENOENT : -EIO;
	}

	/* First get the actual size */
	size_t stored_len = 0;

	err = nvs_get_blob(handle, name, NULL, &stored_len);
	if (err != ESP_OK) {
		nvs_close(handle);
		return (err == ESP_ERR_NVS_NOT_FOUND) ? -ENOENT : -EIO;
	}

	if (actual_len) {
		*actual_len = stored_len;
	}

	/* Read up to buf_len bytes */
	if (buf && buf_len > 0) {
		size_t to_read = (stored_len < buf_len) ? stored_len : buf_len;

		err = nvs_get_blob(handle, name, buf, &to_read);
		if (err != ESP_OK) {
			nvs_close(handle);
			return -EIO;
		}
	}

	nvs_close(handle);
	return 0;
}

int eai_settings_delete(const char *key)
{
	if (!key) {
		return -EINVAL;
	}

	char ns[NVS_MAX_NAME_LEN + 1];
	char name[NVS_MAX_NAME_LEN + 1];
	int ret = parse_key(key, ns, sizeof(ns), name, sizeof(name));

	if (ret) {
		return ret;
	}

	nvs_handle_t handle;
	esp_err_t err = nvs_open(ns, NVS_READWRITE, &handle);

	if (err != ESP_OK) {
		return (err == ESP_ERR_NVS_NOT_FOUND) ? -ENOENT : -EIO;
	}

	err = nvs_erase_key(handle, name);
	if (err != ESP_OK) {
		nvs_close(handle);
		return (err == ESP_ERR_NVS_NOT_FOUND) ? -ENOENT : -EIO;
	}

	nvs_commit(handle);
	nvs_close(handle);
	return 0;
}

bool eai_settings_exists(const char *key)
{
	if (!key) {
		return false;
	}

	char ns[NVS_MAX_NAME_LEN + 1];
	char name[NVS_MAX_NAME_LEN + 1];

	if (parse_key(key, ns, sizeof(ns), name, sizeof(name)) != 0) {
		return false;
	}

	nvs_handle_t handle;
	esp_err_t err = nvs_open(ns, NVS_READONLY, &handle);

	if (err != ESP_OK) {
		return false;
	}

	size_t len = 0;

	err = nvs_get_blob(handle, name, NULL, &len);
	nvs_close(handle);

	return (err == ESP_OK);
}
