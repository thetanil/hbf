/* SPDX-License-Identifier: MIT */
#include "config.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_config_init(void)
{
	hbf_config_t config;

	hbf_config_init(&config);

	assert(config.port == 5309);
	assert(config.log_level == HBF_LOG_INFO);
	assert(config.dev_mode == false);
}

static void test_config_init_null(void)
{
	/* Should not crash */
	hbf_config_init(NULL);
}

static void test_config_parse_defaults(void)
{
	hbf_config_t config;
	char *argv[] = {(char *)"hbf"};
	int ret;

	ret = hbf_config_parse(&config, 1, argv);

	assert(ret == 0);
	assert(config.port == 5309);
	assert(config.log_level == HBF_LOG_INFO);
	assert(config.dev_mode == false);
}

static void test_config_parse_help(void)
{
	hbf_config_t config;
	char *argv[] = {(char *)"hbf", (char *)"--help"};
	int ret;

	ret = hbf_config_parse(&config, 2, argv);

	assert(ret == 1); /* --help returns 1 */
}

static void test_config_parse_help_short(void)
{
	hbf_config_t config;
	char *argv[] = {(char *)"hbf", (char *)"-h"};
	int ret;

	ret = hbf_config_parse(&config, 2, argv);

	assert(ret == 1); /* -h returns 1 */
}

static void test_config_parse_port(void)
{
	hbf_config_t config;
	char *argv[] = {(char *)"hbf", (char *)"--port", (char *)"8080"};
	int ret;

	ret = hbf_config_parse(&config, 3, argv);

	assert(ret == 0);
	assert(config.port == 8080);
}

static void test_config_parse_port_min(void)
{
	hbf_config_t config;
	char *argv[] = {(char *)"hbf", (char *)"--port", (char *)"1"};
	int ret;

	ret = hbf_config_parse(&config, 3, argv);

	assert(ret == 0);
	assert(config.port == 1);
}

static void test_config_parse_port_max(void)
{
	hbf_config_t config;
	char *argv[] = {(char *)"hbf", (char *)"--port", (char *)"65535"};
	int ret;

	ret = hbf_config_parse(&config, 3, argv);

	assert(ret == 0);
	assert(config.port == 65535);
}

static void test_config_parse_port_invalid_zero(void)
{
	hbf_config_t config;
	char *argv[] = {(char *)"hbf", (char *)"--port", (char *)"0"};
	int ret;

	ret = hbf_config_parse(&config, 3, argv);

	assert(ret == -1); /* Invalid port */
}

static void test_config_parse_port_invalid_negative(void)
{
	hbf_config_t config;
	char *argv[] = {(char *)"hbf", (char *)"--port", (char *)"-1"};
	int ret;

	ret = hbf_config_parse(&config, 3, argv);

	assert(ret == -1); /* Invalid port */
}

static void test_config_parse_port_invalid_too_large(void)
{
	hbf_config_t config;
	char *argv[] = {(char *)"hbf", (char *)"--port", (char *)"65536"};
	int ret;

	ret = hbf_config_parse(&config, 3, argv);

	assert(ret == -1); /* Port too large */
}

static void test_config_parse_port_invalid_not_number(void)
{
	hbf_config_t config;
	char *argv[] = {(char *)"hbf", (char *)"--port", (char *)"abc"};
	int ret;

	ret = hbf_config_parse(&config, 3, argv);

	assert(ret == -1); /* Not a number */
}

static void test_config_parse_port_invalid_partial_number(void)
{
	hbf_config_t config;
	char *argv[] = {(char *)"hbf", (char *)"--port", (char *)"123abc"};
	int ret;

	ret = hbf_config_parse(&config, 3, argv);

	assert(ret == -1); /* Partial number not allowed */
}

static void test_config_parse_port_missing_arg(void)
{
	hbf_config_t config;
	char *argv[] = {(char *)"hbf", (char *)"--port"};
	int ret;

	ret = hbf_config_parse(&config, 2, argv);

	assert(ret == -1); /* Missing argument */
}

static void test_config_parse_log_level_debug(void)
{
	hbf_config_t config;
	char *argv[] = {(char *)"hbf", (char *)"--log_level", (char *)"debug"};
	int ret;

	ret = hbf_config_parse(&config, 3, argv);

	assert(ret == 0);
	assert(config.log_level == HBF_LOG_DEBUG);
}

