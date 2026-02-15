#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <eai_osal/eai_osal.h>

LOG_MODULE_REGISTER(osal_demo, LOG_LEVEL_INF);

/* Shared state for producer/consumer demo */
static eai_osal_queue_t queue;
static uint8_t __aligned(4) queue_buf[8 * sizeof(uint32_t)];

static eai_osal_mutex_t stats_mtx;
static volatile uint32_t produced;
static volatile uint32_t consumed;

static eai_osal_event_t done_event;
#define EVT_PRODUCER_DONE BIT(0)
#define EVT_CONSUMER_DONE BIT(1)

/* Producer: sends 10 messages, one every 100ms */
EAI_OSAL_THREAD_STACK_DEFINE(producer_stack, 1024);
static eai_osal_thread_t producer_thread;

static void producer_entry(void *arg)
{
	ARG_UNUSED(arg);

	for (uint32_t i = 1; i <= 10; i++) {
		eai_osal_queue_send(&queue, &i, EAI_OSAL_WAIT_FOREVER);

		eai_osal_mutex_lock(&stats_mtx, EAI_OSAL_WAIT_FOREVER);
		produced++;
		eai_osal_mutex_unlock(&stats_mtx);

		eai_osal_thread_sleep(100);
	}

	LOG_INF("Producer done (%u sent)", produced);
	eai_osal_event_set(&done_event, EVT_PRODUCER_DONE);
}

/* Consumer: receives messages until producer signals done */
EAI_OSAL_THREAD_STACK_DEFINE(consumer_stack, 1024);
static eai_osal_thread_t consumer_thread;

static void consumer_entry(void *arg)
{
	ARG_UNUSED(arg);
	uint32_t val;

	while (true) {
		eai_osal_status_t ret = eai_osal_queue_recv(&queue, &val, 500);

		if (ret == EAI_OSAL_OK) {
			eai_osal_mutex_lock(&stats_mtx, EAI_OSAL_WAIT_FOREVER);
			consumed++;
			eai_osal_mutex_unlock(&stats_mtx);
		} else {
			/* Timeout — check if producer is done */
			uint32_t actual;

			if (eai_osal_event_wait(&done_event, EVT_PRODUCER_DONE,
						false, &actual,
						EAI_OSAL_NO_WAIT) == EAI_OSAL_OK) {
				break;
			}
		}
	}

	LOG_INF("Consumer done (%u received)", consumed);
	eai_osal_event_set(&done_event, EVT_CONSUMER_DONE);
}

/* Heartbeat timer — fires every 500ms */
static eai_osal_timer_t heartbeat;
static volatile uint32_t heartbeat_count;

static void heartbeat_cb(void *arg)
{
	ARG_UNUSED(arg);
	heartbeat_count++;
}

int main(void)
{
	uint32_t t_start = eai_osal_time_get_ms();

	LOG_INF("=== EAI OSAL Demo ===");

	/* Init primitives */
	eai_osal_queue_create(&queue, sizeof(uint32_t), 8, queue_buf);
	eai_osal_mutex_create(&stats_mtx);
	eai_osal_event_create(&done_event);
	eai_osal_timer_create(&heartbeat, heartbeat_cb, NULL);

	/* Start heartbeat timer */
	eai_osal_timer_start(&heartbeat, 500, 500);

	/* Critical section test */
	eai_osal_critical_key_t key = eai_osal_critical_enter();
	LOG_INF("Critical section: OK");
	eai_osal_critical_exit(key);

	/* Semaphore test */
	eai_osal_sem_t sem;

	eai_osal_sem_create(&sem, 0, 1);
	eai_osal_sem_give(&sem);
	eai_osal_status_t ret = eai_osal_sem_take(&sem, EAI_OSAL_NO_WAIT);

	LOG_INF("Semaphore: %s", ret == EAI_OSAL_OK ? "OK" : "FAIL");
	eai_osal_sem_destroy(&sem);

	/* Launch producer/consumer */
	LOG_INF("Starting producer/consumer...");
	eai_osal_thread_create(&producer_thread, "producer", producer_entry, NULL,
			       producer_stack,
			       EAI_OSAL_THREAD_STACK_SIZEOF(producer_stack), 10);
	eai_osal_thread_create(&consumer_thread, "consumer", consumer_entry, NULL,
			       consumer_stack,
			       EAI_OSAL_THREAD_STACK_SIZEOF(consumer_stack), 10);

	/* Wait for both to finish */
	uint32_t actual;

	eai_osal_event_wait(&done_event, EVT_PRODUCER_DONE | EVT_CONSUMER_DONE,
			    true, &actual, EAI_OSAL_WAIT_FOREVER);

	eai_osal_thread_join(&producer_thread, EAI_OSAL_WAIT_FOREVER);
	eai_osal_thread_join(&consumer_thread, EAI_OSAL_WAIT_FOREVER);

	/* Stop timer and report */
	eai_osal_timer_stop(&heartbeat);

	uint32_t elapsed = eai_osal_time_get_ms() - t_start;

	LOG_INF("=== Results ===");
	LOG_INF("Produced: %u, Consumed: %u", produced, consumed);
	LOG_INF("Heartbeats: %u", heartbeat_count);
	LOG_INF("Elapsed: %u ms", elapsed);
	LOG_INF("ALL OSAL PRIMITIVES OK");

	/* Cleanup */
	eai_osal_timer_destroy(&heartbeat);
	eai_osal_event_destroy(&done_event);
	eai_osal_mutex_destroy(&stats_mtx);
	eai_osal_queue_destroy(&queue);

	return 0;
}
