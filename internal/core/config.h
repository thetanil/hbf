/* SPDX-License-Identifier: MIT */
#ifndef HBF_CORE_CONFIG_H
#define HBF_CORE_CONFIG_H

#include "log.h"
#include <stdbool.h>

/*
 * Global configuration structure for HBF.
 */
typedef struct {
	int port;
	hbf_log_level_t log_level;
	bool dev_mode;
	char storage_dir[256];
} hbf_config_t;

/*
 * Parse command-line arguments and populate config.
 *
 * @param config: Config structure to populate
 * @param argc: Argument count from main()
 * @param argv: Argument vector from main()
 * @return 0 on success, -1 on error, 1 if help was displayed
 */
int hbf_config_parse(hbf_config_t *config, int argc, char *argv[]);

/*
 * Initialize config with default values.
 *
 * @param config: Config structure to initialize
 */
void hbf_config_init(hbf_config_t *config);

/*
 * Print usage information.
 *
 * @param program_name: Name of the program (argv[0])
 */
void hbf_config_print_usage(const char *program_name);

#endif /* HBF_CORE_CONFIG_H */
