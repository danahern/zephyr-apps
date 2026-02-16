#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <string.h>

#include <wifi_prov/wifi_prov.h>

LOG_MODULE_DECLARE(wifi_prov, LOG_LEVEL_INF);

static struct wifi_prov_cred stored_cred;
static bool cred_loaded;

static int cred_set(const char *name, size_t len, settings_read_cb read_cb,
		    void *cb_arg)
{
	if (!strcmp(name, "ssid")) {
		if (len > WIFI_PROV_SSID_MAX_LEN) {
			return -EINVAL;
		}
		stored_cred.ssid_len = len;
		int rc = read_cb(cb_arg, stored_cred.ssid, len);

		return rc < 0 ? rc : 0;
	}

	if (!strcmp(name, "psk")) {
		if (len > WIFI_PROV_PSK_MAX_LEN) {
			return -EINVAL;
		}
		stored_cred.psk_len = len;
		int rc = read_cb(cb_arg, stored_cred.psk, len);

		return rc < 0 ? rc : 0;
	}

	if (!strcmp(name, "sec")) {
		if (len != sizeof(uint8_t)) {
			return -EINVAL;
		}
		int rc = read_cb(cb_arg, &stored_cred.security, len);

		return rc < 0 ? rc : 0;
	}

	return -ENOENT;
}

static int cred_commit(void)
{
	cred_loaded = true;
	return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(wifi_prov, "wifi_prov", NULL, cred_set,
			       cred_commit, NULL);

int wifi_prov_cred_store(const struct wifi_prov_cred *cred)
{
	int ret;

	if (!cred || cred->ssid_len == 0 || cred->ssid_len > WIFI_PROV_SSID_MAX_LEN) {
		return -EINVAL;
	}

	/* Update in-memory copy first (always works, even on QEMU) */
	memcpy(&stored_cred, cred, sizeof(stored_cred));
	cred_loaded = true;

	/* Persist to flash if a storage backend is available */
	ret = settings_save_one("wifi_prov/ssid", cred->ssid, cred->ssid_len);
	if (ret) {
		LOG_WRN("Failed to persist SSID: %d (in-memory OK)", ret);
	}

	ret = settings_save_one("wifi_prov/psk", cred->psk, cred->psk_len);
	if (ret) {
		LOG_WRN("Failed to persist PSK: %d (in-memory OK)", ret);
	}

	ret = settings_save_one("wifi_prov/sec", &cred->security,
				sizeof(cred->security));
	if (ret) {
		LOG_WRN("Failed to persist security: %d (in-memory OK)", ret);
	}

	LOG_INF("Credentials stored (SSID len=%u)", cred->ssid_len);
	return 0;
}

int wifi_prov_cred_load(struct wifi_prov_cred *cred)
{
	if (!cred) {
		return -EINVAL;
	}

	if (!cred_loaded) {
		int ret = settings_load_subtree("wifi_prov");
		if (ret) {
			return ret;
		}
	}

	if (stored_cred.ssid_len == 0) {
		return -ENOENT;
	}

	memcpy(cred, &stored_cred, sizeof(*cred));
	return 0;
}

int wifi_prov_cred_erase(void)
{
	/* Clear in-memory copy first */
	memset(&stored_cred, 0, sizeof(stored_cred));
	cred_loaded = true;

	/* Best-effort persistent delete */
	settings_delete("wifi_prov/ssid");
	settings_delete("wifi_prov/psk");
	settings_delete("wifi_prov/sec");

	LOG_INF("Credentials erased");
	return 0;
}

bool wifi_prov_cred_exists(void)
{
	if (!cred_loaded) {
		settings_load_subtree("wifi_prov");
	}

	return stored_cred.ssid_len > 0;
}
