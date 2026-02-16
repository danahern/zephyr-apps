#include <eai_osal/thread.h>
#include "internal.h"
#include <unistd.h>
#include <sched.h>

static void *thread_trampoline(void *arg)
{
	eai_osal_thread_t *thread = (eai_osal_thread_t *)arg;

	thread->_entry(thread->_entry_arg);

	/* Signal join waiters */
	pthread_mutex_lock(&thread->_join_lock);
	thread->_done = true;
	pthread_cond_broadcast(&thread->_join_cond);
	pthread_mutex_unlock(&thread->_join_lock);

	return NULL;
}

eai_osal_status_t eai_osal_thread_create(eai_osal_thread_t *thread,
					 const char *name,
					 eai_osal_thread_entry_t entry,
					 void *arg,
					 void *stack,
					 size_t stack_size,
					 uint8_t priority)
{
	(void)name;

	if (thread == NULL || entry == NULL || stack == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	if (priority > 31) {
		return EAI_OSAL_INVALID_PARAM;
	}

	thread->_entry = entry;
	thread->_entry_arg = arg;
	thread->_done = false;

	if (pthread_mutex_init(&thread->_join_lock, NULL) != 0) {
		return EAI_OSAL_ERROR;
	}
	if (pthread_cond_init(&thread->_join_cond, NULL) != 0) {
		pthread_mutex_destroy(&thread->_join_lock);
		return EAI_OSAL_ERROR;
	}

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, stack_size < 16384 ? 16384 : stack_size);

	int ret = pthread_create(&thread->_handle, &attr, thread_trampoline, thread);
	pthread_attr_destroy(&attr);

	if (ret != 0) {
		pthread_cond_destroy(&thread->_join_cond);
		pthread_mutex_destroy(&thread->_join_lock);
		return EAI_OSAL_ERROR;
	}

	/* Best-effort priority setting — no error if insufficient privileges */
	struct sched_param sp = { .sched_priority = 0 };
	pthread_setschedparam(thread->_handle, SCHED_OTHER, &sp);

	return EAI_OSAL_OK;
}

eai_osal_status_t eai_osal_thread_join(eai_osal_thread_t *thread,
				       uint32_t timeout_ms)
{
	if (thread == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}

	pthread_mutex_lock(&thread->_join_lock);

	if (timeout_ms == EAI_OSAL_WAIT_FOREVER) {
		while (!thread->_done) {
			pthread_cond_wait(&thread->_join_cond,
					  &thread->_join_lock);
		}
	} else {
		struct timespec ts = osal_timespec(timeout_ms);
		while (!thread->_done) {
			int ret = pthread_cond_timedwait(&thread->_join_cond,
							 &thread->_join_lock,
							 &ts);
			if (ret != 0) {
				pthread_mutex_unlock(&thread->_join_lock);
				return EAI_OSAL_TIMEOUT;
			}
		}
	}

	pthread_mutex_unlock(&thread->_join_lock);
	pthread_join(thread->_handle, NULL);
	pthread_cond_destroy(&thread->_join_cond);
	pthread_mutex_destroy(&thread->_join_lock);
	return EAI_OSAL_OK;
}

void eai_osal_thread_sleep(uint32_t ms)
{
	usleep((useconds_t)ms * 1000);
}

void eai_osal_thread_yield(void)
{
	sched_yield();
}
