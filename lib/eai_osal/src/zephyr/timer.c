#include <eai_osal/timer.h>
#include "internal.h"

static void timer_trampoline(struct k_timer *ztimer)
{
	eai_osal_timer_t *timer = CONTAINER_OF(ztimer, eai_osal_timer_t, _impl);

	if (timer->_cb != NULL) {
		timer->_cb(timer->_cb_arg);
	}
}

eai_osal_status_t eai_osal_timer_create(eai_osal_timer_t *timer,
					eai_osal_timer_cb_t callback,
					void *arg)
{
	if (timer == NULL || callback == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	timer->_cb = callback;
	timer->_cb_arg = arg;
	k_timer_init(&timer->_impl, timer_trampoline, NULL);
	return EAI_OSAL_OK;
}

eai_osal_status_t eai_osal_timer_destroy(eai_osal_timer_t *timer)
{
	if (timer == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	k_timer_stop(&timer->_impl);
	return EAI_OSAL_OK;
}

eai_osal_status_t eai_osal_timer_start(eai_osal_timer_t *timer,
				       uint32_t initial_ms,
				       uint32_t period_ms)
{
	if (timer == NULL || initial_ms == 0) {
		return EAI_OSAL_INVALID_PARAM;
	}
	k_timer_start(&timer->_impl, K_MSEC(initial_ms),
		      period_ms > 0 ? K_MSEC(period_ms) : K_NO_WAIT);
	return EAI_OSAL_OK;
}

eai_osal_status_t eai_osal_timer_stop(eai_osal_timer_t *timer)
{
	if (timer == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	k_timer_stop(&timer->_impl);
	return EAI_OSAL_OK;
}

bool eai_osal_timer_is_running(eai_osal_timer_t *timer)
{
	if (timer == NULL) {
		return false;
	}
	return k_timer_remaining_get(&timer->_impl) > 0;
}
