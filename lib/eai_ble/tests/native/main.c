/*
 * eai_ble POSIX stub tests — 9 test cases.
 *
 * Verifies API contract using the POSIX stub backend.
 */

#include "unity.h"
#include <eai_ble/eai_ble.h>
#include <errno.h>
#include <string.h>

/* Track callback invocations */
static int connect_count;
static int disconnect_count;

static void on_connect(void)
{
	connect_count++;
}

static void on_disconnect(void)
{
	disconnect_count++;
}

static const struct eai_ble_callbacks test_cbs = {
	.on_connect = on_connect,
	.on_disconnect = on_disconnect,
};

/* Sample service with 3 characteristics */
static void dummy_write(uint8_t idx, const uint8_t *data, uint16_t len)
{
	(void)idx;
	(void)data;
	(void)len;
}

static const struct eai_ble_char test_chars[] = {
	{
		.uuid = EAI_BLE_UUID128_INIT(0x12345678, 0x1234, 0x1234,
					     0x1234, 0x123456789abcULL),
		.properties = EAI_BLE_PROP_READ,
		.on_write = NULL,
		.on_read = NULL,
	},
	{
		.uuid = EAI_BLE_UUID128_INIT(0x12345678, 0x1234, 0x1234,
					     0x1234, 0x123456789abdULL),
		.properties = EAI_BLE_PROP_WRITE,
		.on_write = dummy_write,
		.on_read = NULL,
	},
	{
		.uuid = EAI_BLE_UUID128_INIT(0x12345678, 0x1234, 0x1234,
					     0x1234, 0x123456789abeULL),
		.properties = EAI_BLE_PROP_NOTIFY,
		.on_write = NULL,
		.on_read = NULL,
	},
};

static const struct eai_ble_service test_svc = {
	.uuid = EAI_BLE_UUID128_INIT(0xa0e4f2b0, 0x0001, 0x1000, 0x8000,
				     0x00805f9b34fbULL),
	.chars = test_chars,
	.char_count = 3,
};

void setUp(void)
{
	connect_count = 0;
	disconnect_count = 0;
}

void tearDown(void)
{
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 1: init returns 0
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_init_success(void)
{
	TEST_ASSERT_EQUAL(0, eai_ble_init(&test_cbs));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 2: init with NULL callbacks returns 0
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_init_null_callbacks(void)
{
	TEST_ASSERT_EQUAL(0, eai_ble_init(NULL));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 3: register 3-char service returns 0
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_register_service(void)
{
	eai_ble_init(&test_cbs);
	TEST_ASSERT_EQUAL(0, eai_ble_gatt_register(&test_svc));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 4: register NULL returns -EINVAL
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_register_null_service(void)
{
	eai_ble_init(&test_cbs);
	TEST_ASSERT_EQUAL(-EINVAL, eai_ble_gatt_register(NULL));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 5: register >8 chars returns -EINVAL
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_register_too_many_chars(void)
{
	struct eai_ble_char many_chars[9];
	struct eai_ble_service big_svc = {
		.chars = many_chars,
		.char_count = 9,
	};

	memset(many_chars, 0, sizeof(many_chars));

	eai_ble_init(&test_cbs);
	TEST_ASSERT_EQUAL(-EINVAL, eai_ble_gatt_register(&big_svc));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 6: adv_start after init+register returns 0
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_adv_start(void)
{
	eai_ble_init(&test_cbs);
	eai_ble_gatt_register(&test_svc);
	TEST_ASSERT_EQUAL(0, eai_ble_adv_start("TestDevice"));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 7: notify returns -ENOTCONN when not connected
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_notify_not_connected(void)
{
	uint8_t data[] = {0x01, 0x02};

	eai_ble_init(&test_cbs);
	eai_ble_gatt_register(&test_svc);
	TEST_ASSERT_EQUAL(-ENOTCONN, eai_ble_notify(2, data, sizeof(data)));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 8: notify with NULL data returns -EINVAL
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_notify_null_data(void)
{
	eai_ble_init(&test_cbs);
	eai_ble_gatt_register(&test_svc);
	eai_ble_test_set_connected(true);
	TEST_ASSERT_EQUAL(-EINVAL, eai_ble_notify(0, NULL, 10));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 9: is_connected returns false initially
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_is_connected_default(void)
{
	eai_ble_init(&test_cbs);
	TEST_ASSERT_FALSE(eai_ble_is_connected());
}

/* ═══════════════════════════════════════════════════════════════════════════ */

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_init_success);
	RUN_TEST(test_init_null_callbacks);
	RUN_TEST(test_register_service);
	RUN_TEST(test_register_null_service);
	RUN_TEST(test_register_too_many_chars);
	RUN_TEST(test_adv_start);
	RUN_TEST(test_notify_not_connected);
	RUN_TEST(test_notify_null_data);
	RUN_TEST(test_is_connected_default);
	return UNITY_END();
}
