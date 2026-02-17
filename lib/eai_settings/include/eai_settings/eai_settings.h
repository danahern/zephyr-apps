/*
 * eai_settings — Portable key-value store
 *
 * Provides a simple set/get/delete/exists API with backends for
 * Zephyr Settings, ESP-IDF NVS, and POSIX file-based storage.
 *
 * Key format: "namespace/key" — first '/' separates namespace from key name.
 * Example: "wifi_prov/ssid"
 *
 * Returns: 0 on success, -EINVAL, -ENOENT, -EIO on error.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef EAI_SETTINGS_H
#define EAI_SETTINGS_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the settings subsystem.
 *
 * Must be called before any other eai_settings function.
 *
 * @return 0 on success, negative errno on error.
 */
int eai_settings_init(void);

/**
 * @brief Store a value.
 *
 * @param key   Key in "namespace/key" format.
 * @param data  Pointer to data to store.
 * @param len   Length of data in bytes (must be > 0).
 * @return 0 on success, -EINVAL if key/data is NULL or len is 0.
 */
int eai_settings_set(const char *key, const void *data, size_t len);

/**
 * @brief Read a stored value.
 *
 * If buf_len < actual data size, the first buf_len bytes are copied and
 * actual_len reports the full data size (caller can detect truncation).
 *
 * @param key        Key in "namespace/key" format.
 * @param buf        Buffer to read data into.
 * @param buf_len    Size of buffer.
 * @param actual_len If non-NULL, receives the actual data size.
 * @return 0 on success, -ENOENT if key not found, -EINVAL if key is NULL.
 */
int eai_settings_get(const char *key, void *buf, size_t buf_len,
		     size_t *actual_len);

/**
 * @brief Delete a stored value.
 *
 * @param key  Key in "namespace/key" format.
 * @return 0 on success, -ENOENT if key not found, -EINVAL if key is NULL.
 */
int eai_settings_delete(const char *key);

/**
 * @brief Check if a key exists.
 *
 * @param key  Key in "namespace/key" format.
 * @return true if key exists, false otherwise.
 */
bool eai_settings_exists(const char *key);

#ifdef __cplusplus
}
#endif

#endif /* EAI_SETTINGS_H */
