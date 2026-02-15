#ifndef EAI_OSAL_WORKQUEUE_H
#define EAI_OSAL_WORKQUEUE_H

#include <eai_osal/types.h>

/**
 * @brief Initialize a work item.
 *
 * @param work     Work item to initialize.
 * @param callback Function to call when work is processed.
 * @param arg      Argument passed to callback.
 * @return EAI_OSAL_OK on success, EAI_OSAL_INVALID_PARAM if work or callback is NULL.
 */
eai_osal_status_t eai_osal_work_init(eai_osal_work_t *work,
				     eai_osal_work_cb_t callback,
				     void *arg);

/**
 * @brief Submit work to the system work queue.
 *
 * @param work Work item to submit.
 * @return EAI_OSAL_OK if submitted, EAI_OSAL_ERROR if already pending.
 */
eai_osal_status_t eai_osal_work_submit(eai_osal_work_t *work);

/**
 * @brief Submit work to a specific work queue.
 *
 * @param work Work item to submit.
 * @param wq   Target work queue.
 * @return EAI_OSAL_OK if submitted, EAI_OSAL_ERROR if already pending.
 */
eai_osal_status_t eai_osal_work_submit_to(eai_osal_work_t *work,
					  eai_osal_workqueue_t *wq);

/**
 * @brief Initialize a delayed work item.
 *
 * @param dwork    Delayed work item to initialize.
 * @param callback Function to call when work is processed.
 * @param arg      Argument passed to callback.
 * @return EAI_OSAL_OK on success, EAI_OSAL_INVALID_PARAM if dwork or callback is NULL.
 */
eai_osal_status_t eai_osal_dwork_init(eai_osal_dwork_t *dwork,
				      eai_osal_work_cb_t callback,
				      void *arg);

/**
 * @brief Submit delayed work to the system work queue.
 *
 * @param dwork    Delayed work item.
 * @param delay_ms Delay in milliseconds before execution.
 * @return EAI_OSAL_OK if submitted, EAI_OSAL_ERROR if already pending.
 */
eai_osal_status_t eai_osal_dwork_submit(eai_osal_dwork_t *dwork,
					uint32_t delay_ms);

/**
 * @brief Submit delayed work to a specific work queue.
 *
 * @param dwork    Delayed work item.
 * @param wq       Target work queue.
 * @param delay_ms Delay in milliseconds before execution.
 * @return EAI_OSAL_OK if submitted, EAI_OSAL_ERROR if already pending.
 */
eai_osal_status_t eai_osal_dwork_submit_to(eai_osal_dwork_t *dwork,
					   eai_osal_workqueue_t *wq,
					   uint32_t delay_ms);

/**
 * @brief Cancel a pending delayed work item.
 *
 * @param dwork Delayed work item to cancel.
 * @return EAI_OSAL_OK if cancelled, EAI_OSAL_ERROR if not pending or already running.
 */
eai_osal_status_t eai_osal_dwork_cancel(eai_osal_dwork_t *dwork);

/**
 * @brief Create a custom work queue with its own thread.
 *
 * @param wq       Work queue to initialize.
 * @param name     Thread name (for debug).
 * @param stack    Stack defined via EAI_OSAL_THREAD_STACK_DEFINE.
 * @param stack_size Stack size via EAI_OSAL_THREAD_STACK_SIZEOF.
 * @param priority OSAL priority (0-31, higher = higher priority).
 * @return EAI_OSAL_OK on success, EAI_OSAL_INVALID_PARAM on NULL args.
 */
eai_osal_status_t eai_osal_workqueue_create(eai_osal_workqueue_t *wq,
					    const char *name,
					    void *stack,
					    size_t stack_size,
					    uint8_t priority);

#endif /* EAI_OSAL_WORKQUEUE_H */
