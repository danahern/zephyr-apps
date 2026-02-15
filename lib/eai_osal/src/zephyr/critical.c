#include <eai_osal/critical.h>
#include <zephyr/irq.h>

eai_osal_critical_key_t eai_osal_critical_enter(void)
{
	return irq_lock();
}

void eai_osal_critical_exit(eai_osal_critical_key_t key)
{
	irq_unlock(key);
}
