/* SPDX-License-Identifier: MIT */
#include "config.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_config_parse_defaults(void)
{
	hbf_config_t config;
	char *argv[] = {(char *)"hbf"};
	int ret;

	ret = hbf_config_parse(1, argv, &config);

	assert(ret == 0);
	assert(config.port == 5309);
	assert(strcmp(config.log_level, "info") == 0);
	assert(config.dev == 0);
	assert(config.inmem == 0);

	printf("  ✓ Config defaults\n");
}

static void test_config_parse_help(void)
{
	hbf_config_t config;
	char *argv[] = {(char *)"hbf", (char *)"--help"};
	int ret;

	ret = hbf_config_parse(2, argv, &config);

	assert(ret == 1); /* --help returns 1 */

	printf("  ✓ Help flag\n");
}

static void test_config_parse_port(void)
{
	hbf_config_t config;
	char *argv[] = {(char *)"hbf", (char *)"--port", (char *)"8080"};
	int ret;

	ret = hbf_config_parse(3, argv, &config);

	assert(ret == 0);
	assert(config.port == 8080);

	printf("  ✓ Port parsing\n");
}

static void test_config_parse_log_level(void)
{
	hbf_config_t config;
	char *argv[] = {(char *)"hbf", (char *)"--log-level", (char *)"debug"};
	int ret;

	ret = hbf_config_parse(3, argv, &config);

	assert(ret == 0);
	assert(strcmp(config.log_level, "debug") == 0);

	printf("  ✓ Log level parsing\n");
}

static void test_config_parse_dev(void)
{
	hbf_config_t config;
	char *argv[] = {(char *)"hbf", (char *)"--dev"};
	int ret;

	ret = hbf_config_parse(2, argv, &config);

	assert(ret == 0);
	assert(config.dev == 1);

	printf("  ✓ Dev flag\n");
}

static void test_config_parse_inmem(void)
{
	hbf_config_t config;
	char *argv[] = {(char *)"hbf", (char *)"--inmem"};
	int ret;

	ret = hbf_config_parse(2, argv, &config);

	assert(ret == 0);
	assert(config.inmem == 1);

	printf("  ✓ Inmem flag\n");
}

static void test_config_parse_combined(void)
{
	hbf_config_t config;
	char *argv[] = {
		(char *)"hbf",
		(char *)"--port", (char *)"3000",
		(char *)"--log-level", (char *)"warn",
		(char *)"--dev",
		(char *)"--inmem"
	};
	int ret;

	ret = hbf_config_parse(7, argv, &config);

	assert(ret == 0);
	assert(config.port == 3000);
	assert(strcmp(config.log_level, "warn") == 0);
	assert(config.dev == 1);
	assert(config.inmem == 1);

	printf("  ✓ Combined flags\n");
}

int main(void)
{
	printf("Config tests:\n");

	test_config_parse_defaults();
	test_config_parse_help();
	test_config_parse_port();
	test_config_parse_log_level();
	test_config_parse_dev();
	test_config_parse_inmem();
	test_config_parse_combined();

	printf("\nAll config tests passed!\n");
	return 0;
}
