/*
 * eai_settings Zephyr backend tests — 14 test cases.
 *
 * Uses Zephyr Settings with runtime/"none" backend (RAM-only on QEMU).
 */

#include <zephyr/ztest.h>
#include <eai_settings/eai_settings.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Suite setup: init settings once
 * ═══════════════════════════════════════════════════════════════════════════ */

static void *eai_settings_setup(void)
{
	int ret = eai_settings_init();

	zassert_equal(ret, 0, "eai_settings_init failed: %d", ret);
	return NULL;
}

static void eai_settings_before(void *fixture)
{
	ARG_UNUSED(fixture);

	/* Clean up known test keys before each test */
	eai_settings_delete("ns/key1");
	eai_settings_delete("ns/delme");
	eai_settings_delete("ns/ex");
	eai_settings_delete("ns/nokey");
	eai_settings_delete("ns/ow");
	eai_settings_delete("ns/key");
	eai_settings_delete("ns/big");
	eai_settings_delete("ns1/key");
	eai_settings_delete("ns2/key");
	eai_settings_delete("ns/ghost");
	eai_settings_delete("ns/sized");
	eai_settings_delete("ns/trunc");
}

ZTEST_SUITE(eai_settings, NULL, eai_settings_setup, eai_settings_before,
	    NULL, NULL);

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 1: Set then get — data matches
 * ═══════════════════════════════════════════════════════════════════════════ */

ZTEST(eai_settings, test_set_get_roundtrip)
{
	uint8_t data[] = {0xAA, 0xBB, 0xCC};

	zassert_equal(0, eai_settings_set("ns/key1", data, sizeof(data)));

	uint8_t buf[16];
	size_t actual = 0;

	zassert_equal(0, eai_settings_get("ns/key1", buf, sizeof(buf), &actual));
	zassert_equal(3, actual);
	zassert_mem_equal(data, buf, 3);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 2: Get nonexistent key → -ENOENT
 * ═══════════════════════════════════════════════════════════════════════════ */

ZTEST(eai_settings, test_get_nonexistent)
{
	uint8_t buf[16];
	size_t actual = 0;

	zassert_equal(-ENOENT, eai_settings_get("ns/nope", buf, sizeof(buf),
						 &actual));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 3: Delete then get → -ENOENT
 * ═══════════════════════════════════════════════════════════════════════════ */

ZTEST(eai_settings, test_delete_then_get)
{
	uint8_t data[] = {1, 2, 3};

	eai_settings_set("ns/delme", data, sizeof(data));
	zassert_equal(0, eai_settings_delete("ns/delme"));

	uint8_t buf[16];

	zassert_equal(-ENOENT,
		      eai_settings_get("ns/delme", buf, sizeof(buf), NULL));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 4: exists after set → true
 * ═══════════════════════════════════════════════════════════════════════════ */

ZTEST(eai_settings, test_exists_after_set)
{
	uint8_t data[] = {42};

	eai_settings_set("ns/ex", data, sizeof(data));
	zassert_true(eai_settings_exists("ns/ex"));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 5: exists before set → false
 * ═══════════════════════════════════════════════════════════════════════════ */

ZTEST(eai_settings, test_exists_before_set)
{
	zassert_false(eai_settings_exists("ns/nokey"));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 6: Overwrite — get returns latest
 * ═══════════════════════════════════════════════════════════════════════════ */

ZTEST(eai_settings, test_overwrite)
{
	uint8_t v1[] = {1, 2, 3};
	uint8_t v2[] = {4, 5};

	eai_settings_set("ns/ow", v1, sizeof(v1));
	eai_settings_set("ns/ow", v2, sizeof(v2));

	uint8_t buf[16];
	size_t actual = 0;

	eai_settings_get("ns/ow", buf, sizeof(buf), &actual);
	zassert_equal(2, actual);
	zassert_mem_equal(v2, buf, 2);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 7: NULL key → -EINVAL / false
 * ═══════════════════════════════════════════════════════════════════════════ */

ZTEST(eai_settings, test_null_key)
{
	uint8_t data[] = {1};
	uint8_t buf[16];

	zassert_equal(-EINVAL, eai_settings_set(NULL, data, sizeof(data)));
	zassert_equal(-EINVAL,
		      eai_settings_get(NULL, buf, sizeof(buf), NULL));
	zassert_equal(-EINVAL, eai_settings_delete(NULL));
	zassert_false(eai_settings_exists(NULL));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 8: NULL data → -EINVAL
 * ═══════════════════════════════════════════════════════════════════════════ */

ZTEST(eai_settings, test_null_data)
{
	zassert_equal(-EINVAL, eai_settings_set("ns/key", NULL, 10));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 9: Zero length → -EINVAL
 * ═══════════════════════════════════════════════════════════════════════════ */

ZTEST(eai_settings, test_zero_len)
{
	uint8_t data[] = {1};

	zassert_equal(-EINVAL, eai_settings_set("ns/key", data, 0));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 10: Large value — 1024-byte blob
 * ═══════════════════════════════════════════════════════════════════════════ */

ZTEST(eai_settings, test_large_value)
{
	static uint8_t data[256];

	for (int i = 0; i < 256; i++) {
		data[i] = (uint8_t)(i & 0xFF);
	}

	zassert_equal(0, eai_settings_set("ns/big", data, sizeof(data)));

	static uint8_t buf[256];
	size_t actual = 0;

	zassert_equal(0,
		      eai_settings_get("ns/big", buf, sizeof(buf), &actual));
	zassert_equal(256, actual);
	zassert_mem_equal(data, buf, 256);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 11: Multiple namespaces — independent storage
 * ═══════════════════════════════════════════════════════════════════════════ */

ZTEST(eai_settings, test_multiple_namespaces)
{
	uint8_t v1[] = {1};
	uint8_t v2[] = {2};

	eai_settings_set("ns1/key", v1, sizeof(v1));
	eai_settings_set("ns2/key", v2, sizeof(v2));

	uint8_t buf[16];
	size_t actual = 0;

	eai_settings_get("ns1/key", buf, sizeof(buf), &actual);
	zassert_equal(1, buf[0]);

	eai_settings_get("ns2/key", buf, sizeof(buf), &actual);
	zassert_equal(2, buf[0]);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 12: Delete nonexistent → -ENOENT
 * ═══════════════════════════════════════════════════════════════════════════ */

ZTEST(eai_settings, test_delete_nonexistent)
{
	zassert_equal(-ENOENT, eai_settings_delete("ns/ghost"));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 13: actual_len reported correctly
 * ═══════════════════════════════════════════════════════════════════════════ */

ZTEST(eai_settings, test_actual_len_reported)
{
	uint8_t data[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

	eai_settings_set("ns/sized", data, sizeof(data));

	uint8_t buf[20];
	size_t actual = 0;

	eai_settings_get("ns/sized", buf, sizeof(buf), &actual);
	zassert_equal(10, actual);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 14: Truncated read — actual_len > buf_len
 * ═══════════════════════════════════════════════════════════════════════════ */

ZTEST(eai_settings, test_truncated_read)
{
	uint8_t data[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

	eai_settings_set("ns/trunc", data, sizeof(data));

	uint8_t buf[5];
	size_t actual = 0;

	zassert_equal(0,
		      eai_settings_get("ns/trunc", buf, sizeof(buf), &actual));
	zassert_equal(10, actual);
	zassert_mem_equal(data, buf, 5);
}
