/*
 * eai_settings POSIX backend — file-based key-value store.
 *
 * Storage layout: <base_path>/<namespace>/<key> (raw bytes)
 *
 * Thread safety: pthread_mutex wrapping all operations.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <eai_settings/eai_settings.h>

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef EAI_SETTINGS_BASE_PATH
#define EAI_SETTINGS_BASE_PATH "/tmp/eai_settings"
#endif

#define MAX_PATH_LEN 256
#define MAX_NS_LEN   64
#define MAX_KEY_LEN   64

static pthread_mutex_t settings_lock = PTHREAD_MUTEX_INITIALIZER;
static bool initialized;

/**
 * Parse "namespace/key" into separate strings.
 * Returns 0 on success, -EINVAL if format is invalid.
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

	memcpy(ns, key, ns_sz);
	ns[ns_sz] = '\0';
	memcpy(name, sep + 1, name_sz);
	name[name_sz] = '\0';

	return 0;
}

/** Build full file path: <base>/<ns>/<name> */
static int build_path(const char *key, char *path, size_t path_len)
{
	char ns[MAX_NS_LEN];
	char name[MAX_KEY_LEN];
	int ret = parse_key(key, ns, sizeof(ns), name, sizeof(name));

	if (ret) {
		return ret;
	}

	int n = snprintf(path, path_len, "%s/%s/%s",
			 EAI_SETTINGS_BASE_PATH, ns, name);
	if (n < 0 || (size_t)n >= path_len) {
		return -EINVAL;
	}

	return 0;
}

/** Ensure directory exists (mkdir -p for one level) */
static int ensure_ns_dir(const char *key)
{
	char ns[MAX_NS_LEN];
	char name[MAX_KEY_LEN];
	int ret = parse_key(key, ns, sizeof(ns), name, sizeof(name));

	if (ret) {
		return ret;
	}

	char dir[MAX_PATH_LEN];
	snprintf(dir, sizeof(dir), "%s/%s", EAI_SETTINGS_BASE_PATH, ns);

	if (mkdir(dir, 0755) != 0 && errno != EEXIST) {
		return -EIO;
	}

	return 0;
}

int eai_settings_init(void)
{
	pthread_mutex_lock(&settings_lock);

	if (mkdir(EAI_SETTINGS_BASE_PATH, 0755) != 0 && errno != EEXIST) {
		pthread_mutex_unlock(&settings_lock);
		return -EIO;
	}

	initialized = true;
	pthread_mutex_unlock(&settings_lock);
	return 0;
}

int eai_settings_set(const char *key, const void *data, size_t len)
{
	if (!key || !data || len == 0) {
		return -EINVAL;
	}

	char path[MAX_PATH_LEN];
	int ret = build_path(key, path, sizeof(path));

	if (ret) {
		return ret;
	}

	pthread_mutex_lock(&settings_lock);

	ret = ensure_ns_dir(key);
	if (ret) {
		pthread_mutex_unlock(&settings_lock);
		return ret;
	}

	FILE *f = fopen(path, "wb");
	if (!f) {
		pthread_mutex_unlock(&settings_lock);
		return -EIO;
	}

	size_t written = fwrite(data, 1, len, f);
	fclose(f);

	pthread_mutex_unlock(&settings_lock);
	return (written == len) ? 0 : -EIO;
}

int eai_settings_get(const char *key, void *buf, size_t buf_len,
		     size_t *actual_len)
{
	if (!key) {
		return -EINVAL;
	}

	char path[MAX_PATH_LEN];
	int ret = build_path(key, path, sizeof(path));

	if (ret) {
		return ret;
	}

	pthread_mutex_lock(&settings_lock);

	FILE *f = fopen(path, "rb");
	if (!f) {
		pthread_mutex_unlock(&settings_lock);
		return -ENOENT;
	}

	/* Get file size */
	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);

	if (size < 0) {
		fclose(f);
		pthread_mutex_unlock(&settings_lock);
		return -EIO;
	}

	if (actual_len) {
		*actual_len = (size_t)size;
	}

	/* Read up to buf_len bytes */
	if (buf && buf_len > 0) {
		size_t to_read = ((size_t)size < buf_len) ? (size_t)size : buf_len;
		size_t n = fread(buf, 1, to_read, f);

		if (n != to_read) {
			fclose(f);
			pthread_mutex_unlock(&settings_lock);
			return -EIO;
		}
	}

	fclose(f);
	pthread_mutex_unlock(&settings_lock);
	return 0;
}

int eai_settings_delete(const char *key)
{
	if (!key) {
		return -EINVAL;
	}

	char path[MAX_PATH_LEN];
	int ret = build_path(key, path, sizeof(path));

	if (ret) {
		return ret;
	}

	pthread_mutex_lock(&settings_lock);

	if (unlink(path) != 0) {
		pthread_mutex_unlock(&settings_lock);
		return (errno == ENOENT) ? -ENOENT : -EIO;
	}

	pthread_mutex_unlock(&settings_lock);
	return 0;
}

bool eai_settings_exists(const char *key)
{
	if (!key) {
		return false;
	}

	char path[MAX_PATH_LEN];

	if (build_path(key, path, sizeof(path)) != 0) {
		return false;
	}

	pthread_mutex_lock(&settings_lock);
	bool exists = (access(path, F_OK) == 0);
	pthread_mutex_unlock(&settings_lock);

	return exists;
}
