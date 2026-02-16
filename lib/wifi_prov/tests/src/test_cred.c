#include <zephyr/ztest.h>
#include <zephyr/settings/settings.h>
#include <string.h>
#include <wifi_prov/wifi_prov.h>

static void *cred_setup(void)
{
	settings_subsys_init();
	wifi_prov_cred_erase();
	return NULL;
}

static void cred_before(void *fixture)
{
	ARG_UNUSED(fixture);
	wifi_prov_cred_erase();
}

ZTEST(wifi_prov_cred, test_no_cred_on_clean_boot)
{
	zassert_false(wifi_prov_cred_exists(),
		      "No credentials should exist after erase");
}

ZTEST(wifi_prov_cred, test_store_and_load)
{
	struct wifi_prov_cred cred = {
		.ssid = "TestNetwork",
		.ssid_len = 11,
		.psk = "password123",
		.psk_len = 11,
		.security = WIFI_PROV_SEC_WPA2_PSK,
	};
	struct wifi_prov_cred loaded = {0};

	zassert_ok(wifi_prov_cred_store(&cred), "Store should succeed");
	zassert_true(wifi_prov_cred_exists(), "Credentials should exist after store");
	zassert_ok(wifi_prov_cred_load(&loaded), "Load should succeed");
	zassert_equal(loaded.ssid_len, 11, "SSID length mismatch");
	zassert_mem_equal(loaded.ssid, "TestNetwork", 11, "SSID mismatch");
	zassert_equal(loaded.psk_len, 11, "PSK length mismatch");
	zassert_mem_equal(loaded.psk, "password123", 11, "PSK mismatch");
	zassert_equal(loaded.security, WIFI_PROV_SEC_WPA2_PSK, "Security mismatch");
}

ZTEST(wifi_prov_cred, test_erase)
{
	struct wifi_prov_cred cred = {
		.ssid = "ToErase",
		.ssid_len = 7,
		.psk = "pass",
		.psk_len = 4,
		.security = WIFI_PROV_SEC_WPA_PSK,
	};

	zassert_ok(wifi_prov_cred_store(&cred), "Store should succeed");
	zassert_true(wifi_prov_cred_exists(), "Should exist after store");
	zassert_ok(wifi_prov_cred_erase(), "Erase should succeed");
	zassert_false(wifi_prov_cred_exists(), "Should not exist after erase");
}

ZTEST(wifi_prov_cred, test_load_when_empty)
{
	struct wifi_prov_cred loaded = {0};
	int ret = wifi_prov_cred_load(&loaded);

	zassert_equal(ret, -ENOENT,
		      "Load should return -ENOENT when empty, got %d", ret);
}

ZTEST(wifi_prov_cred, test_overwrite)
{
	struct wifi_prov_cred first = {
		.ssid = "First",
		.ssid_len = 5,
		.psk = "pass1",
		.psk_len = 5,
		.security = WIFI_PROV_SEC_WPA_PSK,
	};
	struct wifi_prov_cred second = {
		.ssid = "Second",
		.ssid_len = 6,
		.psk = "pass2",
		.psk_len = 5,
		.security = WIFI_PROV_SEC_WPA2_PSK,
	};
	struct wifi_prov_cred loaded = {0};

	zassert_ok(wifi_prov_cred_store(&first), "First store should succeed");
	zassert_ok(wifi_prov_cred_store(&second), "Second store should succeed");
	zassert_ok(wifi_prov_cred_load(&loaded), "Load should succeed");
	zassert_equal(loaded.ssid_len, 6, "Should have second SSID length");
	zassert_mem_equal(loaded.ssid, "Second", 6, "Should have second SSID");
}

ZTEST_SUITE(wifi_prov_cred, NULL, cred_setup, cred_before, NULL, NULL);
