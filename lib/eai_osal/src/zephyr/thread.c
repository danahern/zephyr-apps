#include <eai_osal/thread.h>
#include "internal.h"
#include <zephyr/sys/util.h>

BUILD_ASSERT(CONFIG_NUM_PREEMPT_PRIORITIES >= 32,
	     "EAI OSAL requires CONFIG_NUM_PREEMPT_PRIORITIES >= 32");

static void thread_trampoline(void *entry, void *arg, void *unused)
{
	ARG_UNUSED(unused);
	((eai_osal_thread_entry_t)entry)(arg);
}

eai_osal_status_t eai_osal_thread_create(eai_osal_thread_t *thread,
					 const char *name,
					 eai_osal_thread_entry_t entry,
					 void *arg,
					 void *stack,
					 size_t stack_size,
					 uint8_t priority)
{
	if (thread == NULL || entry == NULL || stack == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	if (priority > 31) {
		return EAI_OSAL_INVALID_PARAM;
	}

	/* OSAL 0-31 (higher=higher) → Zephyr 31-0 (lower=higher) */
	int zephyr_prio = 31 - (int)priority;

	k_tid_t tid = k_thread_create(&thread->_impl,
				      (k_thread_stack_t *)stack,
				      stack_size,
				      thread_trampoline,
				      (void *)entry,
				      arg,
				      NULL,
				      zephyr_prio,
				      0,
				      K_NO_WAIT);
	if (tid == NULL) {
		return EAI_OSAL_ERROR;
	}

	if (name != NULL) {
		k_thread_name_set(tid, name);
	}

	return EAI_OSAL_OK;
}

eai_osal_status_t eai_osal_thread_join(eai_osal_thread_t *thread,
				       uint32_t timeout_ms)
{
	if (thread == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	return osal_status(k_thread_join(&thread->_impl, osal_timeout(timeout_ms)));
}

void eai_osal_thread_sleep(uint32_t ms)
{
	k_msleep(ms);
}

void eai_osal_thread_yield(void)
{
	k_yield();
}
