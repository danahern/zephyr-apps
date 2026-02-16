#include <eai_osal/time.h>
#include <time.h>

/*
 * POSIX time — uses CLOCK_MONOTONIC for monotonic uptime.
 * Ticks are microseconds for reasonable precision without overflow.
 */

#define TICKS_PER_MS 1000 /* 1 tick = 1 microsecond */

uint32_t eai_osal_time_get_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

uint64_t eai_osal_time_get_ticks(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)(ts.tv_nsec / 1000);
}

uint32_t eai_osal_time_ticks_to_ms(uint64_t ticks)
{
	return (uint32_t)(ticks / TICKS_PER_MS);
}
