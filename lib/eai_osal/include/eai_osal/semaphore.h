#ifndef EAI_OSAL_SEMAPHORE_H
#define EAI_OSAL_SEMAPHORE_H

#include <eai_osal/types.h>

eai_osal_status_t eai_osal_sem_create(eai_osal_sem_t *sem, uint32_t initial,
				      uint32_t limit);
eai_osal_status_t eai_osal_sem_destroy(eai_osal_sem_t *sem);
eai_osal_status_t eai_osal_sem_give(eai_osal_sem_t *sem);
eai_osal_status_t eai_osal_sem_take(eai_osal_sem_t *sem, uint32_t timeout_ms);

#endif /* EAI_OSAL_SEMAPHORE_H */
