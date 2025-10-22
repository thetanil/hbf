/* SPDX-License-Identifier: MIT */
#include "config.h"
#include "hash.h"
#include "log.h"
#include "../henv/manager.h"
#include "../http/server.h"
#include "internal/qjs/engine.h"
#include "../db/db.h"
#include "../db/schema.h"
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <sqlite3.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static volatile bool g_shutdown = false;
static hbf_server_t *g_server = NULL;
static sqlite3 *g_default_db = NULL;
hbf_qjs_ctx_t *g_qjs_ctx = NULL;
pthread_mutex_t g_qjs_mutex = PTHREAD_MUTEX_INITIALIZER;

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

	/* Open default database using henv manager with time-based hash */
	/* This creates {storage_dir}/{hash(timestamp)}/index.db - unique per invocation */
	char time_str[64];
	char default_db_path[256];
	char user_hash[9];
	struct timespec ts;

	clock_gettime(CLOCK_REALTIME, &ts);
	ret = snprintf(time_str, sizeof(time_str), "%ld.%ld", ts.tv_sec, ts.tv_nsec);
	if (ret < 0 || (size_t)ret >= sizeof(time_str)) {
		hbf_log_error("Failed to format timestamp");
		hbf_henv_shutdown();
		return 1;
	}

	/* Generate hash from timestamp */
	ret = hbf_dns_safe_hash(time_str, user_hash);
	if (ret != 0) {
		hbf_log_error("Failed to generate hash for default database");
		hbf_henv_shutdown();
		return 1;
	}

	/* Create user pod if it doesn't exist */
	if (!hbf_henv_user_exists(user_hash)) {
		hbf_log_info("Creating user pod: %s", user_hash);
		ret = hbf_henv_create_user_pod(user_hash);
		if (ret != 0) {
			hbf_log_error("Failed to create user pod");
			hbf_henv_shutdown();
			return 1;
		}
	}

	/* Get database path */
	ret = hbf_henv_get_db_path(user_hash, default_db_path);
	if (ret != 0) {
		hbf_log_error("Failed to get default database path");
		hbf_henv_shutdown();
		return 1;
	}

	/* Open the database */
	ret = hbf_db_open(default_db_path, &g_default_db);
	if (ret != 0) {
		hbf_log_error("Failed to open default database at %s", default_db_path);
		hbf_henv_shutdown();
		return 1;
	}
	hbf_log_info("Opened default database: %s", default_db_path);

	/* Initialize database schema (skip if already exists) */
	ret = hbf_db_init_schema(g_default_db);
	if (ret != 0) {
		hbf_log_error("Failed to initialize database schema");
		hbf_db_close(g_default_db);
		hbf_henv_shutdown();
		return 1;
	}

	// Initialize QuickJS engine (memory limit: 64MB, timeout: 5000ms)
	ret = hbf_qjs_init(64, 5000);
	if (ret != 0) {
		hbf_log_error("Failed to initialize QuickJS engine");
		hbf_db_close(g_default_db);
		hbf_henv_shutdown();
		return 1;
	}

	// Create global QuickJS context with the main database
	g_qjs_ctx = hbf_qjs_ctx_create_with_db(g_default_db);
	if (!g_qjs_ctx) {
		hbf_log_error("Failed to create QuickJS context");
		hbf_qjs_shutdown();
		hbf_db_close(g_default_db);
		hbf_henv_shutdown();
		return 1;
	}

	// Load router.js and server.js from database (nodes table)
	hbf_log_info("Loading router.js and server.js...");

	// Query for router.js
	{
		sqlite3_stmt *stmt = NULL;
		const char *sql = "SELECT body FROM nodes WHERE name = 'router.js' AND type = 'js'";
		ret = sqlite3_prepare_v2(g_default_db, sql, -1, &stmt, NULL);
		if (ret == SQLITE_OK) {
			if (sqlite3_step(stmt) == SQLITE_ROW) {
				const char *code = (const char *)sqlite3_column_text(stmt, 0);
				int code_len = sqlite3_column_bytes(stmt, 0);
				if (code && code_len > 0) {
					ret = hbf_qjs_eval(g_qjs_ctx, code, (size_t)code_len, "router.js");
					if (ret != 0) {
						hbf_log_error("Failed to load router.js: %s",
							     hbf_qjs_get_error(g_qjs_ctx));
					} else {
						hbf_log_info("Loaded router.js (%d bytes)", code_len);
					}
				}
			} else {
				hbf_log_warn("router.js not found in database");
			}
			sqlite3_finalize(stmt);
		}
	}

	// Query for server.js
	{
		sqlite3_stmt *stmt = NULL;
		const char *sql = "SELECT body FROM nodes WHERE name = 'server.js' AND type = 'js'";
		ret = sqlite3_prepare_v2(g_default_db, sql, -1, &stmt, NULL);
		if (ret == SQLITE_OK) {
			if (sqlite3_step(stmt) == SQLITE_ROW) {
				const char *code = (const char *)sqlite3_column_text(stmt, 0);
				int code_len = sqlite3_column_bytes(stmt, 0);
				if (code && code_len > 0) {
					ret = hbf_qjs_eval(g_qjs_ctx, code, (size_t)code_len, "server.js");
					if (ret != 0) {
						hbf_log_error("Failed to load server.js: %s",
							     hbf_qjs_get_error(g_qjs_ctx));
					} else {
						hbf_log_info("Loaded server.js (%d bytes)", code_len);
					}
				}
			} else {
				hbf_log_warn("server.js not found in database - no routes configured");
			}
			sqlite3_finalize(stmt);
		}
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

	// Shutdown global QuickJS context and engine
	if (g_qjs_ctx) {
		hbf_qjs_ctx_destroy(g_qjs_ctx);
		g_qjs_ctx = NULL;
	}
	hbf_qjs_shutdown();

	/* Close default database */
	if (g_default_db) {
		hbf_db_close(g_default_db);
		g_default_db = NULL;
	}

	/* Shutdown henv manager */
	hbf_henv_shutdown();

	hbf_log_info("HBF stopped");

	return 0;
}
