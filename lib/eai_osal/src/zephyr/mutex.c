#include <eai_osal/mutex.h>
#include "internal.h"

eai_osal_status_t eai_osal_mutex_create(eai_osal_mutex_t *mutex)
{
	if (mutex == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	return osal_status(k_mutex_init(&mutex->_impl));
}

eai_osal_status_t eai_osal_mutex_destroy(eai_osal_mutex_t *mutex)
{
	if (mutex == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	return EAI_OSAL_OK;
}

eai_osal_status_t eai_osal_mutex_lock(eai_osal_mutex_t *mutex, uint32_t timeout_ms)
{
	if (mutex == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	return osal_status(k_mutex_lock(&mutex->_impl, osal_timeout(timeout_ms)));
}

eai_osal_status_t eai_osal_mutex_unlock(eai_osal_mutex_t *mutex)
{
	if (mutex == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	return osal_status(k_mutex_unlock(&mutex->_impl));
}
