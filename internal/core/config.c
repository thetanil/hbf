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
	strncpy(config->storage_dir, "./henvs", sizeof(config->storage_dir) - 1);
	config->storage_dir[sizeof(config->storage_dir) - 1] = '\0';
}

void hbf_config_print_usage(const char *program_name)
{
	printf("Usage: %s [options]\n", program_name);
	printf("\n");
	printf("Options:\n");
	printf("  --port <num>         HTTP server port (default: 5309)\n");
	printf("  --storage_dir <path> Directory for user pod storage (default: ./henvs)\n");
	printf("  --log_level <level>  Log level: debug, info, warn, error (default: info)\n");
	printf("  --dev                Enable development mode\n");
	printf("  --help               Show this help message\n");
	printf("\n");
}

/* Helper: parse port argument */
static int parse_port(const char *arg)
{
	char *endptr;
	long port_long;

	port_long = strtol(arg, &endptr, 10);
	if (endptr == arg || *endptr != '\0' || port_long <= 0 || port_long > 65535) {
		return -1;
	}
	return (int)port_long;
}

/* Helper: set storage_dir in config */
static int set_storage_dir(hbf_config_t *config, const char *path)
{
	if (strlen(path) >= sizeof(config->storage_dir)) {
		return -1;
	}
	strncpy(config->storage_dir, path, sizeof(config->storage_dir) - 1);
	config->storage_dir[sizeof(config->storage_dir) - 1] = '\0';
	return 0;
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
		const char *arg = argv[i];

		if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
			hbf_config_print_usage(argv[0]);
			return 1;
		}
		if (strcmp(arg, "--dev") == 0) {
			config->dev_mode = true;
			continue;
		}

		/* Check arguments that require values */
		if (i + 1 >= argc &&
		    (strcmp(arg, "--port") == 0 || strcmp(arg, "--storage_dir") == 0 ||
		     strcmp(arg, "--log_level") == 0)) {
			fprintf(stderr, "Error: %s requires an argument\n", arg);
			return -1;
		}

		if (strcmp(arg, "--port") == 0) {
			int port = parse_port(argv[++i]);
			if (port < 0) {
				fprintf(stderr, "Error: Invalid port number: %s\n", argv[i]);
				return -1;
			}
			config->port = port;
		} else if (strcmp(arg, "--storage_dir") == 0) {
			if (set_storage_dir(config, argv[++i]) != 0) {
				fprintf(stderr, "Error: storage_dir path too long: %s\n", argv[i]);
				return -1;
			}
		} else if (strcmp(arg, "--log_level") == 0) {
			config->log_level = hbf_log_parse_level(argv[++i]);
		} else {
			fprintf(stderr, "Error: Unknown option: %s\n", arg);
			hbf_config_print_usage(argv[0]);
			return -1;
		}
	}

	return 0;
}
