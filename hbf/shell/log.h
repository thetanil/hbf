/* SPDX-License-Identifier: MIT */
#ifndef HBF_CORE_LOG_H
#define HBF_CORE_LOG_H

/*
 * Logging infrastructure with levels and timestamps.
 */

typedef enum {
	HBF_LOG_DEBUG = 0,
	HBF_LOG_INFO = 1,
	HBF_LOG_WARN = 2,
	HBF_LOG_ERROR = 3
} hbf_log_level_t;

/*
 * Initialize logging system.
 * @param level: Minimum log level to display
 */
void hbf_log_init(hbf_log_level_t level);

/*
 * Set current log level.
 * @param level: New minimum log level
 */
void hbf_log_set_level(hbf_log_level_t level);

/*
 * Get current log level.
 * @return Current minimum log level
 */
hbf_log_level_t hbf_log_get_level(void);

/*
 * Log a message at the specified level.
 * Format: [TIMESTAMP] [LEVEL] message
 */
void hbf_log(hbf_log_level_t level, const char *fmt, ...)
	__attribute__((format(printf, 2, 3)));

/* Convenience macros */
#define hbf_log_debug(...) hbf_log(HBF_LOG_DEBUG, __VA_ARGS__)
#define hbf_log_info(...)  hbf_log(HBF_LOG_INFO, __VA_ARGS__)
#define hbf_log_warn(...)  hbf_log(HBF_LOG_WARN, __VA_ARGS__)
#define hbf_log_error(...) hbf_log(HBF_LOG_ERROR, __VA_ARGS__)

/*
 * Parse log level from string.
 * @param level_str: String like "debug", "info", "warn", "error"
 * @return Log level, or HBF_LOG_INFO if invalid
 */
hbf_log_level_t hbf_log_parse_level(const char *level_str);

#endif /* HBF_CORE_LOG_H */
