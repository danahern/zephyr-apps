#include <eai_osal/workqueue.h>
#include <eai_osal/queue.h>
#include "internal.h"
#include <string.h>

/*
 * POSIX work queue implementation.
 *
 * Same pattern as FreeRTOS: a thread blocks on an internal OSAL queue,
 * processing {callback, arg} pairs. System work queue is lazily initialized.
 * Delayed work uses a timer thread that enqueues on expiry.
 */

#define WQ_DEPTH 16

struct wq_item {
	eai_osal_work_cb_t cb;
	void *arg;
};

/* ── System work queue (lazy init) ────────────────────────────────────── */

static eai_osal_workqueue_t sys_wq;
static bool sys_wq_ready;

static void *wq_task(void *arg)
{
	eai_osal_workqueue_t *wq = (eai_osal_workqueue_t *)arg;
	struct wq_item item;

	for (;;) {
		if (eai_osal_queue_recv(&wq->_queue, &item,
					EAI_OSAL_WAIT_FOREVER) == EAI_OSAL_OK) {
			if (item.cb != NULL) {
				item.cb(item.arg);
			}
		}
	}
	return NULL;
}

static eai_osal_workqueue_t *get_sys_wq(void)
{
	if (!sys_wq_ready) {
		eai_osal_status_t ret = eai_osal_queue_create(
			&sys_wq._queue, sizeof(struct wq_item),
			WQ_DEPTH, sys_wq._buf);
		if (ret != EAI_OSAL_OK) {
			return NULL;
		}

		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_attr_setstacksize(&attr, 65536);

		int rc = pthread_create(&sys_wq._thread, &attr, wq_task, &sys_wq);
		pthread_attr_destroy(&attr);

		if (rc != 0) {
			eai_osal_queue_destroy(&sys_wq._queue);
			return NULL;
		}
		sys_wq_ready = true;
	}
	return &sys_wq;
}

/* ── Work item ────────────────────────────────────────────────────────── */

