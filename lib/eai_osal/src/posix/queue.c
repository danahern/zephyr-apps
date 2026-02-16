#include <eai_osal/queue.h>
#include "internal.h"
#include <string.h>

eai_osal_status_t eai_osal_queue_create(eai_osal_queue_t *queue, size_t msg_size,
					uint32_t max_msgs, void *buffer)
{
	if (queue == NULL || buffer == NULL || msg_size == 0 || max_msgs == 0) {
		return EAI_OSAL_INVALID_PARAM;
	}

	queue->_buf = (uint8_t *)buffer;
	queue->_msg_size = msg_size;
	queue->_max_msgs = max_msgs;
	queue->_head = 0;
	queue->_tail = 0;
	queue->_count = 0;

	if (pthread_mutex_init(&queue->_lock, NULL) != 0) {
		return EAI_OSAL_ERROR;
	}
	if (pthread_cond_init(&queue->_not_full, NULL) != 0) {
		pthread_mutex_destroy(&queue->_lock);
		return EAI_OSAL_ERROR;
	}
	if (pthread_cond_init(&queue->_not_empty, NULL) != 0) {
		pthread_cond_destroy(&queue->_not_full);
		pthread_mutex_destroy(&queue->_lock);
		return EAI_OSAL_ERROR;
	}
	return EAI_OSAL_OK;
}

eai_osal_status_t eai_osal_queue_destroy(eai_osal_queue_t *queue)
{
	if (queue == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	pthread_cond_destroy(&queue->_not_empty);
	pthread_cond_destroy(&queue->_not_full);
	pthread_mutex_destroy(&queue->_lock);
	return EAI_OSAL_OK;
}

eai_osal_status_t eai_osal_queue_send(eai_osal_queue_t *queue, const void *msg,
				      uint32_t timeout_ms)
{
	if (queue == NULL || msg == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}

	pthread_mutex_lock(&queue->_lock);

	if (timeout_ms == EAI_OSAL_NO_WAIT) {
		if (queue->_count >= queue->_max_msgs) {
			pthread_mutex_unlock(&queue->_lock);
			return EAI_OSAL_TIMEOUT;
		}
	} else if (timeout_ms == EAI_OSAL_WAIT_FOREVER) {
		while (queue->_count >= queue->_max_msgs) {
			pthread_cond_wait(&queue->_not_full, &queue->_lock);
		}
	} else {
		struct timespec ts = osal_timespec(timeout_ms);
		while (queue->_count >= queue->_max_msgs) {
			int ret = pthread_cond_timedwait(&queue->_not_full,
							 &queue->_lock, &ts);
			if (ret != 0) {
				pthread_mutex_unlock(&queue->_lock);
				return EAI_OSAL_TIMEOUT;
			}
		}
	}

	memcpy(queue->_buf + queue->_head * queue->_msg_size, msg, queue->_msg_size);
	queue->_head = (queue->_head + 1) % queue->_max_msgs;
	queue->_count++;
	pthread_cond_signal(&queue->_not_empty);
	pthread_mutex_unlock(&queue->_lock);
	return EAI_OSAL_OK;
}

eai_osal_status_t eai_osal_queue_recv(eai_osal_queue_t *queue, void *msg,
				      uint32_t timeout_ms)
{
	if (queue == NULL || msg == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}

	pthread_mutex_lock(&queue->_lock);

	if (timeout_ms == EAI_OSAL_NO_WAIT) {
		if (queue->_count == 0) {
			pthread_mutex_unlock(&queue->_lock);
			return EAI_OSAL_TIMEOUT;
		}
	} else if (timeout_ms == EAI_OSAL_WAIT_FOREVER) {
		while (queue->_count == 0) {
			pthread_cond_wait(&queue->_not_empty, &queue->_lock);
		}
	} else {
		struct timespec ts = osal_timespec(timeout_ms);
		while (queue->_count == 0) {
			int ret = pthread_cond_timedwait(&queue->_not_empty,
							 &queue->_lock, &ts);
			if (ret != 0) {
				pthread_mutex_unlock(&queue->_lock);
				return EAI_OSAL_TIMEOUT;
			}
		}
	}

	memcpy(msg, queue->_buf + queue->_tail * queue->_msg_size, queue->_msg_size);
	queue->_tail = (queue->_tail + 1) % queue->_max_msgs;
	queue->_count--;
	pthread_cond_signal(&queue->_not_full);
	pthread_mutex_unlock(&queue->_lock);
	return EAI_OSAL_OK;
}
