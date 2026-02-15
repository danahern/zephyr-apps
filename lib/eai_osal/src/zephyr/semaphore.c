#include <eai_osal/semaphore.h>
#include "internal.h"

eai_osal_status_t eai_osal_sem_create(eai_osal_sem_t *sem, uint32_t initial,
				      uint32_t limit)
{
	if (sem == NULL || limit == 0) {
		return EAI_OSAL_INVALID_PARAM;
	}
	return osal_status(k_sem_init(&sem->_impl, initial, limit));
}

eai_osal_status_t eai_osal_sem_destroy(eai_osal_sem_t *sem)
{
	if (sem == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	return EAI_OSAL_OK;
}

eai_osal_status_t eai_osal_sem_give(eai_osal_sem_t *sem)
{
	if (sem == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	k_sem_give(&sem->_impl);
	return EAI_OSAL_OK;
}

eai_osal_status_t eai_osal_sem_take(eai_osal_sem_t *sem, uint32_t timeout_ms)
{
	if (sem == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	return osal_status(k_sem_take(&sem->_impl, osal_timeout(timeout_ms)));
}
