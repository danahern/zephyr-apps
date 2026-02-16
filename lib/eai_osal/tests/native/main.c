/*
 * OSAL POSIX backend tests — ported from ESP-IDF Unity tests.
 *
 * 44 tests across 9 suites: mutex, semaphore, thread, queue, timer,
 * event, critical, time, work.
 *
 * FreeRTOS-specific helpers replaced with OSAL semaphores (since
 * the semaphore is tested before any test that uses it as a helper).
 */

#include "unity.h"
#include <eai_osal/eai_osal.h>
#include <string.h>
#include <unistd.h>

/* Unity requires setUp/tearDown */
void setUp(void) {}
void tearDown(void) {}

/* Helper: sleep for ms */
static void test_sleep_ms(uint32_t ms)
{
	usleep((useconds_t)ms * 1000);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Mutex tests (6)
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_mutex_create_destroy(void)
{
	eai_osal_mutex_t mtx;
	TEST_ASSERT_EQUAL(EAI_OSAL_OK, eai_osal_mutex_create(&mtx));
	TEST_ASSERT_EQUAL(EAI_OSAL_OK, eai_osal_mutex_destroy(&mtx));
}

static void test_mutex_lock_unlock(void)
{
	eai_osal_mutex_t mtx;
	eai_osal_mutex_create(&mtx);
	TEST_ASSERT_EQUAL(EAI_OSAL_OK, eai_osal_mutex_lock(&mtx, EAI_OSAL_WAIT_FOREVER));
	TEST_ASSERT_EQUAL(EAI_OSAL_OK, eai_osal_mutex_unlock(&mtx));
	eai_osal_mutex_destroy(&mtx);
}

static void test_mutex_recursive_lock(void)
{
	eai_osal_mutex_t mtx;
	eai_osal_mutex_create(&mtx);
	TEST_ASSERT_EQUAL(EAI_OSAL_OK, eai_osal_mutex_lock(&mtx, EAI_OSAL_WAIT_FOREVER));
	TEST_ASSERT_EQUAL(EAI_OSAL_OK, eai_osal_mutex_lock(&mtx, EAI_OSAL_WAIT_FOREVER));
	TEST_ASSERT_EQUAL(EAI_OSAL_OK, eai_osal_mutex_unlock(&mtx));
	TEST_ASSERT_EQUAL(EAI_OSAL_OK, eai_osal_mutex_unlock(&mtx));
	eai_osal_mutex_destroy(&mtx);
}

static void test_mutex_try_lock(void)
{
	eai_osal_mutex_t mtx;
	eai_osal_mutex_create(&mtx);
	TEST_ASSERT_EQUAL(EAI_OSAL_OK, eai_osal_mutex_lock(&mtx, EAI_OSAL_NO_WAIT));
	TEST_ASSERT_EQUAL(EAI_OSAL_OK, eai_osal_mutex_unlock(&mtx));
	eai_osal_mutex_destroy(&mtx);
}

/* Contention test — use OSAL thread to hold mutex */
static eai_osal_mutex_t contention_mtx;

static void mutex_holder_entry(void *arg)
{
	(void)arg;
	eai_osal_mutex_lock(&contention_mtx, EAI_OSAL_WAIT_FOREVER);
	test_sleep_ms(200);
	eai_osal_mutex_unlock(&contention_mtx);
}

EAI_OSAL_THREAD_STACK_DEFINE(mtx_helper_stack, 2048);

static void test_mutex_contention_timeout(void)
{
	eai_osal_thread_t thread;
	eai_osal_mutex_create(&contention_mtx);

	eai_osal_thread_create(&thread, "holder", mutex_holder_entry, NULL,
			       mtx_helper_stack,
			       EAI_OSAL_THREAD_STACK_SIZEOF(mtx_helper_stack), 5);

	test_sleep_ms(10); /* let holder acquire */

	TEST_ASSERT_EQUAL(EAI_OSAL_TIMEOUT, eai_osal_mutex_lock(&contention_mtx, 50));

	eai_osal_thread_join(&thread, EAI_OSAL_WAIT_FOREVER);
	eai_osal_mutex_destroy(&contention_mtx);
}

static void test_mutex_null_param(void)
{
	TEST_ASSERT_EQUAL(EAI_OSAL_INVALID_PARAM, eai_osal_mutex_create(NULL));
	TEST_ASSERT_EQUAL(EAI_OSAL_INVALID_PARAM, eai_osal_mutex_lock(NULL, 0));
	TEST_ASSERT_EQUAL(EAI_OSAL_INVALID_PARAM, eai_osal_mutex_unlock(NULL));
	TEST_ASSERT_EQUAL(EAI_OSAL_INVALID_PARAM, eai_osal_mutex_destroy(NULL));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Semaphore tests (5)
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_sem_create_destroy(void)
{
	eai_osal_sem_t sem;
	TEST_ASSERT_EQUAL(EAI_OSAL_OK, eai_osal_sem_create(&sem, 0, 1));
	TEST_ASSERT_EQUAL(EAI_OSAL_OK, eai_osal_sem_destroy(&sem));
}

static void test_sem_binary(void)
{
	eai_osal_sem_t sem;
	eai_osal_sem_create(&sem, 0, 1);
	TEST_ASSERT_EQUAL(EAI_OSAL_TIMEOUT, eai_osal_sem_take(&sem, EAI_OSAL_NO_WAIT));
	TEST_ASSERT_EQUAL(EAI_OSAL_OK, eai_osal_sem_give(&sem));
	TEST_ASSERT_EQUAL(EAI_OSAL_OK, eai_osal_sem_take(&sem, EAI_OSAL_NO_WAIT));
	TEST_ASSERT_EQUAL(EAI_OSAL_TIMEOUT, eai_osal_sem_take(&sem, EAI_OSAL_NO_WAIT));
	eai_osal_sem_destroy(&sem);
}

static void test_sem_counting(void)
{
	eai_osal_sem_t sem;
	eai_osal_sem_create(&sem, 0, 5);
	for (int i = 0; i < 3; i++) {
		eai_osal_sem_give(&sem);
	}
	for (int i = 0; i < 3; i++) {
		TEST_ASSERT_EQUAL(EAI_OSAL_OK, eai_osal_sem_take(&sem, EAI_OSAL_NO_WAIT));
	}
	TEST_ASSERT_EQUAL(EAI_OSAL_TIMEOUT, eai_osal_sem_take(&sem, EAI_OSAL_NO_WAIT));
	eai_osal_sem_destroy(&sem);
}

static void test_sem_timeout(void)
{
	eai_osal_sem_t sem;
	eai_osal_sem_create(&sem, 0, 1);
	TEST_ASSERT_EQUAL(EAI_OSAL_TIMEOUT, eai_osal_sem_take(&sem, 50));
	eai_osal_sem_destroy(&sem);
}

static void test_sem_give_at_limit(void)
{
	eai_osal_sem_t sem;
	eai_osal_sem_create(&sem, 1, 1);
	eai_osal_sem_give(&sem); /* at limit — should not exceed */
	TEST_ASSERT_EQUAL(EAI_OSAL_OK, eai_osal_sem_take(&sem, EAI_OSAL_NO_WAIT));
	TEST_ASSERT_EQUAL(EAI_OSAL_TIMEOUT, eai_osal_sem_take(&sem, EAI_OSAL_NO_WAIT));
	eai_osal_sem_destroy(&sem);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Thread tests (4)
 * ═══════════════════════════════════════════════════════════════════════════ */

static volatile int thread_counter;

static void simple_thread_entry(void *arg)
{
	int *val = (int *)arg;
	thread_counter = *val;
}

EAI_OSAL_THREAD_STACK_DEFINE(test_thread_stack, 2048);

static void test_thread_create_join(void)
{
	eai_osal_thread_t thread;
	int arg = 42;

	thread_counter = 0;
	TEST_ASSERT_EQUAL(EAI_OSAL_OK,
			  eai_osal_thread_create(&thread, "test",
						 simple_thread_entry, &arg,
						 test_thread_stack,
						 EAI_OSAL_THREAD_STACK_SIZEOF(test_thread_stack),
						 10));
	TEST_ASSERT_EQUAL(EAI_OSAL_OK,
			  eai_osal_thread_join(&thread, EAI_OSAL_WAIT_FOREVER));
	TEST_ASSERT_EQUAL(42, thread_counter);
}

static void test_thread_sleep(void)
{
	uint32_t start = eai_osal_time_get_ms();
	eai_osal_thread_sleep(100);
	uint32_t elapsed = eai_osal_time_get_ms() - start;

	TEST_ASSERT_GREATER_OR_EQUAL(90, elapsed);
	TEST_ASSERT_LESS_OR_EQUAL(200, elapsed);
}

static void test_thread_yield(void)
{
	eai_osal_thread_yield(); /* should not hang */
}

/*
 * Priority test — adapted for POSIX.
 * POSIX SCHED_OTHER doesn't guarantee priority ordering without root.
 * Instead of checking FreeRTOS priority values, just verify both threads
 * execute at different OSAL priority levels.
 */
static eai_osal_sem_t prio_gate;
static volatile int prio_exec_count;

static void prio_thread_entry_gated(void *arg)
{
	(void)arg;
	__atomic_fetch_add(&prio_exec_count, 1, __ATOMIC_SEQ_CST);
	eai_osal_sem_take(&prio_gate, EAI_OSAL_WAIT_FOREVER);
}

EAI_OSAL_THREAD_STACK_DEFINE(prio_stack_a, 2048);
EAI_OSAL_THREAD_STACK_DEFINE(prio_stack_b, 2048);

static void test_thread_priority(void)
{
	eai_osal_thread_t thread_lo, thread_hi;
	prio_exec_count = 0;
	eai_osal_sem_create(&prio_gate, 0, 2);

	eai_osal_thread_create(&thread_lo, "lo", prio_thread_entry_gated,
			       NULL, prio_stack_a,
			       EAI_OSAL_THREAD_STACK_SIZEOF(prio_stack_a), 5);

	eai_osal_thread_create(&thread_hi, "hi", prio_thread_entry_gated,
			       NULL, prio_stack_b,
			       EAI_OSAL_THREAD_STACK_SIZEOF(prio_stack_b), 20);

	/* Give threads time to start */
	test_sleep_ms(50);

	/* Verify both threads executed (regardless of priority ordering) */
	TEST_ASSERT_EQUAL(2, prio_exec_count);

	/* Release both threads and clean up */
	eai_osal_sem_give(&prio_gate);
	eai_osal_sem_give(&prio_gate);

	eai_osal_thread_join(&thread_lo, EAI_OSAL_WAIT_FOREVER);
	eai_osal_thread_join(&thread_hi, EAI_OSAL_WAIT_FOREVER);
	eai_osal_sem_destroy(&prio_gate);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Queue tests (5)
 * ═══════════════════════════════════════════════════════════════════════════ */

static uint8_t __attribute__((aligned(4))) queue_buf_4[4 * sizeof(int)];
static uint8_t __attribute__((aligned(4))) queue_buf_2[2 * sizeof(int)];

static void test_queue_create_destroy(void)
{
	eai_osal_queue_t queue;
	TEST_ASSERT_EQUAL(EAI_OSAL_OK,
			  eai_osal_queue_create(&queue, sizeof(int), 4, queue_buf_4));
	TEST_ASSERT_EQUAL(EAI_OSAL_OK, eai_osal_queue_destroy(&queue));
}

static void test_queue_send_recv(void)
{
	eai_osal_queue_t queue;
	int msg;

	eai_osal_queue_create(&queue, sizeof(int), 4, queue_buf_4);

	msg = 99;
	TEST_ASSERT_EQUAL(EAI_OSAL_OK,
			  eai_osal_queue_send(&queue, &msg, EAI_OSAL_NO_WAIT));
	msg = 0;
	TEST_ASSERT_EQUAL(EAI_OSAL_OK,
			  eai_osal_queue_recv(&queue, &msg, EAI_OSAL_NO_WAIT));
	TEST_ASSERT_EQUAL(99, msg);

	eai_osal_queue_destroy(&queue);
}

static void test_queue_full(void)
{
	eai_osal_queue_t queue;
	int msg = 42;

	eai_osal_queue_create(&queue, sizeof(int), 2, queue_buf_2);

	TEST_ASSERT_EQUAL(EAI_OSAL_OK,
			  eai_osal_queue_send(&queue, &msg, EAI_OSAL_NO_WAIT));
	TEST_ASSERT_EQUAL(EAI_OSAL_OK,
			  eai_osal_queue_send(&queue, &msg, EAI_OSAL_NO_WAIT));
	TEST_ASSERT_EQUAL(EAI_OSAL_TIMEOUT,
			  eai_osal_queue_send(&queue, &msg, EAI_OSAL_NO_WAIT));

	eai_osal_queue_destroy(&queue);
}

static void test_queue_fifo_order(void)
{
	eai_osal_queue_t queue;
	int vals[] = {10, 20, 30, 40};
	int msg;

	eai_osal_queue_create(&queue, sizeof(int), 4, queue_buf_4);

	for (int i = 0; i < 4; i++) {
		eai_osal_queue_send(&queue, &vals[i], EAI_OSAL_NO_WAIT);
	}
	for (int i = 0; i < 4; i++) {
		eai_osal_queue_recv(&queue, &msg, EAI_OSAL_NO_WAIT);
		TEST_ASSERT_EQUAL(vals[i], msg);
	}

	eai_osal_queue_destroy(&queue);
}

static void test_queue_empty_timeout(void)
{
	eai_osal_queue_t queue;
	int msg;

	eai_osal_queue_create(&queue, sizeof(int), 2, queue_buf_2);
	TEST_ASSERT_EQUAL(EAI_OSAL_TIMEOUT,
			  eai_osal_queue_recv(&queue, &msg, EAI_OSAL_NO_WAIT));
	eai_osal_queue_destroy(&queue);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Timer tests (5)
 * ═══════════════════════════════════════════════════════════════════════════ */

static eai_osal_sem_t timer_sem;
static volatile int timer_count;

static void timer_callback(void *arg)
{
	(void)arg;
	__atomic_fetch_add(&timer_count, 1, __ATOMIC_SEQ_CST);
	eai_osal_sem_give(&timer_sem);
}

static void test_timer_create_destroy(void)
{
	eai_osal_timer_t timer;
	TEST_ASSERT_EQUAL(EAI_OSAL_OK,
			  eai_osal_timer_create(&timer, timer_callback, NULL));
	TEST_ASSERT_EQUAL(EAI_OSAL_OK, eai_osal_timer_destroy(&timer));
}

static void test_timer_one_shot(void)
{
	eai_osal_timer_t timer;
	timer_count = 0;
	eai_osal_sem_create(&timer_sem, 0, 10);

	eai_osal_timer_create(&timer, timer_callback, NULL);
	eai_osal_timer_start(&timer, 50, 0);

	eai_osal_sem_take(&timer_sem, 200);
	TEST_ASSERT_EQUAL(1, timer_count);

	test_sleep_ms(100);
	TEST_ASSERT_EQUAL(1, timer_count); /* should not fire again */

	eai_osal_timer_destroy(&timer);
	eai_osal_sem_destroy(&timer_sem);
}

static void test_timer_periodic(void)
{
	eai_osal_timer_t timer;
	timer_count = 0;
	eai_osal_sem_create(&timer_sem, 0, 10);

	eai_osal_timer_create(&timer, timer_callback, NULL);
	eai_osal_timer_start(&timer, 50, 50);

	test_sleep_ms(275);
	eai_osal_timer_stop(&timer);

	TEST_ASSERT_GREATER_OR_EQUAL(4, timer_count);
	TEST_ASSERT_LESS_OR_EQUAL(6, timer_count);

	eai_osal_timer_destroy(&timer);
	eai_osal_sem_destroy(&timer_sem);
}

static void test_timer_stop(void)
{
	eai_osal_timer_t timer;
	timer_count = 0;
	eai_osal_sem_create(&timer_sem, 0, 10);

	eai_osal_timer_create(&timer, timer_callback, NULL);
	eai_osal_timer_start(&timer, 50, 50);

	test_sleep_ms(75);
	eai_osal_timer_stop(&timer);
	int count_at_stop = timer_count;

	test_sleep_ms(200);
	TEST_ASSERT_EQUAL(count_at_stop, timer_count);

	eai_osal_timer_destroy(&timer);
	eai_osal_sem_destroy(&timer_sem);
}

static void test_timer_is_running(void)
{
	eai_osal_timer_t timer;
	eai_osal_timer_create(&timer, timer_callback, NULL);

	TEST_ASSERT_FALSE(eai_osal_timer_is_running(&timer));

	eai_osal_timer_start(&timer, 500, 0);
	test_sleep_ms(10);
	TEST_ASSERT_TRUE(eai_osal_timer_is_running(&timer));

	eai_osal_timer_stop(&timer);
	test_sleep_ms(20);
	TEST_ASSERT_FALSE(eai_osal_timer_is_running(&timer));

	eai_osal_timer_destroy(&timer);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Event tests (5)
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_event_create_destroy(void)
{
	eai_osal_event_t event;
	TEST_ASSERT_EQUAL(EAI_OSAL_OK, eai_osal_event_create(&event));
	TEST_ASSERT_EQUAL(EAI_OSAL_OK, eai_osal_event_destroy(&event));
}

static void test_event_set_wait_any(void)
{
	eai_osal_event_t event;
	uint32_t actual = 0;

	eai_osal_event_create(&event);
	eai_osal_event_set(&event, 0x03);

	TEST_ASSERT_EQUAL(EAI_OSAL_OK,
			  eai_osal_event_wait(&event, 0x0F, false, &actual,
					      EAI_OSAL_NO_WAIT));
	TEST_ASSERT_NOT_EQUAL(0, actual & 0x03);

	eai_osal_event_destroy(&event);
}

static void test_event_wait_all(void)
{
	eai_osal_event_t event;
	uint32_t actual = 0;

	eai_osal_event_create(&event);
	eai_osal_event_set(&event, 0x01);

	/* Only bit 0 set, wait for 0 AND 1 — timeout */
	TEST_ASSERT_EQUAL(EAI_OSAL_TIMEOUT,
			  eai_osal_event_wait(&event, 0x03, true, &actual,
					      EAI_OSAL_NO_WAIT));

	eai_osal_event_set(&event, 0x02);

	TEST_ASSERT_EQUAL(EAI_OSAL_OK,
			  eai_osal_event_wait(&event, 0x03, true, &actual,
					      EAI_OSAL_NO_WAIT));
	TEST_ASSERT_EQUAL(0x03, actual & 0x03);

	eai_osal_event_destroy(&event);
}

static void test_event_clear(void)
{
	eai_osal_event_t event;
	uint32_t actual = 0;

	eai_osal_event_create(&event);
	eai_osal_event_set(&event, 0x07);
	eai_osal_event_clear(&event, 0x02);

	TEST_ASSERT_EQUAL(EAI_OSAL_TIMEOUT,
			  eai_osal_event_wait(&event, 0x02, false, &actual,
					      EAI_OSAL_NO_WAIT));
	TEST_ASSERT_EQUAL(EAI_OSAL_OK,
			  eai_osal_event_wait(&event, 0x01, false, &actual,
					      EAI_OSAL_NO_WAIT));

	eai_osal_event_destroy(&event);
}

static void test_event_timeout(void)
{
	eai_osal_event_t event;
	uint32_t actual = 0;

	eai_osal_event_create(&event);
	TEST_ASSERT_EQUAL(EAI_OSAL_TIMEOUT,
			  eai_osal_event_wait(&event, 0xFF, false, &actual, 50));
	eai_osal_event_destroy(&event);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Critical section tests (2)
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_critical_enter_exit(void)
{
	volatile int shared = 0;
	eai_osal_critical_key_t key = eai_osal_critical_enter();
	shared = 42;
	eai_osal_critical_exit(key);
	TEST_ASSERT_EQUAL(42, shared);
}

static void test_critical_nested(void)
{
	eai_osal_critical_key_t key1 = eai_osal_critical_enter();
	eai_osal_critical_key_t key2 = eai_osal_critical_enter();
	eai_osal_critical_exit(key2);
	eai_osal_critical_exit(key1);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Time tests (3)
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_time_get_ms(void)
{
	uint32_t ms = eai_osal_time_get_ms();
	TEST_ASSERT_GREATER_OR_EQUAL(0, ms);
}

static void test_time_monotonic(void)
{
	uint32_t t1 = eai_osal_time_get_ms();
	test_sleep_ms(10);
	uint32_t t2 = eai_osal_time_get_ms();
	TEST_ASSERT_GREATER_THAN(t1, t2);
}

static void test_time_tick_roundtrip(void)
{
	uint64_t ticks = eai_osal_time_get_ticks();
	uint32_t ms_from_ticks = eai_osal_time_ticks_to_ms(ticks);
	uint32_t ms_direct = eai_osal_time_get_ms();

	int32_t diff = (int32_t)ms_direct - (int32_t)ms_from_ticks;
	TEST_ASSERT_INT_WITHIN(10, 0, diff);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Work queue tests (9)
 * ═══════════════════════════════════════════════════════════════════════════ */

static eai_osal_sem_t work_sem;
static volatile int work_counter;

static void work_callback(void *arg)
{
	(void)arg;
	__atomic_fetch_add(&work_counter, 1, __ATOMIC_SEQ_CST);
	eai_osal_sem_give(&work_sem);
}

static void test_work_init(void)
{
	eai_osal_work_t work;
	TEST_ASSERT_EQUAL(EAI_OSAL_OK, eai_osal_work_init(&work, work_callback, NULL));
}

static void test_work_init_null(void)
{
	eai_osal_work_t work;
	TEST_ASSERT_EQUAL(EAI_OSAL_INVALID_PARAM,
			  eai_osal_work_init(NULL, work_callback, NULL));
	TEST_ASSERT_EQUAL(EAI_OSAL_INVALID_PARAM,
			  eai_osal_work_init(&work, NULL, NULL));
}

static void test_work_submit(void)
{
	eai_osal_work_t work;
	work_counter = 0;
	eai_osal_sem_create(&work_sem, 0, 1);

	eai_osal_work_init(&work, work_callback, NULL);
	TEST_ASSERT_EQUAL(EAI_OSAL_OK, eai_osal_work_submit(&work));

	eai_osal_sem_take(&work_sem, 500);
	TEST_ASSERT_EQUAL(1, work_counter);

	eai_osal_sem_destroy(&work_sem);
}

static volatile int work_arg_val;

static void work_with_arg(void *arg)
{
	work_arg_val = *(int *)arg;
	eai_osal_sem_give(&work_sem);
}

static void test_work_arg_passthrough(void)
{
	eai_osal_work_t work;
	static int my_arg = 77;

	work_arg_val = 0;
	eai_osal_sem_create(&work_sem, 0, 1);

	eai_osal_work_init(&work, work_with_arg, &my_arg);
	eai_osal_work_submit(&work);

	eai_osal_sem_take(&work_sem, 500);
	TEST_ASSERT_EQUAL(77, work_arg_val);

	eai_osal_sem_destroy(&work_sem);
}

static void test_dwork_init(void)
{
	eai_osal_dwork_t dwork;
	TEST_ASSERT_EQUAL(EAI_OSAL_OK, eai_osal_dwork_init(&dwork, work_callback, NULL));
}

static void test_dwork_submit(void)
{
	eai_osal_dwork_t dwork;
	work_counter = 0;
	eai_osal_sem_create(&work_sem, 0, 1);

	eai_osal_dwork_init(&dwork, work_callback, NULL);

	uint32_t start = eai_osal_time_get_ms();
	TEST_ASSERT_EQUAL(EAI_OSAL_OK, eai_osal_dwork_submit(&dwork, 100));

	eai_osal_sem_take(&work_sem, 500);
	uint32_t elapsed = eai_osal_time_get_ms() - start;

	TEST_ASSERT_EQUAL(1, work_counter);
	TEST_ASSERT_GREATER_OR_EQUAL(90, elapsed);

	eai_osal_sem_destroy(&work_sem);
}

static void test_dwork_cancel(void)
{
	eai_osal_dwork_t dwork;
	work_counter = 0;
	eai_osal_sem_create(&work_sem, 0, 1);

	eai_osal_dwork_init(&dwork, work_callback, NULL);
	eai_osal_dwork_submit(&dwork, 200);

	test_sleep_ms(50);
	TEST_ASSERT_EQUAL(EAI_OSAL_OK, eai_osal_dwork_cancel(&dwork));

	test_sleep_ms(300);
	TEST_ASSERT_EQUAL(0, work_counter);

	eai_osal_sem_destroy(&work_sem);
}

/* Custom work queue — static, persists across tests */
EAI_OSAL_THREAD_STACK_DEFINE(custom_wq_stack, 2048);
static eai_osal_workqueue_t test_wq;
static bool test_wq_started;

static void ensure_test_wq(void)
{
	if (!test_wq_started) {
		eai_osal_workqueue_create(&test_wq, "test_wq",
					  custom_wq_stack,
					  EAI_OSAL_THREAD_STACK_SIZEOF(custom_wq_stack),
					  10);
		test_wq_started = true;
	}
}

static void test_custom_workqueue(void)
{
	eai_osal_work_t work;
	work_counter = 0;
	eai_osal_sem_create(&work_sem, 0, 1);

	ensure_test_wq();

	eai_osal_work_init(&work, work_callback, NULL);
	TEST_ASSERT_EQUAL(EAI_OSAL_OK, eai_osal_work_submit_to(&work, &test_wq));

	eai_osal_sem_take(&work_sem, 500);
	TEST_ASSERT_EQUAL(1, work_counter);

	eai_osal_sem_destroy(&work_sem);
}

static void test_dwork_submit_to_queue(void)
{
	eai_osal_dwork_t dwork;
	work_counter = 0;
	eai_osal_sem_create(&work_sem, 0, 1);

	ensure_test_wq();

	eai_osal_dwork_init(&dwork, work_callback, NULL);
	TEST_ASSERT_EQUAL(EAI_OSAL_OK, eai_osal_dwork_submit_to(&dwork, &test_wq, 50));

	eai_osal_sem_take(&work_sem, 500);
	TEST_ASSERT_EQUAL(1, work_counter);

	eai_osal_sem_destroy(&work_sem);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test runner
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(void)
{
	UNITY_BEGIN();

	/* Mutex (6) */
	RUN_TEST(test_mutex_create_destroy);
	RUN_TEST(test_mutex_lock_unlock);
	RUN_TEST(test_mutex_recursive_lock);
	RUN_TEST(test_mutex_try_lock);
	RUN_TEST(test_mutex_contention_timeout);
	RUN_TEST(test_mutex_null_param);

	/* Semaphore (5) */
	RUN_TEST(test_sem_create_destroy);
	RUN_TEST(test_sem_binary);
	RUN_TEST(test_sem_counting);
	RUN_TEST(test_sem_timeout);
	RUN_TEST(test_sem_give_at_limit);

	/* Thread (4) */
	RUN_TEST(test_thread_create_join);
	RUN_TEST(test_thread_sleep);
	RUN_TEST(test_thread_yield);
	RUN_TEST(test_thread_priority);

	/* Queue (5) */
	RUN_TEST(test_queue_create_destroy);
	RUN_TEST(test_queue_send_recv);
	RUN_TEST(test_queue_full);
	RUN_TEST(test_queue_fifo_order);
	RUN_TEST(test_queue_empty_timeout);

	/* Timer (5) */
	RUN_TEST(test_timer_create_destroy);
	RUN_TEST(test_timer_one_shot);
	RUN_TEST(test_timer_periodic);
	RUN_TEST(test_timer_stop);
	RUN_TEST(test_timer_is_running);

	/* Event (5) */
	RUN_TEST(test_event_create_destroy);
	RUN_TEST(test_event_set_wait_any);
	RUN_TEST(test_event_wait_all);
	RUN_TEST(test_event_clear);
	RUN_TEST(test_event_timeout);

	/* Critical (2) */
	RUN_TEST(test_critical_enter_exit);
	RUN_TEST(test_critical_nested);

	/* Time (3) */
	RUN_TEST(test_time_get_ms);
	RUN_TEST(test_time_monotonic);
	RUN_TEST(test_time_tick_roundtrip);

	/* Work (9) */
	RUN_TEST(test_work_init);
	RUN_TEST(test_work_init_null);
	RUN_TEST(test_work_submit);
	RUN_TEST(test_work_arg_passthrough);
	RUN_TEST(test_dwork_init);
	RUN_TEST(test_dwork_submit);
	RUN_TEST(test_dwork_cancel);
	RUN_TEST(test_custom_workqueue);
	RUN_TEST(test_dwork_submit_to_queue);

	return UNITY_END();
}
