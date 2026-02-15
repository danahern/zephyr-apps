#ifndef EAI_OSAL_QUEUE_H
#define EAI_OSAL_QUEUE_H

#include <eai_osal/types.h>

eai_osal_status_t eai_osal_queue_create(eai_osal_queue_t *queue, size_t msg_size,
					uint32_t max_msgs, void *buffer);
eai_osal_status_t eai_osal_queue_destroy(eai_osal_queue_t *queue);
eai_osal_status_t eai_osal_queue_send(eai_osal_queue_t *queue, const void *msg,
				      uint32_t timeout_ms);
eai_osal_status_t eai_osal_queue_recv(eai_osal_queue_t *queue, void *msg,
				      uint32_t timeout_ms);

#endif /* EAI_OSAL_QUEUE_H */
