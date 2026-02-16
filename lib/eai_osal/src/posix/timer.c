#include <eai_osal/timer.h>
#include "internal.h"

static void *timer_thread_func(void *arg)
{
	eai_osal_timer_t *timer = (eai_osal_timer_t *)arg;

	pthread_mutex_lock(&timer->_lock);

	/* Wait for first start signal */
	while (!timer->_running && !timer->_destroy) {
		pthread_cond_wait(&timer->_cond, &timer->_lock);
	}

	while (!timer->_destroy) {
		if (!timer->_running) {
			pthread_cond_wait(&timer->_cond, &timer->_lock);
			continue;
		}

		/* Determine wait: initial_ms for first fire, period_ms after */
		uint32_t wait_ms;
		if (timer->_initial_ms > 0) {
			wait_ms = timer->_initial_ms;
		} else {
			wait_ms = timer->_period_ms;
		}

		struct timespec ts = osal_timespec(wait_ms);
		pthread_cond_timedwait(&timer->_cond, &timer->_lock, &ts);

		if (timer->_destroy) {
			break;
		}
		if (!timer->_running) {
			continue;
		}

		/* Fire callback with lock released */
		eai_osal_timer_cb_t cb = timer->_cb;
		void *cb_arg = timer->_cb_arg;
		pthread_mutex_unlock(&timer->_lock);

		if (cb != NULL) {
			cb(cb_arg);
		}

		pthread_mutex_lock(&timer->_lock);

		/* After initial fire: switch to repeat period or stop */
		if (timer->_initial_ms > 0) {
			timer->_initial_ms = 0;
			if (timer->_period_ms == 0) {
				timer->_running = false;
			}
		}
	}

	pthread_mutex_unlock(&timer->_lock);
	return NULL;
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
	timer->_initial_ms = 0;
	timer->_period_ms = 0;
	timer->_running = false;
	timer->_destroy = false;
	timer->_thread_alive = false;

	if (pthread_mutex_init(&timer->_lock, NULL) != 0) {
		return EAI_OSAL_ERROR;
	}
	if (pthread_cond_init(&timer->_cond, NULL) != 0) {
		pthread_mutex_destroy(&timer->_lock);
		return EAI_OSAL_ERROR;
	}

	return EAI_OSAL_OK;
}

eai_osal_status_t eai_osal_timer_destroy(eai_osal_timer_t *timer)
{
	if (timer == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}

	pthread_mutex_lock(&timer->_lock);
	timer->_destroy = true;
	timer->_running = false;
	pthread_cond_signal(&timer->_cond);
	bool alive = timer->_thread_alive;
	pthread_mutex_unlock(&timer->_lock);

	if (alive) {
		pthread_join(timer->_thread, NULL);
	}

	pthread_cond_destroy(&timer->_cond);
	pthread_mutex_destroy(&timer->_lock);
	return EAI_OSAL_OK;
}

eai_osal_status_t eai_osal_timer_start(eai_osal_timer_t *timer,
				       uint32_t initial_ms,
				       uint32_t period_ms)
{
	if (timer == NULL || initial_ms == 0) {
		return EAI_OSAL_INVALID_PARAM;
	}

	pthread_mutex_lock(&timer->_lock);
	timer->_initial_ms = initial_ms;
	timer->_period_ms = period_ms;
	timer->_running = true;
	timer->_destroy = false;

	if (!timer->_thread_alive) {
		int ret = pthread_create(&timer->_thread, NULL,
					 timer_thread_func, timer);
		if (ret != 0) {
			timer->_running = false;
			pthread_mutex_unlock(&timer->_lock);
			return EAI_OSAL_ERROR;
		}
		timer->_thread_alive = true;
	}

	pthread_cond_signal(&timer->_cond);
	pthread_mutex_unlock(&timer->_lock);
	return EAI_OSAL_OK;
}

eai_osal_status_t eai_osal_timer_stop(eai_osal_timer_t *timer)
{
	if (timer == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	pthread_mutex_lock(&timer->_lock);
	timer->_running = false;
	pthread_cond_signal(&timer->_cond);
	pthread_mutex_unlock(&timer->_lock);
	return EAI_OSAL_OK;
}

bool eai_osal_timer_is_running(eai_osal_timer_t *timer)
{
	if (timer == NULL) {
		return false;
	}
	pthread_mutex_lock(&timer->_lock);
	bool running = timer->_running;
	pthread_mutex_unlock(&timer->_lock);
	return running;
}
