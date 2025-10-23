/* SPDX-License-Identifier: MIT */
#include "server.h"
#include "../core/log.h"
#include "handler.h"
#include "../db/db.h"
#include <civetweb.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

struct hbf_server {
	struct mg_context *ctx;
	time_t start_time;
	int port;
	char db_path[256]; /* Path to default database for static file serving */
};

/* Thread-local storage for read-only database connections */
static pthread_key_t g_thread_db_key;
static pthread_once_t g_thread_db_key_once = PTHREAD_ONCE_INIT;

/* Initialize thread-local storage key */
static void init_thread_db_key(void)
{
	pthread_key_create(&g_thread_db_key, NULL);
}

/* Get or create thread-local read-only DB connection */
static sqlite3 *get_thread_db(const char *db_path)
{
	sqlite3 *db;
	int ret;

	pthread_once(&g_thread_db_key_once, init_thread_db_key);

	db = pthread_getspecific(g_thread_db_key);
	if (db) {
		return db;
	}

	/* Open read-only connection */
	ret = sqlite3_open_v2(db_path, &db,
			      SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX, NULL);
	if (ret != SQLITE_OK) {
		hbf_log_error("Failed to open read-only DB for thread: %s",
			      sqlite3_errmsg(db));
		if (db) {
			sqlite3_close(db);
		}
		return NULL;
	}

	pthread_setspecific(g_thread_db_key, db);
	hbf_log_debug("Opened thread-local read-only DB connection");
	return db;
}

/* Get MIME type from file extension */
static const char *get_mime_type(const char *path)
{
	const char *ext;

	ext = strrchr(path, '.');
	if (!ext) {
		return "application/octet-stream";
	}

	ext++; /* Skip the dot */

	if (strcmp(ext, "html") == 0 || strcmp(ext, "htm") == 0) {
		return "text/html";
	} else if (strcmp(ext, "css") == 0) {
		return "text/css";
	} else if (strcmp(ext, "js") == 0) {
		return "application/javascript";
	} else if (strcmp(ext, "json") == 0) {
		return "application/json";
	} else if (strcmp(ext, "png") == 0) {
		return "image/png";
	} else if (strcmp(ext, "jpg") == 0 || strcmp(ext, "jpeg") == 0) {
		return "image/jpeg";
	} else if (strcmp(ext, "gif") == 0) {
		return "image/gif";
	} else if (strcmp(ext, "svg") == 0) {
		return "image/svg+xml";
	} else if (strcmp(ext, "ico") == 0) {
		return "image/x-icon";
	} else if (strcmp(ext, "woff") == 0) {
		return "font/woff";
	} else if (strcmp(ext, "woff2") == 0) {
		return "font/woff2";
	} else if (strcmp(ext, "ttf") == 0) {
		return "font/ttf";
	} else if (strcmp(ext, "txt") == 0) {
		return "text/plain";
	} else if (strcmp(ext, "xml") == 0) {
		return "application/xml";
	}

	return "application/octet-stream";
}

/* Forward declarations */
static int health_handler(struct mg_connection *conn, void *cbdata);
static int static_file_handler(struct mg_connection *conn, void *cbdata);

hbf_server_t *hbf_server_start(const hbf_config_t *config, const char *db_path)
{
	hbf_server_t *server;
	char port_str[16];
	const char *options[] = {
		"listening_ports", port_str,
		"num_threads", "4",
		"request_timeout_ms", "10000",
		"enable_keep_alive", "yes",
		NULL
	};

	if (!config) {
		hbf_log_error("Invalid config");
		return NULL;
	}

	if (!db_path) {
		hbf_log_error("Invalid db_path");
		return NULL;
	}

	server = calloc(1, sizeof(hbf_server_t));
	if (!server) {
		hbf_log_error("Failed to allocate server structure");
		return NULL;
	}

	server->port = config->port;
	server->start_time = time(NULL);

	/* Store database path for static file serving */
	strncpy(server->db_path, db_path, sizeof(server->db_path) - 1);
	server->db_path[sizeof(server->db_path) - 1] = '\0';

	/* Convert port to string */
	snprintf(port_str, sizeof(port_str), "%d", config->port);

	/* Initialize CivetWeb */
	mg_init_library(0);

	/* Start server */
	server->ctx = mg_start(NULL, NULL, options);
	if (!server->ctx) {
		hbf_log_error("Failed to start HTTP server on port %d", config->port);
		free(server);
		mg_exit_library();
		return NULL;
	}

	/* Register handlers (order matters - first match wins) */
	mg_set_request_handler(server->ctx, "/health", health_handler, server);
	mg_set_request_handler(server->ctx, "/static/**", static_file_handler, server);
	/* All other routes handled by QuickJS */
	mg_set_request_handler(server->ctx, "**", hbf_qjs_request_handler, server);

	hbf_log_info("HTTP server started on port %d", config->port);

	return server;
}

