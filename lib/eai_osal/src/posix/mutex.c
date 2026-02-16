#include <eai_osal/mutex.h>
#include "internal.h"
#include <errno.h>
#include <unistd.h>

eai_osal_status_t eai_osal_mutex_create(eai_osal_mutex_t *mutex)
{
	if (mutex == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}

	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

	int ret = pthread_mutex_init(&mutex->_handle, &attr);
	pthread_mutexattr_destroy(&attr);

	return ret == 0 ? EAI_OSAL_OK : EAI_OSAL_ERROR;
}

eai_osal_status_t eai_osal_mutex_destroy(eai_osal_mutex_t *mutex)
{
	if (mutex == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	pthread_mutex_destroy(&mutex->_handle);
	return EAI_OSAL_OK;
}

eai_osal_status_t eai_osal_mutex_lock(eai_osal_mutex_t *mutex, uint32_t timeout_ms)
{
	if (mutex == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}

	if (timeout_ms == EAI_OSAL_WAIT_FOREVER) {
		return pthread_mutex_lock(&mutex->_handle) == 0
			? EAI_OSAL_OK : EAI_OSAL_ERROR;
	}

	if (timeout_ms == EAI_OSAL_NO_WAIT) {
		return pthread_mutex_trylock(&mutex->_handle) == 0
			? EAI_OSAL_OK : EAI_OSAL_TIMEOUT;
	}

	/*
	 * macOS lacks pthread_mutex_timedlock. Use trylock + sleep loop.
	 * 1ms resolution is fine for test timeouts (50-200ms).
	 */
	uint32_t waited = 0;

	while (waited < timeout_ms) {
		if (pthread_mutex_trylock(&mutex->_handle) == 0) {
			return EAI_OSAL_OK;
		}
		usleep(1000); /* 1ms */
		waited++;
	}
	return EAI_OSAL_TIMEOUT;
}

eai_osal_status_t eai_osal_mutex_unlock(eai_osal_mutex_t *mutex)
{
	if (mutex == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	return pthread_mutex_unlock(&mutex->_handle) == 0
		? EAI_OSAL_OK : EAI_OSAL_ERROR;
}
