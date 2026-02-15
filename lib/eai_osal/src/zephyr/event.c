#include <eai_osal/event.h>
#include "internal.h"

eai_osal_status_t eai_osal_event_create(eai_osal_event_t *event)
{
	if (event == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	k_event_init(&event->_impl);
	return EAI_OSAL_OK;
}

eai_osal_status_t eai_osal_event_destroy(eai_osal_event_t *event)
{
	if (event == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	return EAI_OSAL_OK;
}

eai_osal_status_t eai_osal_event_set(eai_osal_event_t *event, uint32_t bits)
{
	if (event == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	k_event_post(&event->_impl, bits);
	return EAI_OSAL_OK;
}

eai_osal_status_t eai_osal_event_wait(eai_osal_event_t *event, uint32_t bits,
				      bool wait_all, uint32_t *actual,
				      uint32_t timeout_ms)
{
	if (event == NULL || bits == 0) {
		return EAI_OSAL_INVALID_PARAM;
	}

	uint32_t result;

	if (wait_all) {
		result = k_event_wait_all(&event->_impl, bits, false,
					  osal_timeout(timeout_ms));
	} else {
		result = k_event_wait(&event->_impl, bits, false,
				      osal_timeout(timeout_ms));
	}

	if (result == 0) {
		return EAI_OSAL_TIMEOUT;
	}

	if (actual != NULL) {
		*actual = result;
	}
	return EAI_OSAL_OK;
}

eai_osal_status_t eai_osal_event_clear(eai_osal_event_t *event, uint32_t bits)
{
	if (event == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	k_event_clear(&event->_impl, bits);
	return EAI_OSAL_OK;
}