void hbf_server_stop(hbf_server_t *server)
{
	if (!server) {
		return;
	}

	hbf_log_info("Stopping HTTP server");

	if (server->ctx) {
		mg_stop(server->ctx);
	}

	mg_exit_library();
	free(server);

	hbf_log_info("HTTP server stopped");
}

unsigned long hbf_server_uptime(const hbf_server_t *server)
{
	time_t now;

	if (!server) {
		return 0;
	}

	now = time(NULL);
	return (unsigned long)(now - server->start_time);
}

/* Static file handler - serves files from nodes table */
static int static_file_handler(struct mg_connection *conn, void *cbdata)
{
	hbf_server_t *server;
	const struct mg_request_info *ri;
	sqlite3 *db;
	sqlite3_stmt *stmt;
	const char *uri;
	const char *file_path;
	const char *mime_type;
	const char *body;
	int body_len;
	int ret;

	server = (hbf_server_t *)cbdata;
	ri = mg_get_request_info(conn);
	if (!ri) {
		return 500;
	}

	uri = ri->local_uri;

	/* Strip /static/ prefix to get file path */
	if (strncmp(uri, "/static/", 8) == 0) {
		file_path = uri + 8; /* Skip "/static/" */
	} else {
		file_path = uri + 1; /* Skip leading "/" */
	}

	hbf_log_debug("Static file request: %s (path=%s)", uri, file_path);

	/* Get thread-local read-only DB connection */
	db = get_thread_db(server->db_path);
	if (!db) {
		mg_send_http_error(conn, 500, "Database error");
		return 500;
	}

	/* Query nodes table for file - extract content from JSON body */
	ret = sqlite3_prepare_v2(db,
		"SELECT json_extract(body, '$.content') FROM nodes WHERE name = ? AND type = 'static'",
		-1, &stmt, NULL);
	if (ret != SQLITE_OK) {
		hbf_log_error("Failed to prepare static file query: %s",
			      sqlite3_errmsg(db));
		mg_send_http_error(conn, 500, "Database error");
		return 500;
	}

	ret = sqlite3_bind_text(stmt, 1, file_path, -1, SQLITE_STATIC);
	if (ret != SQLITE_OK) {
		hbf_log_error("Failed to bind file path: %s", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		mg_send_http_error(conn, 500, "Database error");
		return 500;
	}

	ret = sqlite3_step(stmt);
	if (ret != SQLITE_ROW) {
		/* File not found */
		hbf_log_debug("Static file not found: %s", file_path);
		sqlite3_finalize(stmt);
		mg_send_http_error(conn, 404, "File not found");
		return 404;
	}

	/* Get file content */
	body = (const char *)sqlite3_column_text(stmt, 0);
	body_len = sqlite3_column_bytes(stmt, 0);

	if (!body || body_len == 0) {
		hbf_log_warn("Empty static file: %s", file_path);
		sqlite3_finalize(stmt);
		mg_send_http_error(conn, 500, "Empty file");
		return 500;
	}

	/* Determine MIME type */
	mime_type = get_mime_type(file_path);

	/* Send response */
	mg_printf(conn,
		"HTTP/1.1 200 OK\r\n"
		"Content-Type: %s\r\n"
		"Content-Length: %d\r\n"
		"Cache-Control: public, max-age=3600\r\n"
		"Connection: close\r\n"
		"\r\n",
		mime_type,
		body_len);

	mg_write(conn, body, (size_t)body_len);

	sqlite3_finalize(stmt);

	hbf_log_debug("Served static file: %s (%d bytes, %s)",
		      file_path, body_len, mime_type);

	return 200;
}

/* Health endpoint handler */
static int health_handler(struct mg_connection *conn, void *cbdata)
{
	hbf_server_t *server;
	char response[512];
	unsigned long uptime;

	server = (hbf_server_t *)cbdata;
	uptime = hbf_server_uptime(server);

	/* Build JSON response */
	snprintf(response, sizeof(response),
		"{\"status\":\"ok\",\"version\":\"0.1.0\",\"uptime_seconds\":%lu}",
		uptime);

	/* Send response */
	mg_printf(conn,
		"HTTP/1.1 200 OK\r\n"
		"Content-Type: application/json\r\n"
		"Content-Length: %d\r\n"
		"Connection: close\r\n"
		"\r\n"
		"%s",
		(int)strlen(response),
		response);

	return 200;
}
