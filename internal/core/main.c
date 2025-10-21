/* SPDX-License-Identifier: MIT */
#include "config.h"
#include "log.h"
#include "../henv/manager.h"
#include "../http/server.h"
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

static volatile bool g_shutdown = false;
static hbf_server_t *g_server = NULL;

static void signal_handler(int signum)
{
	(void)signum;
	g_shutdown = true;
}

int main(int argc, char *argv[])
{
	hbf_config_t config;
	struct sigaction sa;
	int ret;

	/* Parse configuration */
	ret = hbf_config_parse(&config, argc, argv);
	if (ret != 0) {
		return (ret < 0) ? 1 : 0;
	}

	/* Initialize logging */
	hbf_log_init(config.log_level);

	hbf_log_info("HBF v0.1.0 starting");
	hbf_log_info("Port: %d", config.port);
	hbf_log_info("Storage directory: %s", config.storage_dir);
	hbf_log_info("Log level: %d", config.log_level);
	hbf_log_info("Dev mode: %s", config.dev_mode ? "enabled" : "disabled");

	/* Initialize henv manager (max 100 cached connections) */
	if (hbf_henv_init(config.storage_dir, 100) != 0) {
		hbf_log_error("Failed to initialize henv manager");
		return 1;
	}

	/* Set up signal handlers for graceful shutdown */
	sa.sa_handler = signal_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	if (sigaction(SIGINT, &sa, NULL) < 0) {
		hbf_log_error("Failed to set SIGINT handler");
		return 1;
	}

	if (sigaction(SIGTERM, &sa, NULL) < 0) {
		hbf_log_error("Failed to set SIGTERM handler");
		return 1;
	}

	/* Start HTTP server */
	g_server = hbf_server_start(&config);
	if (!g_server) {
		hbf_log_error("Failed to start server");
		return 1;
	}

	hbf_log_info("Server running. Press Ctrl+C to stop.");

	/* Wait for shutdown signal */
	while (!g_shutdown) {
		sleep(1);
	}

	hbf_log_info("Shutdown signal received");

	/* Stop server */
	hbf_server_stop(g_server);
	g_server = NULL;

	/* Shutdown henv manager */
	hbf_henv_shutdown();

	hbf_log_info("HBF stopped");

	return 0;
}
