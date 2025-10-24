/* SPDX-License-Identifier: MIT */
#include "config.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(const char *program)
{
	printf("Usage: %s [options]\n", program);
	printf("Options:\n");
	printf("  --port PORT          HTTP server port (default: 5309)\n");
	printf("  --log-level LEVEL    Log level: debug, info, warn, error (default: info)\n");
	printf("  --dev                Enable development mode\n");
	printf("  --inmem              Use in-memory database (for testing)\n");
	printf("  --help, -h           Show this help message\n");
}

int hbf_config_parse(int argc, char *argv[], hbf_config_t *config)
{
	int i;

	if (!config) {
		return -1;
	}

	/* Set defaults */
	config->port = 5309;
	strncpy(config->log_level, "info", sizeof(config->log_level) - 1);
	config->dev = 0;
	config->inmem = 0;

	/* Parse arguments */
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
			print_usage(argv[0]);
			return 1;
		} else if (strcmp(argv[i], "--port") == 0) {
			if (i + 1 >= argc) {
				hbf_log_error("--port requires an argument");
				return -1;
			}
			config->port = atoi(argv[++i]);
			if (config->port <= 0 || config->port > 65535) {
				hbf_log_error("Invalid port: %d", config->port);
				return -1;
			}
		} else if (strcmp(argv[i], "--log-level") == 0) {
			if (i + 1 >= argc) {
				hbf_log_error("--log-level requires an argument");
				return -1;
			}
			strncpy(config->log_level, argv[++i],
			        sizeof(config->log_level) - 1);
		} else if (strcmp(argv[i], "--dev") == 0) {
			config->dev = 1;
		} else if (strcmp(argv[i], "--inmem") == 0) {
			config->inmem = 1;
		} else {
			hbf_log_error("Unknown option: %s", argv[i]);
			return -1;
		}
	}

	return 0;
}
