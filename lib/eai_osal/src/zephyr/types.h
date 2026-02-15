#ifndef EAI_OSAL_ZEPHYR_TYPES_H
#define EAI_OSAL_ZEPHYR_TYPES_H

/*
 * Internal header — included only via include/eai_osal/types.h.
 * eai_osal_timer_cb_t and eai_osal_thread_entry_t are already typedef'd
 * by the time this file is processed.
 */

#include <zephyr/kernel.h>

typedef struct { struct k_mutex _impl; } eai_osal_mutex_t;
typedef struct { struct k_sem _impl; } eai_osal_sem_t;
typedef struct { struct k_thread _impl; } eai_osal_thread_t;
typedef struct { struct k_msgq _impl; } eai_osal_queue_t;

typedef struct {
	struct k_timer _impl;
	eai_osal_timer_cb_t _cb;
	void *_cb_arg;
} eai_osal_timer_t;

typedef struct { struct k_event _impl; } eai_osal_event_t;
typedef unsigned int eai_osal_critical_key_t;

typedef struct {
	struct k_work _impl;
	eai_osal_work_cb_t _cb;
	void *_cb_arg;
} eai_osal_work_t;

typedef struct {
	struct k_work_delayable _impl;
	eai_osal_work_cb_t _cb;
	void *_cb_arg;
} eai_osal_dwork_t;

typedef struct { struct k_work_q _impl; } eai_osal_workqueue_t;

#define EAI_OSAL_THREAD_STACK_DEFINE(name, size) K_THREAD_STACK_DEFINE(name, size)
#define EAI_OSAL_THREAD_STACK_SIZEOF(name)       K_THREAD_STACK_SIZEOF(name)

#endif /* EAI_OSAL_ZEPHYR_TYPES_H */