eai_osal_status_t eai_osal_work_init(eai_osal_work_t *work,
				     eai_osal_work_cb_t callback,
				     void *arg)
{
	if (work == NULL || callback == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	work->_cb = callback;
	work->_cb_arg = arg;
	return EAI_OSAL_OK;
}

static eai_osal_status_t submit_to_queue(eai_osal_queue_t *q,
					 eai_osal_work_cb_t cb, void *arg)
{
	struct wq_item item = { .cb = cb, .arg = arg };

	return eai_osal_queue_send(q, &item, EAI_OSAL_NO_WAIT);
}

eai_osal_status_t eai_osal_work_submit(eai_osal_work_t *work)
{
	if (work == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	eai_osal_workqueue_t *wq = get_sys_wq();

	if (wq == NULL) {
		return EAI_OSAL_ERROR;
	}
	return submit_to_queue(&wq->_queue, work->_cb, work->_cb_arg);
}

eai_osal_status_t eai_osal_work_submit_to(eai_osal_work_t *work,
					  eai_osal_workqueue_t *wq)
{
	if (work == NULL || wq == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	return submit_to_queue(&wq->_queue, work->_cb, work->_cb_arg);
}

/* ── Delayed work ─────────────────────────────────────────────────────── */

static void *dwork_timer_thread(void *arg)
{
	eai_osal_dwork_t *dwork = (eai_osal_dwork_t *)arg;

	pthread_mutex_lock(&dwork->_lock);
	struct timespec ts = osal_timespec(dwork->_delay_ms);
	pthread_cond_timedwait(&dwork->_cond, &dwork->_lock, &ts);

	bool cancelled = dwork->_cancel;
	eai_osal_work_cb_t cb = dwork->_cb;
	void *cb_arg = dwork->_cb_arg;
	eai_osal_workqueue_t *wq = (eai_osal_workqueue_t *)dwork->_target_wq;
	dwork->_pending = false;
	dwork->_thread_alive = false;
	pthread_mutex_unlock(&dwork->_lock);

	if (!cancelled) {
		if (wq == NULL) {
			wq = get_sys_wq();
		}
		if (wq != NULL) {
			submit_to_queue(&wq->_queue, cb, cb_arg);
		}
	}

	return NULL;
}

eai_osal_status_t eai_osal_dwork_init(eai_osal_dwork_t *dwork,
				      eai_osal_work_cb_t callback,
				      void *arg)
{
	if (dwork == NULL || callback == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	dwork->_cb = callback;
	dwork->_cb_arg = arg;
	dwork->_target_wq = NULL;
	dwork->_pending = false;
	dwork->_cancel = false;
	dwork->_thread_alive = false;

	if (pthread_mutex_init(&dwork->_lock, NULL) != 0) {
		return EAI_OSAL_ERROR;
	}
	if (pthread_cond_init(&dwork->_cond, NULL) != 0) {
		pthread_mutex_destroy(&dwork->_lock);
		return EAI_OSAL_ERROR;
	}
	return EAI_OSAL_OK;
}

static eai_osal_status_t dwork_start(eai_osal_dwork_t *dwork, uint32_t delay_ms)
{
	pthread_mutex_lock(&dwork->_lock);

	/* If a timer thread is still alive from a previous submit, join it */
	if (dwork->_thread_alive) {
		pthread_t old = dwork->_timer_thread;
		dwork->_cancel = true;
		pthread_cond_signal(&dwork->_cond);
		pthread_mutex_unlock(&dwork->_lock);
		pthread_join(old, NULL);
		pthread_mutex_lock(&dwork->_lock);
	}

	dwork->_delay_ms = delay_ms;
	dwork->_pending = true;
	dwork->_cancel = false;

	pthread_t thread;
	int ret = pthread_create(&thread, NULL, dwork_timer_thread, dwork);
	if (ret != 0) {
		dwork->_pending = false;
		pthread_mutex_unlock(&dwork->_lock);
		return EAI_OSAL_ERROR;
	}
	dwork->_timer_thread = thread;
	dwork->_thread_alive = true;
	pthread_detach(thread);
	pthread_mutex_unlock(&dwork->_lock);
	return EAI_OSAL_OK;
}

eai_osal_status_t eai_osal_dwork_submit(eai_osal_dwork_t *dwork,
					uint32_t delay_ms)
{
	if (dwork == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	dwork->_target_wq = NULL;
	return dwork_start(dwork, delay_ms);
}

eai_osal_status_t eai_osal_dwork_submit_to(eai_osal_dwork_t *dwork,
					   eai_osal_workqueue_t *wq,
					   uint32_t delay_ms)
{
	if (dwork == NULL || wq == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	dwork->_target_wq = wq;
	return dwork_start(dwork, delay_ms);
}

eai_osal_status_t eai_osal_dwork_cancel(eai_osal_dwork_t *dwork)
{
	if (dwork == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	pthread_mutex_lock(&dwork->_lock);
	dwork->_cancel = true;
	pthread_cond_signal(&dwork->_cond);
	pthread_mutex_unlock(&dwork->_lock);
	return EAI_OSAL_OK;
}

/* ── Custom work queue ────────────────────────────────────────────────── */

eai_osal_status_t eai_osal_workqueue_create(eai_osal_workqueue_t *wq,
					    const char *name,
					    void *stack,
					    size_t stack_size,
					    uint8_t priority)
{
	(void)name;
	(void)priority;

	if (wq == NULL || stack == NULL || stack_size == 0) {
		return EAI_OSAL_INVALID_PARAM;
	}

	eai_osal_status_t ret = eai_osal_queue_create(
		&wq->_queue, sizeof(struct wq_item), WQ_DEPTH, wq->_buf);
	if (ret != EAI_OSAL_OK) {
		return ret;
	}

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, stack_size < 16384 ? 16384 : stack_size);

	int rc = pthread_create(&wq->_thread, &attr, wq_task, wq);
	pthread_attr_destroy(&attr);

	if (rc != 0) {
		eai_osal_queue_destroy(&wq->_queue);
		return EAI_OSAL_ERROR;
	}

	return EAI_OSAL_OK;
}
