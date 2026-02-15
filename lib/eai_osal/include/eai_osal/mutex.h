#ifndef EAI_OSAL_MUTEX_H
#define EAI_OSAL_MUTEX_H

#include <eai_osal/types.h>

eai_osal_status_t eai_osal_mutex_create(eai_osal_mutex_t *mutex);
eai_osal_status_t eai_osal_mutex_destroy(eai_osal_mutex_t *mutex);
eai_osal_status_t eai_osal_mutex_lock(eai_osal_mutex_t *mutex, uint32_t timeout_ms);
eai_osal_status_t eai_osal_mutex_unlock(eai_osal_mutex_t *mutex);

#endif /* EAI_OSAL_MUTEX_H */
