#include <zephyr/ztest.h>
#include <zephyr/kernel.h>
#include <eai_osal/eai_osal.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Mutex tests
 * ═══════════════════════════════════════════════════════════════════════════ */

ZTEST_SUITE(osal_mutex, NULL, NULL, NULL, NULL, NULL);

ZTEST(osal_mutex, test_create_destroy)
{
	eai_osal_mutex_t mtx;

	zassert_equal(eai_osal_mutex_create(&mtx), EAI_OSAL_OK);
	zassert_equal(eai_osal_mutex_destroy(&mtx), EAI_OSAL_OK);
}

ZTEST(osal_mutex, test_lock_unlock)
{
	eai_osal_mutex_t mtx;

	eai_osal_mutex_create(&mtx);
	zassert_equal(eai_osal_mutex_lock(&mtx, EAI_OSAL_WAIT_FOREVER), EAI_OSAL_OK);
	zassert_equal(eai_osal_mutex_unlock(&mtx), EAI_OSAL_OK);
	eai_osal_mutex_destroy(&mtx);
}

ZTEST(osal_mutex, test_recursive_lock)
{
	eai_osal_mutex_t mtx;

	eai_osal_mutex_create(&mtx);
	zassert_equal(eai_osal_mutex_lock(&mtx, EAI_OSAL_WAIT_FOREVER), EAI_OSAL_OK);
	zassert_equal(eai_osal_mutex_lock(&mtx, EAI_OSAL_WAIT_FOREVER), EAI_OSAL_OK);
	zassert_equal(eai_osal_mutex_unlock(&mtx), EAI_OSAL_OK);
	zassert_equal(eai_osal_mutex_unlock(&mtx), EAI_OSAL_OK);
	eai_osal_mutex_destroy(&mtx);
}

ZTEST(osal_mutex, test_try_lock)
{
	eai_osal_mutex_t mtx;

	eai_osal_mutex_create(&mtx);
	zassert_equal(eai_osal_mutex_lock(&mtx, EAI_OSAL_NO_WAIT), EAI_OSAL_OK);
	zassert_equal(eai_osal_mutex_unlock(&mtx), EAI_OSAL_OK);
	eai_osal_mutex_destroy(&mtx);
}

/* Helper for contention test — uses raw Zephyr thread to avoid testing OSAL
 * with itself */
static eai_osal_mutex_t contention_mtx;
static K_THREAD_STACK_DEFINE(mtx_helper_stack, 1024);
static struct k_thread mtx_helper_thread;

static void mutex_holder_entry(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	eai_osal_mutex_lock(&contention_mtx, EAI_OSAL_WAIT_FOREVER);
	k_msleep(200);
	eai_osal_mutex_unlock(&contention_mtx);
}

ZTEST(osal_mutex, test_contention_timeout)
{
	eai_osal_mutex_create(&contention_mtx);

	k_thread_create(&mtx_helper_thread, mtx_helper_stack,
			K_THREAD_STACK_SIZEOF(mtx_helper_stack),
			mutex_holder_entry, NULL, NULL, NULL,
			5, 0, K_NO_WAIT);

	k_msleep(10); /* Let helper acquire the mutex */

	/* Try to lock with short timeout — should fail */
	zassert_equal(eai_osal_mutex_lock(&contention_mtx, 50), EAI_OSAL_TIMEOUT);

	k_thread_join(&mtx_helper_thread, K_MSEC(500));
	eai_osal_mutex_destroy(&contention_mtx);
}

