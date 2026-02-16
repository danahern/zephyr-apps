#include <eai_osal/event.h>
#include "internal.h"

eai_osal_status_t eai_osal_event_create(eai_osal_event_t *event)
{
	if (event == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}

	event->_bits = 0;

	if (pthread_mutex_init(&event->_lock, NULL) != 0) {
		return EAI_OSAL_ERROR;
	}
	if (pthread_cond_init(&event->_cond, NULL) != 0) {
		pthread_mutex_destroy(&event->_lock);
		return EAI_OSAL_ERROR;
	}
	return EAI_OSAL_OK;
}

eai_osal_status_t eai_osal_event_destroy(eai_osal_event_t *event)
{
	if (event == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	pthread_cond_destroy(&event->_cond);
	pthread_mutex_destroy(&event->_lock);
	return EAI_OSAL_OK;
}

eai_osal_status_t eai_osal_event_set(eai_osal_event_t *event, uint32_t bits)
{
	if (event == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	pthread_mutex_lock(&event->_lock);
	event->_bits |= bits;
	pthread_cond_broadcast(&event->_cond);
	pthread_mutex_unlock(&event->_lock);
	return EAI_OSAL_OK;
}

eai_osal_status_t eai_osal_event_wait(eai_osal_event_t *event, uint32_t bits,
				      bool wait_all, uint32_t *actual,
				      uint32_t timeout_ms)
{
	if (event == NULL || bits == 0) {
		return EAI_OSAL_INVALID_PARAM;
	}

	pthread_mutex_lock(&event->_lock);

	/* Check condition macro */
	#define BITS_MET(ev, b, all) \
		((all) ? (((ev)->_bits & (b)) == (b)) : (((ev)->_bits & (b)) != 0))

	if (timeout_ms == EAI_OSAL_NO_WAIT) {
		if (!BITS_MET(event, bits, wait_all)) {
			pthread_mutex_unlock(&event->_lock);
			return EAI_OSAL_TIMEOUT;
		}
	} else if (timeout_ms == EAI_OSAL_WAIT_FOREVER) {
		while (!BITS_MET(event, bits, wait_all)) {
			pthread_cond_wait(&event->_cond, &event->_lock);
		}
	} else {
		struct timespec ts = osal_timespec(timeout_ms);
		while (!BITS_MET(event, bits, wait_all)) {
			int ret = pthread_cond_timedwait(&event->_cond,
							 &event->_lock, &ts);
			if (ret != 0) {
				pthread_mutex_unlock(&event->_lock);
				return EAI_OSAL_TIMEOUT;
			}
		}
	}

	#undef BITS_MET

	if (actual != NULL) {
		*actual = event->_bits & bits;
	}
	pthread_mutex_unlock(&event->_lock);
	return EAI_OSAL_OK;
}

eai_osal_status_t eai_osal_event_clear(eai_osal_event_t *event, uint32_t bits)
{
	if (event == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	pthread_mutex_lock(&event->_lock);
	event->_bits &= ~bits;
	pthread_mutex_unlock(&event->_lock);
	return EAI_OSAL_OK;
}
