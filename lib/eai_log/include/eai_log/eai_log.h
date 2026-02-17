/*
 * eai_log — Portable logging macros
 *
 * Header-only library that compiles to the native logging system on each
 * platform. Backend selected at compile time via Kconfig (Zephyr) or
 * compile definitions (ESP-IDF, POSIX).
 *
 * Usage:
 *   EAI_LOG_MODULE_REGISTER(my_module, EAI_LOG_LEVEL_INF);  // one per .c file
 *   EAI_LOG_INF("Connected to %s", ssid);
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef EAI_LOG_H
#define EAI_LOG_H

/* Log levels — numeric values match Zephyr's LOG_LEVEL_* */
#define EAI_LOG_LEVEL_NONE 0
#define EAI_LOG_LEVEL_ERR  1
#define EAI_LOG_LEVEL_WRN  2
#define EAI_LOG_LEVEL_INF  3
#define EAI_LOG_LEVEL_DBG  4

/* Backend dispatch */
#if defined(CONFIG_EAI_LOG_BACKEND_ZEPHYR)
#include "../../src/zephyr.h"
#elif defined(CONFIG_EAI_LOG_BACKEND_FREERTOS)
#include "../../src/freertos.h"
#elif defined(CONFIG_EAI_LOG_BACKEND_POSIX)
#include "../../src/posix.h"
#else
#error "No EAI_LOG backend selected. Set CONFIG_EAI_LOG_BACKEND_ZEPHYR, CONFIG_EAI_LOG_BACKEND_FREERTOS, or CONFIG_EAI_LOG_BACKEND_POSIX."
#endif

#endif /* EAI_LOG_H */
