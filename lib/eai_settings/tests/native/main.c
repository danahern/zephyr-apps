/*
 * eai_settings POSIX backend tests — 14 test cases.
 *
 * Uses a unique temp directory per test run to avoid interference.
 */

#include "unity.h"
#include <eai_settings/eai_settings.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>

#define TEST_BASE_PATH "/tmp/eai_settings_test"

/* Recursive remove helper */
static void rmrf(const char *path)
{
	DIR *d = opendir(path);

	if (!d) {
		unlink(path);
		return;
	}

	struct dirent *ent;

	while ((ent = readdir(d)) != NULL) {
		if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
			continue;
		}
		char child[512];
		snprintf(child, sizeof(child), "%s/%s", path, ent->d_name);
		rmrf(child);
	}
	closedir(d);
	rmdir(path);
}

void setUp(void)
{
	rmrf(TEST_BASE_PATH);
	eai_settings_init();
}

void tearDown(void)
{
	rmrf(TEST_BASE_PATH);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 1: Set then get — data matches
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_set_get_roundtrip(void)
{
	uint8_t data[] = {0xAA, 0xBB, 0xCC};
	TEST_ASSERT_EQUAL(0, eai_settings_set("ns/key1", data, sizeof(data)));

	uint8_t buf[16];
	size_t actual = 0;

	TEST_ASSERT_EQUAL(0, eai_settings_get("ns/key1", buf, sizeof(buf), &actual));
	TEST_ASSERT_EQUAL(3, actual);
	TEST_ASSERT_EQUAL_HEX8_ARRAY(data, buf, 3);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 2: Get nonexistent key → -ENOENT
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_get_nonexistent(void)
{
	uint8_t buf[16];
	size_t actual = 0;

	TEST_ASSERT_EQUAL(-ENOENT, eai_settings_get("ns/nope", buf, sizeof(buf), &actual));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 3: Delete then get → -ENOENT
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_delete_then_get(void)
{
	uint8_t data[] = {1, 2, 3};

	eai_settings_set("ns/delme", data, sizeof(data));
	TEST_ASSERT_EQUAL(0, eai_settings_delete("ns/delme"));

	uint8_t buf[16];
	TEST_ASSERT_EQUAL(-ENOENT, eai_settings_get("ns/delme", buf, sizeof(buf), NULL));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 4: exists after set → true
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_exists_after_set(void)
{
	uint8_t data[] = {42};

	eai_settings_set("ns/ex", data, sizeof(data));
	TEST_ASSERT_TRUE(eai_settings_exists("ns/ex"));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 5: exists before set → false
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_exists_before_set(void)
{
	TEST_ASSERT_FALSE(eai_settings_exists("ns/nokey"));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 6: Overwrite — get returns latest
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_overwrite(void)
{
	uint8_t v1[] = {1, 2, 3};
	uint8_t v2[] = {4, 5};

	eai_settings_set("ns/ow", v1, sizeof(v1));
	eai_settings_set("ns/ow", v2, sizeof(v2));

	uint8_t buf[16];
	size_t actual = 0;

	eai_settings_get("ns/ow", buf, sizeof(buf), &actual);
	TEST_ASSERT_EQUAL(2, actual);
	TEST_ASSERT_EQUAL_HEX8_ARRAY(v2, buf, 2);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 7: NULL key → -EINVAL / false
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_null_key(void)
{
	uint8_t data[] = {1};
	uint8_t buf[16];

	TEST_ASSERT_EQUAL(-EINVAL, eai_settings_set(NULL, data, sizeof(data)));
	TEST_ASSERT_EQUAL(-EINVAL, eai_settings_get(NULL, buf, sizeof(buf), NULL));
	TEST_ASSERT_EQUAL(-EINVAL, eai_settings_delete(NULL));
	TEST_ASSERT_FALSE(eai_settings_exists(NULL));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 8: NULL data → -EINVAL
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_null_data(void)
{
	TEST_ASSERT_EQUAL(-EINVAL, eai_settings_set("ns/key", NULL, 10));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 9: Zero length → -EINVAL
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_zero_len(void)
{
	uint8_t data[] = {1};

	TEST_ASSERT_EQUAL(-EINVAL, eai_settings_set("ns/key", data, 0));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 10: Large value — 1024-byte blob
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_large_value(void)
{
	uint8_t data[1024];

	for (int i = 0; i < 1024; i++) {
		data[i] = (uint8_t)(i & 0xFF);
	}

	TEST_ASSERT_EQUAL(0, eai_settings_set("ns/big", data, sizeof(data)));

	uint8_t buf[1024];
	size_t actual = 0;

	TEST_ASSERT_EQUAL(0, eai_settings_get("ns/big", buf, sizeof(buf), &actual));
	TEST_ASSERT_EQUAL(1024, actual);
	TEST_ASSERT_EQUAL_HEX8_ARRAY(data, buf, 1024);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 11: Multiple namespaces — independent storage
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_multiple_namespaces(void)
{
	uint8_t v1[] = {1};
	uint8_t v2[] = {2};

	eai_settings_set("ns1/key", v1, sizeof(v1));
	eai_settings_set("ns2/key", v2, sizeof(v2));

	uint8_t buf[16];
	size_t actual = 0;

	eai_settings_get("ns1/key", buf, sizeof(buf), &actual);
	TEST_ASSERT_EQUAL(1, buf[0]);

	eai_settings_get("ns2/key", buf, sizeof(buf), &actual);
	TEST_ASSERT_EQUAL(2, buf[0]);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 12: Delete nonexistent → -ENOENT
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_delete_nonexistent(void)
{
	TEST_ASSERT_EQUAL(-ENOENT, eai_settings_delete("ns/ghost"));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 13: actual_len reported correctly
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_actual_len_reported(void)
{
	uint8_t data[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

	eai_settings_set("ns/sized", data, sizeof(data));

	uint8_t buf[20];
	size_t actual = 0;

	eai_settings_get("ns/sized", buf, sizeof(buf), &actual);
	TEST_ASSERT_EQUAL(10, actual);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 14: Truncated read — actual_len > buf_len
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_truncated_read(void)
{
	uint8_t data[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

	eai_settings_set("ns/trunc", data, sizeof(data));

	uint8_t buf[5];
	size_t actual = 0;

	TEST_ASSERT_EQUAL(0, eai_settings_get("ns/trunc", buf, sizeof(buf), &actual));
	TEST_ASSERT_EQUAL(10, actual);
	/* First 5 bytes should match */
	TEST_ASSERT_EQUAL_HEX8_ARRAY(data, buf, 5);
}

/* ═══════════════════════════════════════════════════════════════════════════ */

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_set_get_roundtrip);
	RUN_TEST(test_get_nonexistent);
	RUN_TEST(test_delete_then_get);
	RUN_TEST(test_exists_after_set);
	RUN_TEST(test_exists_before_set);
	RUN_TEST(test_overwrite);
	RUN_TEST(test_null_key);
	RUN_TEST(test_null_data);
	RUN_TEST(test_zero_len);
	RUN_TEST(test_large_value);
	RUN_TEST(test_multiple_namespaces);
	RUN_TEST(test_delete_nonexistent);
	RUN_TEST(test_actual_len_reported);
	RUN_TEST(test_truncated_read);
	return UNITY_END();
}
