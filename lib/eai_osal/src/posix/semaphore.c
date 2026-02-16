#include <eai_osal/semaphore.h>
#include "internal.h"

eai_osal_status_t eai_osal_sem_create(eai_osal_sem_t *sem, uint32_t initial,
				      uint32_t limit)
{
	if (sem == NULL || limit == 0) {
		return EAI_OSAL_INVALID_PARAM;
	}

	sem->_count = initial;
	sem->_limit = limit;

	if (pthread_mutex_init(&sem->_lock, NULL) != 0) {
		return EAI_OSAL_ERROR;
	}
	if (pthread_cond_init(&sem->_cond, NULL) != 0) {
		pthread_mutex_destroy(&sem->_lock);
		return EAI_OSAL_ERROR;
	}
	return EAI_OSAL_OK;
}

eai_osal_status_t eai_osal_sem_destroy(eai_osal_sem_t *sem)
{
	if (sem == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	pthread_cond_destroy(&sem->_cond);
	pthread_mutex_destroy(&sem->_lock);
	return EAI_OSAL_OK;
}

eai_osal_status_t eai_osal_sem_give(eai_osal_sem_t *sem)
{
	if (sem == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}

	pthread_mutex_lock(&sem->_lock);
	if (sem->_count < sem->_limit) {
		sem->_count++;
		pthread_cond_signal(&sem->_cond);
	}
	/* At limit — silently ignore, matching FreeRTOS behavior */
	pthread_mutex_unlock(&sem->_lock);
	return EAI_OSAL_OK;
}

eai_osal_status_t eai_osal_sem_take(eai_osal_sem_t *sem, uint32_t timeout_ms)
{
	if (sem == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}

	pthread_mutex_lock(&sem->_lock);

	if (timeout_ms == EAI_OSAL_NO_WAIT) {
		if (sem->_count > 0) {
			sem->_count--;
			pthread_mutex_unlock(&sem->_lock);
			return EAI_OSAL_OK;
		}
		pthread_mutex_unlock(&sem->_lock);
		return EAI_OSAL_TIMEOUT;
	}

	if (timeout_ms == EAI_OSAL_WAIT_FOREVER) {
		while (sem->_count == 0) {
			pthread_cond_wait(&sem->_cond, &sem->_lock);
		}
		sem->_count--;
		pthread_mutex_unlock(&sem->_lock);
		return EAI_OSAL_OK;
	}

	/* Timed wait */
	struct timespec ts = osal_timespec(timeout_ms);

	while (sem->_count == 0) {
		int ret = pthread_cond_timedwait(&sem->_cond, &sem->_lock, &ts);
		if (ret != 0) {
			pthread_mutex_unlock(&sem->_lock);
			return EAI_OSAL_TIMEOUT;
		}
	}
	sem->_count--;
	pthread_mutex_unlock(&sem->_lock);
	return EAI_OSAL_OK;
}
