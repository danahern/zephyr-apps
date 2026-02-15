#include <eai_osal/workqueue.h>
#include "internal.h"

/* ── Work item trampoline ──────────────────────────────────────────────── */

static void work_trampoline(struct k_work *zwork)
{
	eai_osal_work_t *work = CONTAINER_OF(zwork, eai_osal_work_t, _impl);

	if (work->_cb != NULL) {
		work->_cb(work->_cb_arg);
	}
}

eai_osal_status_t eai_osal_work_init(eai_osal_work_t *work,
				     eai_osal_work_cb_t callback,
				     void *arg)
{
	if (work == NULL || callback == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	work->_cb = callback;
	work->_cb_arg = arg;
	k_work_init(&work->_impl, work_trampoline);
	return EAI_OSAL_OK;
}

eai_osal_status_t eai_osal_work_submit(eai_osal_work_t *work)
{
	if (work == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	int ret = k_work_submit(&work->_impl);

	/* k_work_submit returns: 1 = queued, 2 = running/queued, 0 = already queued */
	return ret >= 0 ? EAI_OSAL_OK : EAI_OSAL_ERROR;
}

eai_osal_status_t eai_osal_work_submit_to(eai_osal_work_t *work,
					  eai_osal_workqueue_t *wq)
{
	if (work == NULL || wq == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	int ret = k_work_submit_to_queue(&wq->_impl, &work->_impl);

	return ret >= 0 ? EAI_OSAL_OK : EAI_OSAL_ERROR;
}

/* ── Delayed work trampoline ───────────────────────────────────────────── */

static void dwork_trampoline(struct k_work *zwork)
{
	struct k_work_delayable *zdwork = CONTAINER_OF(zwork,
						       struct k_work_delayable,
						       work);
	eai_osal_dwork_t *dwork = CONTAINER_OF(zdwork, eai_osal_dwork_t, _impl);

	if (dwork->_cb != NULL) {
		dwork->_cb(dwork->_cb_arg);
	}
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
	k_work_init_delayable(&dwork->_impl, dwork_trampoline);
	return EAI_OSAL_OK;
}

eai_osal_status_t eai_osal_dwork_submit(eai_osal_dwork_t *dwork,
					uint32_t delay_ms)
{
	if (dwork == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	int ret = k_work_schedule(&dwork->_impl, K_MSEC(delay_ms));

	return ret >= 0 ? EAI_OSAL_OK : EAI_OSAL_ERROR;
}

eai_osal_status_t eai_osal_dwork_submit_to(eai_osal_dwork_t *dwork,
					   eai_osal_workqueue_t *wq,
					   uint32_t delay_ms)
{
	if (dwork == NULL || wq == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	int ret = k_work_schedule_for_queue(&wq->_impl, &dwork->_impl,
					    K_MSEC(delay_ms));

	return ret >= 0 ? EAI_OSAL_OK : EAI_OSAL_ERROR;
}

eai_osal_status_t eai_osal_dwork_cancel(eai_osal_dwork_t *dwork)
{
	if (dwork == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	int ret = k_work_cancel_delayable(&dwork->_impl);

	/* Returns 0 if cancelled, negative if not pending */
	return ret >= 0 ? EAI_OSAL_OK : EAI_OSAL_ERROR;
}

/* ── Custom work queue ─────────────────────────────────────────────────── */

eai_osal_status_t eai_osal_workqueue_create(eai_osal_workqueue_t *wq,
					    const char *name,
					    void *stack,
					    size_t stack_size,
					    uint8_t priority)
{
	if (wq == NULL || stack == NULL || stack_size == 0) {
		return EAI_OSAL_INVALID_PARAM;
	}

	int zephyr_prio = 31 - (priority > 31 ? 31 : priority);

	struct k_work_queue_config cfg = {
		.name = name,
	};

	k_work_queue_start(&wq->_impl, (k_thread_stack_t *)stack, stack_size,
			   zephyr_prio, &cfg);

	return EAI_OSAL_OK;
}
