#include <eai_osal/time.h>
#include <zephyr/kernel.h>

uint32_t eai_osal_time_get_ms(void)
{
	return (uint32_t)k_uptime_get();
}

uint64_t eai_osal_time_get_ticks(void)
{
	return k_uptime_ticks();
}

uint32_t eai_osal_time_ticks_to_ms(uint64_t ticks)
{
	return (uint32_t)k_ticks_to_ms_floor64(ticks);
}
