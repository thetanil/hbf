/* SPDX-License-Identifier: MIT */
#include "config.h"
#include "log.h"
#include "../db/db.h"
#include "../http/server.h"
#include "../qjs/engine.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

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
	sqlite3 *fs_db = NULL;
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
	ret = hbf_db_init(config.inmem, &db, &fs_db);
	if (ret != 0) {
		hbf_log_error("Failed to initialize database");
		return 1;
	}

	/* Initialize QuickJS engine (64 MB memory limit, 5000 ms timeout) */
	ret = hbf_qjs_init(64, 5000);
	if (ret != 0) {
		hbf_log_error("Failed to initialize QuickJS engine");
		hbf_db_close(db, fs_db);
		return 1;
	}

	/* Create HTTP server */
	server = hbf_server_create(config.port, db);
	if (!server) {
		hbf_log_error("Failed to create HTTP server");
		hbf_db_close(db, fs_db);
		return 1;
	}

	/* Start HTTP server */
	ret = hbf_server_start(server);
	if (ret != 0) {
		hbf_log_error("Failed to start HTTP server");
		hbf_server_destroy(server);
		hbf_db_close(db, fs_db);
		return 1;
	}

	/* Setup signal handlers */
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	/* Main loop */
	hbf_log_info("HBF running (press Ctrl+C to stop)");
	while (running) {
		sleep(1);
	}

	/* Cleanup */
	hbf_log_info("Shutting down");
	hbf_server_destroy(server);
	hbf_qjs_shutdown();
	hbf_db_close(db, fs_db);

	return 0;
}