static void test_config_parse_log_level_info(void)
{
	hbf_config_t config;
	char *argv[] = {(char *)"hbf", (char *)"--log_level", (char *)"info"};
	int ret;

	ret = hbf_config_parse(&config, 3, argv);

	assert(ret == 0);
	assert(config.log_level == HBF_LOG_INFO);
}

static void test_config_parse_log_level_warn(void)
{
	hbf_config_t config;
	char *argv[] = {(char *)"hbf", (char *)"--log_level", (char *)"warn"};
	int ret;

	ret = hbf_config_parse(&config, 3, argv);

	assert(ret == 0);
	assert(config.log_level == HBF_LOG_WARN);
}

static void test_config_parse_log_level_error(void)
{
	hbf_config_t config;
	char *argv[] = {(char *)"hbf", (char *)"--log_level", (char *)"error"};
	int ret;

	ret = hbf_config_parse(&config, 3, argv);

	assert(ret == 0);
	assert(config.log_level == HBF_LOG_ERROR);
}

static void test_config_parse_log_level_invalid(void)
{
	hbf_config_t config;
	char *argv[] = {(char *)"hbf", (char *)"--log_level", (char *)"invalid"};
	int ret;

	ret = hbf_config_parse(&config, 3, argv);

	assert(ret == 0); /* Defaults to INFO for invalid values */
	assert(config.log_level == HBF_LOG_INFO);
}

static void test_config_parse_log_level_missing_arg(void)
{
	hbf_config_t config;
	char *argv[] = {(char *)"hbf", (char *)"--log_level"};
	int ret;

	ret = hbf_config_parse(&config, 2, argv);

	assert(ret == -1); /* Missing argument */
}

static void test_config_parse_dev_mode(void)
{
	hbf_config_t config;
	char *argv[] = {(char *)"hbf", (char *)"--dev"};
	int ret;

	ret = hbf_config_parse(&config, 2, argv);

	assert(ret == 0);
	assert(config.dev_mode == true);
}

static void test_config_parse_unknown_option(void)
{
	hbf_config_t config;
	char *argv[] = {(char *)"hbf", (char *)"--unknown"};
	int ret;

	ret = hbf_config_parse(&config, 2, argv);

	assert(ret == -1); /* Unknown option */
}

static void test_config_parse_multiple_options(void)
{
	hbf_config_t config;
	char *argv[] = {(char *)"hbf", (char *)"--port", (char *)"9000", (char *)"--log_level", (char *)"debug", (char *)"--dev"};
	int ret;

	ret = hbf_config_parse(&config, 6, argv);

	assert(ret == 0);
	assert(config.port == 9000);
	assert(config.log_level == HBF_LOG_DEBUG);
	assert(config.dev_mode == true);
}

static void test_config_parse_null_config(void)
{
	char *argv[] = {(char *)"hbf"};
	int ret;

	ret = hbf_config_parse(NULL, 1, argv);

	assert(ret == -1); /* NULL config */
}

static void test_config_parse_null_argv(void)
{
	hbf_config_t config;
	int ret;

	ret = hbf_config_parse(&config, 1, NULL);

	assert(ret == -1); /* NULL argv */
}

int main(void)
{
	printf("Running config_test.c:\n");

	test_config_init();
	test_config_init_null();
	test_config_parse_defaults();
	test_config_parse_help();
	test_config_parse_help_short();
	test_config_parse_port();
	test_config_parse_port_min();
	test_config_parse_port_max();
	test_config_parse_port_invalid_zero();
	test_config_parse_port_invalid_negative();
	test_config_parse_port_invalid_too_large();
	test_config_parse_port_invalid_not_number();
	test_config_parse_port_invalid_partial_number();
	test_config_parse_port_missing_arg();
	test_config_parse_log_level_debug();
	test_config_parse_log_level_info();
	test_config_parse_log_level_warn();
	test_config_parse_log_level_error();
	test_config_parse_log_level_invalid();
	test_config_parse_log_level_missing_arg();
	test_config_parse_dev_mode();
	test_config_parse_unknown_option();
	test_config_parse_multiple_options();
	test_config_parse_null_config();
	test_config_parse_null_argv();

	printf("\nAll tests passed!\n");
	return 0;
}
