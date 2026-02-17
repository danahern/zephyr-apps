/*
 * eai_log FreeRTOS/ESP-IDF backend — TAG-based ESP_LOGx macros.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef EAI_LOG_FREERTOS_H
#define EAI_LOG_FREERTOS_H

#include "esp_log.h"

#define EAI_LOG_MODULE_REGISTER(name, level) static const char *_eai_log_tag = #name
#define EAI_LOG_MODULE_DECLARE(name, level)  static const char *_eai_log_tag = #name

#define EAI_LOG_ERR(...) ESP_LOGE(_eai_log_tag, __VA_ARGS__)
#define EAI_LOG_WRN(...) ESP_LOGW(_eai_log_tag, __VA_ARGS__)
#define EAI_LOG_INF(...) ESP_LOGI(_eai_log_tag, __VA_ARGS__)
#define EAI_LOG_DBG(...) ESP_LOGD(_eai_log_tag, __VA_ARGS__)

#endif /* EAI_LOG_FREERTOS_H */