ZTEST(osal_mutex, test_null_param)
{
	zassert_equal(eai_osal_mutex_create(NULL), EAI_OSAL_INVALID_PARAM);
	zassert_equal(eai_osal_mutex_lock(NULL, 0), EAI_OSAL_INVALID_PARAM);
	zassert_equal(eai_osal_mutex_unlock(NULL), EAI_OSAL_INVALID_PARAM);
	zassert_equal(eai_osal_mutex_destroy(NULL), EAI_OSAL_INVALID_PARAM);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Semaphore tests
 * ═══════════════════════════════════════════════════════════════════════════ */

ZTEST_SUITE(osal_sem, NULL, NULL, NULL, NULL, NULL);

ZTEST(osal_sem, test_create_destroy)
{
	eai_osal_sem_t sem;

	zassert_equal(eai_osal_sem_create(&sem, 0, 1), EAI_OSAL_OK);
	zassert_equal(eai_osal_sem_destroy(&sem), EAI_OSAL_OK);
}

ZTEST(osal_sem, test_binary)
{
	eai_osal_sem_t sem;

	eai_osal_sem_create(&sem, 0, 1);
	/* Can't take from empty */
	zassert_equal(eai_osal_sem_take(&sem, EAI_OSAL_NO_WAIT), EAI_OSAL_TIMEOUT);
	/* Give, then take */
	zassert_equal(eai_osal_sem_give(&sem), EAI_OSAL_OK);
	zassert_equal(eai_osal_sem_take(&sem, EAI_OSAL_NO_WAIT), EAI_OSAL_OK);
	/* Empty again */
	zassert_equal(eai_osal_sem_take(&sem, EAI_OSAL_NO_WAIT), EAI_OSAL_TIMEOUT);
	eai_osal_sem_destroy(&sem);
}

ZTEST(osal_sem, test_counting)
{
	eai_osal_sem_t sem;

	eai_osal_sem_create(&sem, 0, 5);
	for (int i = 0; i < 3; i++) {
		eai_osal_sem_give(&sem);
	}
	for (int i = 0; i < 3; i++) {
		zassert_equal(eai_osal_sem_take(&sem, EAI_OSAL_NO_WAIT), EAI_OSAL_OK,
			      "Take %d should succeed", i);
	}
	zassert_equal(eai_osal_sem_take(&sem, EAI_OSAL_NO_WAIT), EAI_OSAL_TIMEOUT);
	eai_osal_sem_destroy(&sem);
}

ZTEST(osal_sem, test_timeout)
{
	eai_osal_sem_t sem;

	eai_osal_sem_create(&sem, 0, 1);
	zassert_equal(eai_osal_sem_take(&sem, 50), EAI_OSAL_TIMEOUT);
	eai_osal_sem_destroy(&sem);
}

ZTEST(osal_sem, test_give_at_limit)
{
	eai_osal_sem_t sem;

	eai_osal_sem_create(&sem, 1, 1); /* Already at limit */
	eai_osal_sem_give(&sem);         /* Should not exceed limit */

	zassert_equal(eai_osal_sem_take(&sem, EAI_OSAL_NO_WAIT), EAI_OSAL_OK);
	zassert_equal(eai_osal_sem_take(&sem, EAI_OSAL_NO_WAIT), EAI_OSAL_TIMEOUT);
	eai_osal_sem_destroy(&sem);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Thread tests
 * ═══════════════════════════════════════════════════════════════════════════ */

ZTEST_SUITE(osal_thread, NULL, NULL, NULL, NULL, NULL);

static volatile int thread_counter;

static void simple_thread_entry(void *arg)
{
	int *val = (int *)arg;

	thread_counter = *val;
}

EAI_OSAL_THREAD_STACK_DEFINE(test_thread_stack, 1024);

ZTEST(osal_thread, test_create_join)
{
	eai_osal_thread_t thread;
	int arg = 42;

	thread_counter = 0;
	zassert_equal(eai_osal_thread_create(&thread, "test",
					     simple_thread_entry, &arg,
					     test_thread_stack,
					     EAI_OSAL_THREAD_STACK_SIZEOF(test_thread_stack),
					     10),
		      EAI_OSAL_OK);
	zassert_equal(eai_osal_thread_join(&thread, EAI_OSAL_WAIT_FOREVER), EAI_OSAL_OK);
	zassert_equal(thread_counter, 42);
}

ZTEST(osal_thread, test_sleep)
{
	uint32_t start = eai_osal_time_get_ms();

	eai_osal_thread_sleep(100);

	uint32_t elapsed = eai_osal_time_get_ms() - start;

	zassert_true(elapsed >= 90, "Sleep too short: %u ms", elapsed);
	zassert_true(elapsed <= 200, "Sleep too long: %u ms", elapsed);
}

ZTEST(osal_thread, test_yield)
{
	/* Should not hang or crash */
	eai_osal_thread_yield();
}

static volatile int prio_order[2];
static volatile int prio_idx;

static void prio_thread_entry(void *arg)
{
	int id = (int)(intptr_t)arg;

	prio_order[prio_idx++] = id;
}

EAI_OSAL_THREAD_STACK_DEFINE(prio_stack_a, 512);
EAI_OSAL_THREAD_STACK_DEFINE(prio_stack_b, 512);

ZTEST(osal_thread, test_priority)
{
	eai_osal_thread_t thread_a, thread_b;

	prio_idx = 0;

	/* Create low-priority thread first (OSAL 5 → Zephyr 26) */
	eai_osal_thread_create(&thread_a, "lo", prio_thread_entry,
			       (void *)(intptr_t)0,
			       prio_stack_a,
			       EAI_OSAL_THREAD_STACK_SIZEOF(prio_stack_a), 5);

	/* Create high-priority thread second (OSAL 20 → Zephyr 11) */
	eai_osal_thread_create(&thread_b, "hi", prio_thread_entry,
			       (void *)(intptr_t)1,
			       prio_stack_b,
			       EAI_OSAL_THREAD_STACK_SIZEOF(prio_stack_b), 20);

	/* Let both threads run — higher OSAL prio should run first */
	k_msleep(50);

	eai_osal_thread_join(&thread_a, EAI_OSAL_WAIT_FOREVER);
	eai_osal_thread_join(&thread_b, EAI_OSAL_WAIT_FOREVER);

	zassert_equal(prio_order[0], 1, "Higher priority thread should run first");
	zassert_equal(prio_order[1], 0, "Lower priority thread should run second");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Queue tests
 * ═══════════════════════════════════════════════════════════════════════════ */

ZTEST_SUITE(osal_queue, NULL, NULL, NULL, NULL, NULL);

static uint8_t __aligned(4) queue_buf_4[4 * sizeof(int)];
static uint8_t __aligned(4) queue_buf_2[2 * sizeof(int)];

ZTEST(osal_queue, test_create_destroy)
{
	eai_osal_queue_t queue;

	zassert_equal(eai_osal_queue_create(&queue, sizeof(int), 4, queue_buf_4),
		      EAI_OSAL_OK);
	zassert_equal(eai_osal_queue_destroy(&queue), EAI_OSAL_OK);
}

ZTEST(osal_queue, test_send_recv)
{
	eai_osal_queue_t queue;
	int msg;

	eai_osal_queue_create(&queue, sizeof(int), 4, queue_buf_4);

	msg = 99;
	zassert_equal(eai_osal_queue_send(&queue, &msg, EAI_OSAL_NO_WAIT), EAI_OSAL_OK);

	msg = 0;
	zassert_equal(eai_osal_queue_recv(&queue, &msg, EAI_OSAL_NO_WAIT), EAI_OSAL_OK);
	zassert_equal(msg, 99);

	eai_osal_queue_destroy(&queue);
}

ZTEST(osal_queue, test_full_queue)
{
	eai_osal_queue_t queue;
	int msg = 42;

	eai_osal_queue_create(&queue, sizeof(int), 2, queue_buf_2);

	zassert_equal(eai_osal_queue_send(&queue, &msg, EAI_OSAL_NO_WAIT), EAI_OSAL_OK);
	zassert_equal(eai_osal_queue_send(&queue, &msg, EAI_OSAL_NO_WAIT), EAI_OSAL_OK);
	/* Queue full */
	zassert_equal(eai_osal_queue_send(&queue, &msg, EAI_OSAL_NO_WAIT),
		      EAI_OSAL_TIMEOUT);

	eai_osal_queue_destroy(&queue);
}

ZTEST(osal_queue, test_fifo_order)
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
		zassert_equal(msg, vals[i], "FIFO: expected %d at pos %d, got %d",
			      vals[i], i, msg);
	}

	eai_osal_queue_destroy(&queue);
}

ZTEST(osal_queue, test_empty_timeout)
{
	eai_osal_queue_t queue;
	int msg;

	eai_osal_queue_create(&queue, sizeof(int), 2, queue_buf_2);
	zassert_equal(eai_osal_queue_recv(&queue, &msg, EAI_OSAL_NO_WAIT),
		      EAI_OSAL_TIMEOUT);
	eai_osal_queue_destroy(&queue);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Timer tests
 * ═══════════════════════════════════════════════════════════════════════════ */

ZTEST_SUITE(osal_timer, NULL, NULL, NULL, NULL, NULL);

static struct k_sem timer_sem;
static volatile int timer_count;

static void timer_callback(void *arg)
{
	ARG_UNUSED(arg);
	timer_count++;
	k_sem_give(&timer_sem);
}

ZTEST(osal_timer, test_create_destroy)
{
	eai_osal_timer_t timer;

	zassert_equal(eai_osal_timer_create(&timer, timer_callback, NULL), EAI_OSAL_OK);
	zassert_equal(eai_osal_timer_destroy(&timer), EAI_OSAL_OK);
}

ZTEST(osal_timer, test_one_shot)
{
	eai_osal_timer_t timer;

	timer_count = 0;
	k_sem_init(&timer_sem, 0, 1);

	eai_osal_timer_create(&timer, timer_callback, NULL);
	eai_osal_timer_start(&timer, 50, 0);

	k_sem_take(&timer_sem, K_MSEC(200));
	zassert_equal(timer_count, 1);

	/* Wait to verify it doesn't fire again */
	k_msleep(100);
	zassert_equal(timer_count, 1);

	eai_osal_timer_destroy(&timer);
}

ZTEST(osal_timer, test_periodic)
{
	eai_osal_timer_t timer;

	timer_count = 0;
	k_sem_init(&timer_sem, 0, 10);

	eai_osal_timer_create(&timer, timer_callback, NULL);
	eai_osal_timer_start(&timer, 50, 50);

	k_msleep(275);
	eai_osal_timer_stop(&timer);

	zassert_true(timer_count >= 4, "Expected >= 4 callbacks, got %d", timer_count);
	zassert_true(timer_count <= 6, "Expected <= 6 callbacks, got %d", timer_count);

	eai_osal_timer_destroy(&timer);
}

ZTEST(osal_timer, test_stop)
{
	eai_osal_timer_t timer;

	timer_count = 0;
	k_sem_init(&timer_sem, 0, 10);

	eai_osal_timer_create(&timer, timer_callback, NULL);
	eai_osal_timer_start(&timer, 50, 50);

	k_msleep(75);
	eai_osal_timer_stop(&timer);
	int count_at_stop = timer_count;

	k_msleep(200);
	zassert_equal(timer_count, count_at_stop,
		      "Timer should not fire after stop");

	eai_osal_timer_destroy(&timer);
}

ZTEST(osal_timer, test_is_running)
{
	eai_osal_timer_t timer;

	eai_osal_timer_create(&timer, timer_callback, NULL);
	zassert_false(eai_osal_timer_is_running(&timer));

	eai_osal_timer_start(&timer, 500, 0);
	zassert_true(eai_osal_timer_is_running(&timer));

	eai_osal_timer_stop(&timer);
	zassert_false(eai_osal_timer_is_running(&timer));

	eai_osal_timer_destroy(&timer);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Event tests
 * ═══════════════════════════════════════════════════════════════════════════ */

ZTEST_SUITE(osal_event, NULL, NULL, NULL, NULL, NULL);

ZTEST(osal_event, test_create_destroy)
{
	eai_osal_event_t event;

	zassert_equal(eai_osal_event_create(&event), EAI_OSAL_OK);
	zassert_equal(eai_osal_event_destroy(&event), EAI_OSAL_OK);
}

ZTEST(osal_event, test_set_wait_any)
{
	eai_osal_event_t event;
	uint32_t actual = 0;

	eai_osal_event_create(&event);
	eai_osal_event_set(&event, 0x03);

	zassert_equal(eai_osal_event_wait(&event, 0x0F, false, &actual, EAI_OSAL_NO_WAIT),
		      EAI_OSAL_OK);
	zassert_true((actual & 0x03) != 0, "Expected bits 0 or 1 set");

	eai_osal_event_destroy(&event);
}

ZTEST(osal_event, test_wait_all)
{
	eai_osal_event_t event;
	uint32_t actual = 0;

	eai_osal_event_create(&event);

	/* Set only bit 0 */
	eai_osal_event_set(&event, 0x01);

	/* Wait for bits 0 AND 1 — should timeout */
	zassert_equal(eai_osal_event_wait(&event, 0x03, true, &actual, EAI_OSAL_NO_WAIT),
		      EAI_OSAL_TIMEOUT);

	/* Set bit 1 too */
	eai_osal_event_set(&event, 0x02);

	/* Now both bits are set — should succeed */
	zassert_equal(eai_osal_event_wait(&event, 0x03, true, &actual, EAI_OSAL_NO_WAIT),
		      EAI_OSAL_OK);
	zassert_equal(actual & 0x03, 0x03);

	eai_osal_event_destroy(&event);
}

ZTEST(osal_event, test_clear)
{
	eai_osal_event_t event;
	uint32_t actual = 0;

	eai_osal_event_create(&event);
	eai_osal_event_set(&event, 0x07); /* bits 0,1,2 */
	eai_osal_event_clear(&event, 0x02); /* clear bit 1 */

	/* Bit 1 is cleared — wait for it should timeout */
	zassert_equal(eai_osal_event_wait(&event, 0x02, false, &actual, EAI_OSAL_NO_WAIT),
		      EAI_OSAL_TIMEOUT);

	/* Bit 0 is still set */
	zassert_equal(eai_osal_event_wait(&event, 0x01, false, &actual, EAI_OSAL_NO_WAIT),
		      EAI_OSAL_OK);

	eai_osal_event_destroy(&event);
}

ZTEST(osal_event, test_timeout)
{
	eai_osal_event_t event;
	uint32_t actual = 0;

	eai_osal_event_create(&event);
	zassert_equal(eai_osal_event_wait(&event, 0xFF, false, &actual, 50),
		      EAI_OSAL_TIMEOUT);
	eai_osal_event_destroy(&event);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Critical section tests
 * ═══════════════════════════════════════════════════════════════════════════ */

ZTEST_SUITE(osal_critical, NULL, NULL, NULL, NULL, NULL);

ZTEST(osal_critical, test_enter_exit)
{
	volatile int shared = 0;

	eai_osal_critical_key_t key = eai_osal_critical_enter();
	shared = 42;
	eai_osal_critical_exit(key);

	zassert_equal(shared, 42);
}

ZTEST(osal_critical, test_nested)
{
	eai_osal_critical_key_t key1 = eai_osal_critical_enter();
	eai_osal_critical_key_t key2 = eai_osal_critical_enter();

	/* Both should succeed without deadlock */
	eai_osal_critical_exit(key2);
	eai_osal_critical_exit(key1);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Time tests
 * ═══════════════════════════════════════════════════════════════════════════ */

ZTEST_SUITE(osal_time, NULL, NULL, NULL, NULL, NULL);

ZTEST(osal_time, test_get_ms)
{
	uint32_t ms = eai_osal_time_get_ms();

	/* Should return some value (system has been running for a bit) */
	zassert_true(ms >= 0, "get_ms should not fail");
}

ZTEST(osal_time, test_monotonic)
{
	uint32_t t1 = eai_osal_time_get_ms();

	k_msleep(10);

	uint32_t t2 = eai_osal_time_get_ms();

	zassert_true(t2 > t1, "Time should be monotonic: t1=%u t2=%u", t1, t2);
}

ZTEST(osal_time, test_tick_roundtrip)
{
	uint64_t ticks = eai_osal_time_get_ticks();
	uint32_t ms_from_ticks = eai_osal_time_ticks_to_ms(ticks);
	uint32_t ms_direct = eai_osal_time_get_ms();

	/* Should be close (within 10ms) */
	int32_t diff = (int32_t)ms_direct - (int32_t)ms_from_ticks;

	zassert_true(diff >= -10 && diff <= 10,
		     "Tick roundtrip off by %d ms", diff);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Work queue tests
 * ═══════════════════════════════════════════════════════════════════════════ */

ZTEST_SUITE(osal_work, NULL, NULL, NULL, NULL, NULL);

static struct k_sem work_sem;
static volatile int work_counter;

static void work_callback(void *arg)
{
	ARG_UNUSED(arg);
	work_counter++;
	k_sem_give(&work_sem);
}

ZTEST(osal_work, test_work_init)
{
	eai_osal_work_t work;

	zassert_equal(eai_osal_work_init(&work, work_callback, NULL), EAI_OSAL_OK);
}

ZTEST(osal_work, test_work_init_null)
{
	eai_osal_work_t work;

	zassert_equal(eai_osal_work_init(NULL, work_callback, NULL),
		      EAI_OSAL_INVALID_PARAM);
	zassert_equal(eai_osal_work_init(&work, NULL, NULL),
		      EAI_OSAL_INVALID_PARAM);
}

ZTEST(osal_work, test_work_submit)
{
	eai_osal_work_t work;

	work_counter = 0;
	k_sem_init(&work_sem, 0, 1);

	eai_osal_work_init(&work, work_callback, NULL);
	zassert_equal(eai_osal_work_submit(&work), EAI_OSAL_OK);

	k_sem_take(&work_sem, K_MSEC(500));
	zassert_equal(work_counter, 1, "Work callback should have executed");
}

static volatile int work_arg_val;

static void work_with_arg(void *arg)
{
	work_arg_val = *(int *)arg;
	k_sem_give(&work_sem);
}

ZTEST(osal_work, test_work_arg_passthrough)
{
	eai_osal_work_t work;
	static int my_arg = 77;

	work_arg_val = 0;
	k_sem_init(&work_sem, 0, 1);

	eai_osal_work_init(&work, work_with_arg, &my_arg);
	eai_osal_work_submit(&work);

	k_sem_take(&work_sem, K_MSEC(500));
	zassert_equal(work_arg_val, 77, "Arg passthrough failed");
}

ZTEST(osal_work, test_dwork_init)
{
	eai_osal_dwork_t dwork;

	zassert_equal(eai_osal_dwork_init(&dwork, work_callback, NULL), EAI_OSAL_OK);
}

ZTEST(osal_work, test_dwork_submit)
{
	eai_osal_dwork_t dwork;

	work_counter = 0;
	k_sem_init(&work_sem, 0, 1);

	eai_osal_dwork_init(&dwork, work_callback, NULL);

	uint32_t start = eai_osal_time_get_ms();

	zassert_equal(eai_osal_dwork_submit(&dwork, 100), EAI_OSAL_OK);

	k_sem_take(&work_sem, K_MSEC(500));

	uint32_t elapsed = eai_osal_time_get_ms() - start;

	zassert_equal(work_counter, 1, "Delayed work should have executed");
	zassert_true(elapsed >= 90, "Delayed work too early: %u ms", elapsed);
}

ZTEST(osal_work, test_dwork_cancel)
{
	eai_osal_dwork_t dwork;

	work_counter = 0;
	k_sem_init(&work_sem, 0, 1);

	eai_osal_dwork_init(&dwork, work_callback, NULL);
	eai_osal_dwork_submit(&dwork, 200);

	/* Cancel before it fires */
	k_msleep(50);
	zassert_equal(eai_osal_dwork_cancel(&dwork), EAI_OSAL_OK);

	/* Wait to verify it doesn't execute */
	k_msleep(300);
	zassert_equal(work_counter, 0, "Cancelled work should not execute");
}

/* Static workqueue — must outlive test functions since the thread persists */
EAI_OSAL_THREAD_STACK_DEFINE(custom_wq_stack, 1024);
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

ZTEST(osal_work, test_custom_workqueue)
{
	eai_osal_work_t work;

	work_counter = 0;
	k_sem_init(&work_sem, 0, 1);

	ensure_test_wq();

	eai_osal_work_init(&work, work_callback, NULL);
	zassert_equal(eai_osal_work_submit_to(&work, &test_wq), EAI_OSAL_OK);

	k_sem_take(&work_sem, K_MSEC(500));
	zassert_equal(work_counter, 1, "Work on custom queue should execute");
}

ZTEST(osal_work, test_dwork_submit_to_queue)
{
	eai_osal_dwork_t dwork;

	work_counter = 0;
	k_sem_init(&work_sem, 0, 1);

	ensure_test_wq();

	eai_osal_dwork_init(&dwork, work_callback, NULL);
	zassert_equal(eai_osal_dwork_submit_to(&dwork, &test_wq, 50), EAI_OSAL_OK);

	k_sem_take(&work_sem, K_MSEC(500));
	zassert_equal(work_counter, 1, "Delayed work on custom queue should execute");
}
