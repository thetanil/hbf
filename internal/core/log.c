/* SPDX-License-Identifier: MIT */
#include "log.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static hbf_log_level_t current_level = HBF_LOG_INFO;

static const char *level_names[] = {
	"DEBUG",
	"INFO",
	"WARN",
	"ERROR"
};

void hbf_log_init(hbf_log_level_t level)
{
	current_level = level;
}

void hbf_log_set_level(hbf_log_level_t level)
{
	current_level = level;
}

hbf_log_level_t hbf_log_get_level(void)
{
	return current_level;
}

void hbf_log(hbf_log_level_t level, const char *fmt, ...)
{
	va_list args;
	time_t now;
	struct tm tm_info;
	char timestamp[32];

	if (level < current_level) {
		return;
	}

	/* Get timestamp */
	now = time(NULL);
	(void)gmtime_r(&now, &tm_info);
	(void)strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm_info);

	/* Print log prefix */
	fprintf(stderr, "[%s] [%s] ", timestamp, level_names[level]);

	/* Print message */
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);

	fprintf(stderr, "\n");
	fflush(stderr);
}

hbf_log_level_t hbf_log_parse_level(const char *level_str)
{
	if (!level_str) {
		return HBF_LOG_INFO;
	}

	if (strcmp(level_str, "debug") == 0) {
		return HBF_LOG_DEBUG;
	} else if (strcmp(level_str, "info") == 0) {
		return HBF_LOG_INFO;
	} else if (strcmp(level_str, "warn") == 0) {
		return HBF_LOG_WARN;
	} else if (strcmp(level_str, "error") == 0) {
		return HBF_LOG_ERROR;
	}

	return HBF_LOG_INFO;
}
