#ifndef EAI_OSAL_ZEPHYR_INTERNAL_H
#define EAI_OSAL_ZEPHYR_INTERNAL_H

#include <zephyr/kernel.h>
#include <eai_osal/types.h>

static inline k_timeout_t osal_timeout(uint32_t ms)
{
	if (ms == EAI_OSAL_WAIT_FOREVER) {
		return K_FOREVER;
	}
	if (ms == EAI_OSAL_NO_WAIT) {
		return K_NO_WAIT;
	}
	return K_MSEC(ms);
}

static inline eai_osal_status_t osal_status(int ret)
{
	if (ret == 0) {
		return EAI_OSAL_OK;
	}
	if (ret == -EAGAIN || ret == -EBUSY || ret == -ENOMSG) {
		return EAI_OSAL_TIMEOUT;
	}
	if (ret == -ENOMEM) {
		return EAI_OSAL_NO_MEMORY;
	}
	if (ret == -EINVAL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	return EAI_OSAL_ERROR;
}

#endif /* EAI_OSAL_ZEPHYR_INTERNAL_H */
