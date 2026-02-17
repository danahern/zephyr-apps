/*
 * eai_log Zephyr backend — direct passthrough to Zephyr logging.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef EAI_LOG_ZEPHYR_H
#define EAI_LOG_ZEPHYR_H

#include <zephyr/logging/log.h>

#define EAI_LOG_MODULE_REGISTER(name, level) LOG_MODULE_REGISTER(name, level)
#define EAI_LOG_MODULE_DECLARE(name, level)  LOG_MODULE_DECLARE(name, level)

#define EAI_LOG_ERR(...) LOG_ERR(__VA_ARGS__)
#define EAI_LOG_WRN(...) LOG_WRN(__VA_ARGS__)
#define EAI_LOG_INF(...) LOG_INF(__VA_ARGS__)
#define EAI_LOG_DBG(...) LOG_DBG(__VA_ARGS__)

#endif /* EAI_LOG_ZEPHYR_H */
