/* SPDX-License-Identifier: MIT */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void hbf_config_init(hbf_config_t *config)
{
	if (!config) {
		return;
	}

	config->port = 5309;
	config->log_level = HBF_LOG_INFO;
	config->dev_mode = false;
}

void hbf_config_print_usage(const char *program_name)
{
	printf("Usage: %s [options]\n", program_name);
	printf("\n");
	printf("Options:\n");
	printf("  --port <num>         HTTP server port (default: 5309)\n");
	printf("  --log_level <level>  Log level: debug, info, warn, error (default: info)\n");
	printf("  --dev                Enable development mode\n");
	printf("  --help               Show this help message\n");
	printf("\n");
}

int hbf_config_parse(hbf_config_t *config, int argc, char *argv[])
{
	int i;

	if (!config || !argv) {
		return -1;
	}

	/* Initialize with defaults */
	hbf_config_init(config);

	/* Parse arguments */
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
			hbf_config_print_usage(argv[0]);
			return 1;
		} else if (strcmp(argv[i], "--port") == 0) {
			if (i + 1 >= argc) {
				fprintf(stderr, "Error: --port requires an argument\n");
				return -1;
			}
			config->port = atoi(argv[++i]);
			if (config->port <= 0 || config->port > 65535) {
				fprintf(stderr, "Error: Invalid port number: %d\n", config->port);
				return -1;
			}
		} else if (strcmp(argv[i], "--log_level") == 0) {
			if (i + 1 >= argc) {
				fprintf(stderr, "Error: --log_level requires an argument\n");
				return -1;
			}
			config->log_level = hbf_log_parse_level(argv[++i]);
		} else if (strcmp(argv[i], "--dev") == 0) {
			config->dev_mode = true;
		} else {
			fprintf(stderr, "Error: Unknown option: %s\n", argv[i]);
			hbf_config_print_usage(argv[0]);
			return -1;
		}
	}

	return 0;
}
