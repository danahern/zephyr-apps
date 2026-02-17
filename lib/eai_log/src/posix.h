/*
 * eai_log POSIX backend — fprintf to stderr with per-module level filtering.
 *
 * Uses ##__VA_ARGS__ GNU extension (GCC/Clang) for zero-arg format strings.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef EAI_LOG_POSIX_H
#define EAI_LOG_POSIX_H

#include <stdio.h>

#define EAI_LOG_MODULE_REGISTER(name, level) \
	static const char *_eai_log_tag = #name; \
	enum { _eai_log_level = (level) }

#define EAI_LOG_MODULE_DECLARE(name, level) \
	static const char *_eai_log_tag = #name; \
	enum { _eai_log_level = (level) }

#define EAI_LOG_ERR(fmt, ...) \
	do { \
		if (EAI_LOG_LEVEL_ERR <= _eai_log_level) \
			fprintf(stderr, "[ERR] %s: " fmt "\n", _eai_log_tag, ##__VA_ARGS__); \
	} while (0)

#define EAI_LOG_WRN(fmt, ...) \
	do { \
		if (EAI_LOG_LEVEL_WRN <= _eai_log_level) \
			fprintf(stderr, "[WRN] %s: " fmt "\n", _eai_log_tag, ##__VA_ARGS__); \
	} while (0)

#define EAI_LOG_INF(fmt, ...) \
	do { \
		if (EAI_LOG_LEVEL_INF <= _eai_log_level) \
			fprintf(stderr, "[INF] %s: " fmt "\n", _eai_log_tag, ##__VA_ARGS__); \
	} while (0)

#define EAI_LOG_DBG(fmt, ...) \
	do { \
		if (EAI_LOG_LEVEL_DBG <= _eai_log_level) \
			fprintf(stderr, "[DBG] %s: " fmt "\n", _eai_log_tag, ##__VA_ARGS__); \
	} while (0)

#endif /* EAI_LOG_POSIX_H */
