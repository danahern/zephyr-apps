#ifndef EAI_OSAL_THREAD_H
#define EAI_OSAL_THREAD_H

#include <eai_osal/types.h>

eai_osal_status_t eai_osal_thread_create(eai_osal_thread_t *thread,
					 const char *name,
					 eai_osal_thread_entry_t entry,
					 void *arg,
					 void *stack,
					 size_t stack_size,
					 uint8_t priority);
eai_osal_status_t eai_osal_thread_join(eai_osal_thread_t *thread,
				       uint32_t timeout_ms);
void eai_osal_thread_sleep(uint32_t ms);
void eai_osal_thread_yield(void);

#endif /* EAI_OSAL_THREAD_H */
