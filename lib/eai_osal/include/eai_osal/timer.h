#ifndef EAI_OSAL_TIMER_H
#define EAI_OSAL_TIMER_H

#include <eai_osal/types.h>

eai_osal_status_t eai_osal_timer_create(eai_osal_timer_t *timer,
					eai_osal_timer_cb_t callback,
					void *arg);
eai_osal_status_t eai_osal_timer_destroy(eai_osal_timer_t *timer);
eai_osal_status_t eai_osal_timer_start(eai_osal_timer_t *timer,
				       uint32_t initial_ms,
				       uint32_t period_ms);
eai_osal_status_t eai_osal_timer_stop(eai_osal_timer_t *timer);
bool eai_osal_timer_is_running(eai_osal_timer_t *timer);

#endif /* EAI_OSAL_TIMER_H */
