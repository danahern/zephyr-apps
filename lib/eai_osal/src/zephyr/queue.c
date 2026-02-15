#include <eai_osal/queue.h>
#include "internal.h"

eai_osal_status_t eai_osal_queue_create(eai_osal_queue_t *queue, size_t msg_size,
					uint32_t max_msgs, void *buffer)
{
	if (queue == NULL || buffer == NULL || msg_size == 0 || max_msgs == 0) {
		return EAI_OSAL_INVALID_PARAM;
	}
	k_msgq_init(&queue->_impl, (char *)buffer, msg_size, max_msgs);
	return EAI_OSAL_OK;
}

eai_osal_status_t eai_osal_queue_destroy(eai_osal_queue_t *queue)
{
	if (queue == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	k_msgq_purge(&queue->_impl);
	return EAI_OSAL_OK;
}

eai_osal_status_t eai_osal_queue_send(eai_osal_queue_t *queue, const void *msg,
				      uint32_t timeout_ms)
{
	if (queue == NULL || msg == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	return osal_status(k_msgq_put(&queue->_impl, msg, osal_timeout(timeout_ms)));
}

eai_osal_status_t eai_osal_queue_recv(eai_osal_queue_t *queue, void *msg,
				      uint32_t timeout_ms)
{
	if (queue == NULL || msg == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	return osal_status(k_msgq_get(&queue->_impl, msg, osal_timeout(timeout_ms)));
}
