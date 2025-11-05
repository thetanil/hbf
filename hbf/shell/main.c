/* SPDX-License-Identifier: MIT */
#include "config.h"
#include "log.h"
#include "hbf/db/db.h"
#include "hbf/http/server.h"
#include "hbf/qjs/engine.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define HBF_QJS_MEMORY_LIMIT_MB 64
#define HBF_QJS_TIMEOUT_MS 5000

static volatile sig_atomic_t running = 1;

static void signal_handler(int sig)
{
	(void)sig;
	running = 0;
}

int main(int argc, char *argv[])
{
	hbf_config_t config;
	sqlite3 *db = NULL;
	hbf_server_t *server = NULL;
	int ret;

	/* Parse configuration */
	ret = hbf_config_parse(argc, argv, &config);
	if (ret != 0) {
		return (ret == 1) ? 0 : 1;
	}

	/* Initialize logging */
	hbf_log_init(hbf_log_parse_level(config.log_level));

	hbf_log_info("HBF starting (port=%d, inmem=%d, dev=%d)",
	             config.port, config.inmem, config.dev);

	/* Initialize database */
	ret = hbf_db_init(config.inmem, &db);
	if (ret != 0) {
		hbf_log_error("Failed to initialize database");
		return 1;
	}

	/* Initialize QuickJS engine */
	ret = hbf_qjs_init(HBF_QJS_MEMORY_LIMIT_MB, HBF_QJS_TIMEOUT_MS);
	if (ret != 0) {
		hbf_log_error("Failed to initialize QuickJS engine");
		hbf_db_close(db);
		return 1;
	}

	/* Create HTTP server */
	server = hbf_server_create(config.port, config.dev, db);
	if (!server) {
		hbf_log_error("Failed to create HTTP server");
		hbf_qjs_shutdown();
		hbf_db_close(db);
		return 1;
	}

	/* Start HTTP server */
	ret = hbf_server_start(server);
	if (ret != 0) {
		/* Error already logged by hbf_server_start() */
		hbf_server_destroy(server);
		hbf_qjs_shutdown();
		hbf_db_close(db);
		return 1;
	}

	/* Setup signal handlers */
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = signal_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	/* Main loop */
	hbf_log_info("HBF running (press Ctrl+C to stop)");
	while (running) {
		sleep(1);
	}

	/* Cleanup */
	hbf_log_info("Shutting down");
	hbf_server_destroy(server);
	hbf_qjs_shutdown();
	hbf_db_close(db);

	return 0;
}
