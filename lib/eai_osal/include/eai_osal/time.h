#ifndef EAI_OSAL_TIME_H
#define EAI_OSAL_TIME_H

#include <eai_osal/types.h>

uint32_t eai_osal_time_get_ms(void);
uint64_t eai_osal_time_get_ticks(void);
uint32_t eai_osal_time_ticks_to_ms(uint64_t ticks);

#endif /* EAI_OSAL_TIME_H */
