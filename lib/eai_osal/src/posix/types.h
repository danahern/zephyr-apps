#ifndef EAI_OSAL_POSIX_TYPES_H
#define EAI_OSAL_POSIX_TYPES_H

/*
 * Internal header — included only via include/eai_osal/types.h.
 * eai_osal_timer_cb_t, eai_osal_thread_entry_t, and eai_osal_work_cb_t
 * are already typedef'd by the time this file is processed.
 */

#include <pthread.h>

typedef struct {
	pthread_mutex_t _handle;
} eai_osal_mutex_t;

typedef struct {
	pthread_mutex_t _lock;
	pthread_cond_t _cond;
	uint32_t _count;
	uint32_t _limit;
} eai_osal_sem_t;

typedef struct {
	pthread_t _handle;
	eai_osal_thread_entry_t _entry;
	void *_entry_arg;
	pthread_mutex_t _join_lock;
	pthread_cond_t _join_cond;
	bool _done;
} eai_osal_thread_t;

typedef struct {
	pthread_mutex_t _lock;
	pthread_cond_t _not_full;
	pthread_cond_t _not_empty;
	uint8_t *_buf;
	size_t _msg_size;
	uint32_t _max_msgs;
	uint32_t _head;
	uint32_t _tail;
	uint32_t _count;
} eai_osal_queue_t;

typedef struct {
	pthread_t _thread;
	pthread_mutex_t _lock;
	pthread_cond_t _cond;
	eai_osal_timer_cb_t _cb;
	void *_cb_arg;
	uint32_t _initial_ms;
	uint32_t _period_ms;
	bool _running;
	bool _destroy;
	bool _thread_alive;
} eai_osal_timer_t;

typedef struct {
	pthread_mutex_t _lock;
	pthread_cond_t _cond;
	uint32_t _bits;
} eai_osal_event_t;

typedef unsigned int eai_osal_critical_key_t;

/* Work item — submitted to a work queue */
typedef struct {
	eai_osal_work_cb_t _cb;
	void *_cb_arg;
} eai_osal_work_t;

/* Forward declaration for delayed work target */
struct eai_osal_workqueue;

/* Delayed work — uses a timer thread to defer submission */
typedef struct {
	eai_osal_work_cb_t _cb;
	void *_cb_arg;
	pthread_t _timer_thread;
	pthread_mutex_t _lock;
	pthread_cond_t _cond;
	uint32_t _delay_ms;
	void *_target_wq; /* eai_osal_workqueue_t*, NULL = system */
	bool _pending;
	bool _cancel;
	bool _thread_alive;
} eai_osal_dwork_t;

/* Work queue — thread that processes {cb, arg} items from an internal queue */
typedef struct eai_osal_workqueue {
	pthread_t _thread;
	eai_osal_queue_t _queue;
	uint8_t _buf[16 * (sizeof(eai_osal_work_cb_t) + sizeof(void *))];
} eai_osal_workqueue_t;

/*
 * Thread stacks on POSIX are allocated by pthread.
 * These macros exist for API compatibility — the stack array is not used.
 */
#define EAI_OSAL_THREAD_STACK_DEFINE(name, size) \
	static uint8_t name[size] __attribute__((unused))
#define EAI_OSAL_THREAD_STACK_SIZEOF(name) sizeof(name)

#endif /* EAI_OSAL_POSIX_TYPES_H */
