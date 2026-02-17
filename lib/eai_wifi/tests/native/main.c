/*
 * eai_wifi POSIX stub tests — 17 test cases.
 *
 * Verifies API contract using the POSIX stub backend.
 */

#include "unity.h"
#include <eai_wifi/eai_wifi.h>
#include <errno.h>
#include <string.h>

/* ── Callback tracking ───────────────────────────────────────────────────── */

static int event_count;
static enum eai_wifi_event last_event;

static void on_event(enum eai_wifi_event event)
{
	event_count++;
	last_event = event;
}

static int scan_result_count;
static struct eai_wifi_scan_result last_scan_result;

static void on_scan_result(const struct eai_wifi_scan_result *result)
{
	scan_result_count++;
	last_scan_result = *result;
}

static int scan_done_count;
static int last_scan_done_status;

static void on_scan_done(int status)
{
	scan_done_count++;
	last_scan_done_status = status;
}

/* ── Setup / teardown ────────────────────────────────────────────────────── */

void setUp(void)
{
	eai_wifi_test_reset();
	event_count = 0;
	last_event = 0;
	scan_result_count = 0;
	memset(&last_scan_result, 0, sizeof(last_scan_result));
	scan_done_count = 0;
	last_scan_done_status = 0;
}

void tearDown(void)
{
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 1: init returns 0
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_init_success(void)
{
	TEST_ASSERT_EQUAL(0, eai_wifi_init());
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 2: state after init is DISCONNECTED
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_state_after_init(void)
{
	eai_wifi_init();
	TEST_ASSERT_EQUAL(EAI_WIFI_STATE_DISCONNECTED, eai_wifi_get_state());
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 3: set_event_callback stores callback (verified via test helpers)
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_set_event_callback(void)
{
	eai_wifi_init();
	eai_wifi_set_event_callback(on_event);

	/* Verify callback fires by simulating a connect */
	eai_wifi_connect((const uint8_t *)"test", 4, NULL, 0, EAI_WIFI_SEC_OPEN);
	uint8_t ip[] = {192, 168, 1, 100};
	eai_wifi_test_set_connected(ip);

	TEST_ASSERT_EQUAL(1, event_count);
	TEST_ASSERT_EQUAL(EAI_WIFI_EVT_CONNECTED, last_event);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 4: set_event_callback NULL unregisters (no crash on events)
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_set_event_callback_null(void)
{
	eai_wifi_init();
	eai_wifi_set_event_callback(on_event);
	eai_wifi_set_event_callback(NULL);

	/* Simulate connect — should not crash or increment counter */
	uint8_t ip[] = {10, 0, 0, 1};
	eai_wifi_test_set_connected(ip);

	TEST_ASSERT_EQUAL(0, event_count);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 5: scan returns 0, state becomes SCANNING
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_scan_success(void)
{
	eai_wifi_init();
	TEST_ASSERT_EQUAL(0, eai_wifi_scan(on_scan_result, on_scan_done));
	TEST_ASSERT_EQUAL(EAI_WIFI_STATE_SCANNING, eai_wifi_get_state());
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 6: scan(NULL, ...) returns -EINVAL
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_scan_null_callback(void)
{
	eai_wifi_init();
	TEST_ASSERT_EQUAL(-EINVAL, eai_wifi_scan(NULL, on_scan_done));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 7: inject_scan_result delivers to on_result
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_scan_result_delivery(void)
{
	eai_wifi_init();
	eai_wifi_scan(on_scan_result, on_scan_done);

	struct eai_wifi_scan_result result = {0};
	memcpy(result.ssid, "MyNetwork", 9);
	result.ssid_len = 9;
	result.rssi = -42;
	result.security = EAI_WIFI_SEC_WPA2_PSK;
	result.channel = 6;

	eai_wifi_test_inject_scan_result(&result);

	TEST_ASSERT_EQUAL(1, scan_result_count);
	TEST_ASSERT_EQUAL(9, last_scan_result.ssid_len);
	TEST_ASSERT_EQUAL(-42, last_scan_result.rssi);
	TEST_ASSERT_EQUAL(EAI_WIFI_SEC_WPA2_PSK, last_scan_result.security);
	TEST_ASSERT_EQUAL(6, last_scan_result.channel);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 8: complete_scan delivers to on_done with status
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_scan_done_delivery(void)
{
	eai_wifi_init();
	eai_wifi_scan(on_scan_result, on_scan_done);
	eai_wifi_test_complete_scan(0);

	TEST_ASSERT_EQUAL(1, scan_done_count);
	TEST_ASSERT_EQUAL(0, last_scan_done_status);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 9: after scan done, state returns to DISCONNECTED
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_scan_state_restored(void)
{
	eai_wifi_init();
	eai_wifi_scan(on_scan_result, on_scan_done);
	TEST_ASSERT_EQUAL(EAI_WIFI_STATE_SCANNING, eai_wifi_get_state());

	eai_wifi_test_complete_scan(0);
	TEST_ASSERT_EQUAL(EAI_WIFI_STATE_DISCONNECTED, eai_wifi_get_state());
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 10: connect returns 0, state becomes CONNECTING
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_connect_success(void)
{
	eai_wifi_init();
	const uint8_t ssid[] = "TestAP";
	const uint8_t psk[] = "password";

	TEST_ASSERT_EQUAL(0, eai_wifi_connect(ssid, 6, psk, 8,
					      EAI_WIFI_SEC_WPA2_PSK));
	TEST_ASSERT_EQUAL(EAI_WIFI_STATE_CONNECTING, eai_wifi_get_state());
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 11: connect(NULL, ...) returns -EINVAL
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_connect_null_ssid(void)
{
	eai_wifi_init();
	TEST_ASSERT_EQUAL(-EINVAL, eai_wifi_connect(NULL, 0, NULL, 0,
						    EAI_WIFI_SEC_OPEN));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 12: test_set_connected fires EVT_CONNECTED
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_connect_event_connected(void)
{
	eai_wifi_init();
	eai_wifi_set_event_callback(on_event);
	eai_wifi_connect((const uint8_t *)"AP", 2, NULL, 0, EAI_WIFI_SEC_OPEN);

	uint8_t ip[] = {192, 168, 1, 1};
	eai_wifi_test_set_connected(ip);

	TEST_ASSERT_EQUAL(1, event_count);
	TEST_ASSERT_EQUAL(EAI_WIFI_EVT_CONNECTED, last_event);
	TEST_ASSERT_EQUAL(EAI_WIFI_STATE_CONNECTED, eai_wifi_get_state());
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 13: test_set_connect_failed fires EVT_CONNECT_FAILED
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_connect_event_failed(void)
{
	eai_wifi_init();
	eai_wifi_set_event_callback(on_event);
	eai_wifi_connect((const uint8_t *)"AP", 2, (const uint8_t *)"pass", 4,
			 EAI_WIFI_SEC_WPA2_PSK);

	eai_wifi_test_set_connect_failed();

	TEST_ASSERT_EQUAL(1, event_count);
	TEST_ASSERT_EQUAL(EAI_WIFI_EVT_CONNECT_FAILED, last_event);
	TEST_ASSERT_EQUAL(EAI_WIFI_STATE_DISCONNECTED, eai_wifi_get_state());
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 14: disconnect returns 0
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_disconnect_success(void)
{
	eai_wifi_init();
	eai_wifi_connect((const uint8_t *)"AP", 2, NULL, 0, EAI_WIFI_SEC_OPEN);
	TEST_ASSERT_EQUAL(0, eai_wifi_disconnect());
	TEST_ASSERT_EQUAL(EAI_WIFI_STATE_DISCONNECTED, eai_wifi_get_state());
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 15: test_set_disconnected fires EVT_DISCONNECTED
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_disconnect_event(void)
{
	eai_wifi_init();
	eai_wifi_set_event_callback(on_event);

	uint8_t ip[] = {10, 0, 0, 1};
	eai_wifi_test_set_connected(ip);
	event_count = 0; /* Reset after connected event */

	eai_wifi_test_set_disconnected();

	TEST_ASSERT_EQUAL(1, event_count);
	TEST_ASSERT_EQUAL(EAI_WIFI_EVT_DISCONNECTED, last_event);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 16: get_ip returns -ENOTCONN when not connected
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_get_ip_not_connected(void)
{
	eai_wifi_init();
	uint8_t ip[4];
	TEST_ASSERT_EQUAL(-ENOTCONN, eai_wifi_get_ip(ip));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 17: get_ip returns correct IP after test_set_connected
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_get_ip_connected(void)
{
	eai_wifi_init();

	uint8_t expected_ip[] = {192, 168, 0, 42};
	eai_wifi_test_set_connected(expected_ip);

	uint8_t actual_ip[4] = {0};
	TEST_ASSERT_EQUAL(0, eai_wifi_get_ip(actual_ip));
	TEST_ASSERT_EQUAL_UINT8_ARRAY(expected_ip, actual_ip, 4);
}

/* ═══════════════════════════════════════════════════════════════════════════ */

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_init_success);
	RUN_TEST(test_state_after_init);
	RUN_TEST(test_set_event_callback);
	RUN_TEST(test_set_event_callback_null);
	RUN_TEST(test_scan_success);
	RUN_TEST(test_scan_null_callback);
	RUN_TEST(test_scan_result_delivery);
	RUN_TEST(test_scan_done_delivery);
	RUN_TEST(test_scan_state_restored);
	RUN_TEST(test_connect_success);
	RUN_TEST(test_connect_null_ssid);
	RUN_TEST(test_connect_event_connected);
	RUN_TEST(test_connect_event_failed);
	RUN_TEST(test_disconnect_success);
	RUN_TEST(test_disconnect_event);
	RUN_TEST(test_get_ip_not_connected);
	RUN_TEST(test_get_ip_connected);
	return UNITY_END();
}
