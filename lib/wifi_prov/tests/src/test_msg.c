#include <zephyr/ztest.h>
#include <string.h>
#include <wifi_prov/wifi_prov_msg.h>

ZTEST(wifi_prov_msg, test_encode_decode_scan_result)
{
	struct wifi_prov_scan_result orig = {
		.ssid = "MyWiFi",
		.ssid_len = 6,
		.rssi = -42,
		.security = WIFI_PROV_SEC_WPA2_PSK,
		.channel = 6,
	};
	struct wifi_prov_scan_result decoded = {0};
	uint8_t buf[64];

	int len = wifi_prov_msg_encode_scan_result(&orig, buf, sizeof(buf));

	zassert_true(len > 0, "Encode should return positive length, got %d", len);
	zassert_ok(wifi_prov_msg_decode_scan_result(buf, len, &decoded));
	zassert_equal(decoded.ssid_len, 6);
	zassert_mem_equal(decoded.ssid, "MyWiFi", 6);
	zassert_equal(decoded.rssi, -42);
	zassert_equal(decoded.security, WIFI_PROV_SEC_WPA2_PSK);
	zassert_equal(decoded.channel, 6);
}

ZTEST(wifi_prov_msg, test_encode_decode_credentials)
{
	struct wifi_prov_cred orig = {
		.ssid = "HomeNet",
		.ssid_len = 7,
		.psk = "secret123",
		.psk_len = 9,
		.security = WIFI_PROV_SEC_WPA2_PSK,
	};
	struct wifi_prov_cred decoded = {0};
	uint8_t buf[128];

	int len = wifi_prov_msg_encode_credentials(&orig, buf, sizeof(buf));

	zassert_true(len > 0, "Encode should return positive length, got %d", len);
	zassert_ok(wifi_prov_msg_decode_credentials(buf, len, &decoded));
	zassert_equal(decoded.ssid_len, 7);
	zassert_mem_equal(decoded.ssid, "HomeNet", 7);
	zassert_equal(decoded.psk_len, 9);
	zassert_mem_equal(decoded.psk, "secret123", 9);
	zassert_equal(decoded.security, WIFI_PROV_SEC_WPA2_PSK);
}

ZTEST(wifi_prov_msg, test_encode_decode_status)
{
	uint8_t ip_orig[4] = {192, 168, 1, 42};
	uint8_t ip_decoded[4] = {0};
	enum wifi_prov_state state;
	uint8_t buf[8];

	int len = wifi_prov_msg_encode_status(WIFI_PROV_STATE_CONNECTED,
					      ip_orig, buf, sizeof(buf));

	zassert_equal(len, 5, "Status should always be 5 bytes");
	zassert_ok(wifi_prov_msg_decode_status(buf, len, &state, ip_decoded));
	zassert_equal(state, WIFI_PROV_STATE_CONNECTED);
	zassert_mem_equal(ip_decoded, ip_orig, 4, "IP address mismatch");
}

ZTEST(wifi_prov_msg, test_decode_truncated_scan_result)
{
	/* Only 2 bytes — too short for any valid scan result */
	uint8_t buf[] = {6, 'A'};
	struct wifi_prov_scan_result result;

	zassert_equal(wifi_prov_msg_decode_scan_result(buf, sizeof(buf), &result),
		      -EINVAL, "Should reject truncated scan result");
}

ZTEST(wifi_prov_msg, test_decode_truncated_credentials)
{
	/* ssid_len=5 but only 3 bytes total */
	uint8_t buf[] = {5, 'A', 'B'};
	struct wifi_prov_cred cred;

	zassert_equal(wifi_prov_msg_decode_credentials(buf, sizeof(buf), &cred),
		      -EINVAL, "Should reject truncated credentials");
}

ZTEST(wifi_prov_msg, test_encode_buffer_too_small)
{
	struct wifi_prov_scan_result result = {
		.ssid = "Test",
		.ssid_len = 4,
		.rssi = -50,
		.security = 0,
		.channel = 1,
	};
	uint8_t buf[2]; /* Too small for 4 + 1 + 3 = 8 bytes */

	zassert_equal(wifi_prov_msg_encode_scan_result(&result, buf, sizeof(buf)),
		      -ENOBUFS, "Should return -ENOBUFS for small buffer");
}

ZTEST(wifi_prov_msg, test_max_length_ssid)
{
	struct wifi_prov_scan_result orig = {
		.ssid_len = WIFI_PROV_SSID_MAX_LEN,
		.rssi = -80,
		.security = WIFI_PROV_SEC_WPA3_SAE,
		.channel = 36,
	};
	struct wifi_prov_scan_result decoded = {0};
	uint8_t buf[64];

	memset(orig.ssid, 'X', WIFI_PROV_SSID_MAX_LEN);

	int len = wifi_prov_msg_encode_scan_result(&orig, buf, sizeof(buf));

	zassert_true(len > 0, "Should encode 32-byte SSID");
	zassert_ok(wifi_prov_msg_decode_scan_result(buf, len, &decoded));
	zassert_equal(decoded.ssid_len, WIFI_PROV_SSID_MAX_LEN);
}

ZTEST(wifi_prov_msg, test_empty_psk)
{
	struct wifi_prov_cred orig = {
		.ssid = "OpenNet",
		.ssid_len = 7,
		.psk_len = 0,
		.security = WIFI_PROV_SEC_NONE,
	};
	struct wifi_prov_cred decoded = {0};
	uint8_t buf[64];

	int len = wifi_prov_msg_encode_credentials(&orig, buf, sizeof(buf));

	zassert_true(len > 0, "Should encode open network credentials");
	zassert_ok(wifi_prov_msg_decode_credentials(buf, len, &decoded));
	zassert_equal(decoded.psk_len, 0, "PSK should be empty");
	zassert_equal(decoded.security, WIFI_PROV_SEC_NONE);
}

ZTEST_SUITE(wifi_prov_msg, NULL, NULL, NULL, NULL, NULL);
