#include <zephyr/ztest.h>
#include <wifi_prov/wifi_prov.h>

static enum wifi_prov_state cb_old_state;
static enum wifi_prov_state cb_new_state;
static int cb_count;

static void test_state_callback(enum wifi_prov_state old_state,
				enum wifi_prov_state new_state)
{
	cb_old_state = old_state;
	cb_new_state = new_state;
	cb_count++;
}

static void sm_before(void *fixture)
{
	ARG_UNUSED(fixture);
	cb_count = 0;
	cb_old_state = WIFI_PROV_STATE_IDLE;
	cb_new_state = WIFI_PROV_STATE_IDLE;
	wifi_prov_sm_init(test_state_callback);
}

ZTEST(wifi_prov_sm, test_initial_state_is_idle)
{
	zassert_equal(wifi_prov_sm_get_state(), WIFI_PROV_STATE_IDLE,
		      "Initial state should be IDLE");
}

ZTEST(wifi_prov_sm, test_scan_flow)
{
	zassert_ok(wifi_prov_sm_process_event(WIFI_PROV_EVT_SCAN_TRIGGER));
	zassert_equal(wifi_prov_sm_get_state(), WIFI_PROV_STATE_SCANNING);

	zassert_ok(wifi_prov_sm_process_event(WIFI_PROV_EVT_SCAN_DONE));
	zassert_equal(wifi_prov_sm_get_state(), WIFI_PROV_STATE_SCAN_COMPLETE);
}

ZTEST(wifi_prov_sm, test_provision_flow)
{
	/* Get to SCAN_COMPLETE */
	wifi_prov_sm_process_event(WIFI_PROV_EVT_SCAN_TRIGGER);
	wifi_prov_sm_process_event(WIFI_PROV_EVT_SCAN_DONE);

	zassert_ok(wifi_prov_sm_process_event(WIFI_PROV_EVT_CREDENTIALS_RX));
	zassert_equal(wifi_prov_sm_get_state(), WIFI_PROV_STATE_PROVISIONING);

	zassert_ok(wifi_prov_sm_process_event(WIFI_PROV_EVT_WIFI_CONNECTING));
	zassert_equal(wifi_prov_sm_get_state(), WIFI_PROV_STATE_CONNECTING);

	zassert_ok(wifi_prov_sm_process_event(WIFI_PROV_EVT_WIFI_CONNECTED));
	zassert_equal(wifi_prov_sm_get_state(), WIFI_PROV_STATE_CONNECTED);
}

ZTEST(wifi_prov_sm, test_connection_failure)
{
	/* Get to CONNECTING */
	wifi_prov_sm_process_event(WIFI_PROV_EVT_SCAN_TRIGGER);
	wifi_prov_sm_process_event(WIFI_PROV_EVT_SCAN_DONE);
	wifi_prov_sm_process_event(WIFI_PROV_EVT_CREDENTIALS_RX);
	wifi_prov_sm_process_event(WIFI_PROV_EVT_WIFI_CONNECTING);

	zassert_ok(wifi_prov_sm_process_event(WIFI_PROV_EVT_WIFI_FAILED));
	zassert_equal(wifi_prov_sm_get_state(), WIFI_PROV_STATE_IDLE,
		      "Should return to IDLE on failure");
}

ZTEST(wifi_prov_sm, test_disconnect)
{
	/* Get to CONNECTED */
	wifi_prov_sm_process_event(WIFI_PROV_EVT_SCAN_TRIGGER);
	wifi_prov_sm_process_event(WIFI_PROV_EVT_SCAN_DONE);
	wifi_prov_sm_process_event(WIFI_PROV_EVT_CREDENTIALS_RX);
	wifi_prov_sm_process_event(WIFI_PROV_EVT_WIFI_CONNECTING);
	wifi_prov_sm_process_event(WIFI_PROV_EVT_WIFI_CONNECTED);

	zassert_ok(wifi_prov_sm_process_event(WIFI_PROV_EVT_WIFI_DISCONNECTED));
	zassert_equal(wifi_prov_sm_get_state(), WIFI_PROV_STATE_IDLE);
}

ZTEST(wifi_prov_sm, test_factory_reset_from_connected)
{
	/* Get to CONNECTED */
	wifi_prov_sm_process_event(WIFI_PROV_EVT_SCAN_TRIGGER);
	wifi_prov_sm_process_event(WIFI_PROV_EVT_SCAN_DONE);
	wifi_prov_sm_process_event(WIFI_PROV_EVT_CREDENTIALS_RX);
	wifi_prov_sm_process_event(WIFI_PROV_EVT_WIFI_CONNECTING);
	wifi_prov_sm_process_event(WIFI_PROV_EVT_WIFI_CONNECTED);

	zassert_ok(wifi_prov_sm_process_event(WIFI_PROV_EVT_FACTORY_RESET));
	zassert_equal(wifi_prov_sm_get_state(), WIFI_PROV_STATE_IDLE);
}

ZTEST(wifi_prov_sm, test_factory_reset_from_scanning)
{
	wifi_prov_sm_process_event(WIFI_PROV_EVT_SCAN_TRIGGER);
	zassert_equal(wifi_prov_sm_get_state(), WIFI_PROV_STATE_SCANNING);

	zassert_ok(wifi_prov_sm_process_event(WIFI_PROV_EVT_FACTORY_RESET));
	zassert_equal(wifi_prov_sm_get_state(), WIFI_PROV_STATE_IDLE);
}

ZTEST(wifi_prov_sm, test_invalid_transition)
{
	/* IDLE + SCAN_DONE should be invalid */
	int ret = wifi_prov_sm_process_event(WIFI_PROV_EVT_SCAN_DONE);

	zassert_equal(ret, -EPERM, "Should return -EPERM for invalid transition");
	zassert_equal(wifi_prov_sm_get_state(), WIFI_PROV_STATE_IDLE,
		      "State should not change on invalid transition");
}

ZTEST(wifi_prov_sm, test_state_change_callback)
{
	int initial_count = cb_count;

	wifi_prov_sm_process_event(WIFI_PROV_EVT_SCAN_TRIGGER);

	zassert_equal(cb_count, initial_count + 1, "Callback should fire once");
	zassert_equal(cb_old_state, WIFI_PROV_STATE_IDLE, "Old state should be IDLE");
	zassert_equal(cb_new_state, WIFI_PROV_STATE_SCANNING,
		      "New state should be SCANNING");
}

ZTEST_SUITE(wifi_prov_sm, NULL, NULL, sm_before, NULL, NULL);
