#ifndef EAI_OSAL_CRITICAL_H
#define EAI_OSAL_CRITICAL_H

#include <eai_osal/types.h>

eai_osal_critical_key_t eai_osal_critical_enter(void);
void eai_osal_critical_exit(eai_osal_critical_key_t key);

#endif /* EAI_OSAL_CRITICAL_H */
