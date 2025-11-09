/* SPDX-License-Identifier: MIT */
#ifndef HBF_CONFIG_H
#define HBF_CONFIG_H

typedef struct {
	int port;
	char log_level[16];
	int inmem;
} hbf_config_t;

/*
 * Parse command line arguments
 *
 * @param argc: Argument count
 * @param argv: Argument vector
 * @param config: Configuration structure to populate
 * @return 0 on success, 1 if help was shown, -1 on error
 */
int hbf_config_parse(int argc, char *argv[], hbf_config_t *config);

#endif /* HBF_CONFIG_H */
