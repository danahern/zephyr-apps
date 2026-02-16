#ifndef EAI_OSAL_POSIX_INTERNAL_H
#define EAI_OSAL_POSIX_INTERNAL_H

#include <eai_osal/types.h>
#include <time.h>

/*
 * Compute absolute timespec for pthread_cond_timedwait / pthread_mutex_timedlock.
 * Uses CLOCK_REALTIME because macOS doesn't support pthread_condattr_setclock
 * with CLOCK_MONOTONIC.
 */
static inline struct timespec osal_timespec(uint32_t ms)
{
	struct timespec ts;

	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec += ms / 1000;
	ts.tv_nsec += (long)(ms % 1000) * 1000000L;
	if (ts.tv_nsec >= 1000000000L) {
		ts.tv_sec++;
		ts.tv_nsec -= 1000000000L;
	}
	return ts;
}

#endif /* EAI_OSAL_POSIX_INTERNAL_H */
