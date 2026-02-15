#ifndef EAI_OSAL_EVENT_H
#define EAI_OSAL_EVENT_H

#include <eai_osal/types.h>

eai_osal_status_t eai_osal_event_create(eai_osal_event_t *event);
eai_osal_status_t eai_osal_event_destroy(eai_osal_event_t *event);
eai_osal_status_t eai_osal_event_set(eai_osal_event_t *event, uint32_t bits);
eai_osal_status_t eai_osal_event_wait(eai_osal_event_t *event, uint32_t bits,
				      bool wait_all, uint32_t *actual,
				      uint32_t timeout_ms);
eai_osal_status_t eai_osal_event_clear(eai_osal_event_t *event, uint32_t bits);

#endif /* EAI_OSAL_EVENT_H */
