/* SPDX-License-Identifier: MIT */
#include "server.h"
#include "internal/http/handler.h"
#include "../core/log.h"
#include "../db/db.h"
#include "civetweb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>



/* MIME type helper */
static const char *get_mime_type(const char *path)
{
	const char *ext = strrchr(path, '.');
	if (!ext) {
		return "application/octet-stream";
	}

	if (strcmp(ext, ".html") == 0) {
		return "text/html";
	}
	if (strcmp(ext, ".css") == 0) {
		return "text/css";
	}
	if (strcmp(ext, ".js") == 0) {
		return "application/javascript";
	}
	if (strcmp(ext, ".json") == 0) {
		return "application/json";
	}
	if (strcmp(ext, ".png") == 0) {
		return "image/png";
	}
	if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) {
		return "image/jpeg";
	}
	if (strcmp(ext, ".svg") == 0) {
		return "image/svg+xml";
	}
	if (strcmp(ext, ".ico") == 0) {
		return "image/x-icon";
	}
	if (strcmp(ext, ".woff") == 0) {
		return "font/woff";
	}
	if (strcmp(ext, ".woff2") == 0) {
		return "font/woff2";
	}
	if (strcmp(ext, ".wasm") == 0) {
		return "application/wasm";
	}
	if (strcmp(ext, ".md") == 0) {
		return "text/markdown";
	}

	return "application/octet-stream";
}

/* Static file handler - serves files from SQLAR archive */
static int static_handler(struct mg_connection *conn, void *cbdata)
{
	hbf_server_t *server = (hbf_server_t *)cbdata;
	const struct mg_request_info *ri = mg_get_request_info(conn);
	const char *uri;
	char path[512];
	unsigned char *data = NULL;
	size_t size;
	const char *mime_type;
	int ret;

	if (!ri) {
		mg_send_http_error(conn, 500, "Internal error");
		return 500;
	}

	uri = ri->local_uri;

	/* Map URL to archive path */
	if (strcmp(uri, "/") == 0) {
		snprintf(path, sizeof(path), "static/index.html");
	} else {
		/* Remove leading slash */
		snprintf(path, sizeof(path), "%s", uri + 1);
	}

	hbf_log_debug("Static request: %s -> %s", uri, path);

	/* Read file from main database SQLAR */
	ret = hbf_db_read_file_from_main(server->db, path, &data, &size);
	if (ret != 0) {
		hbf_log_debug("File not found: %s", path);
		mg_send_http_error(conn, 404, "Not Found");
		return 404;
	}

	/* Determine MIME type */
	mime_type = get_mime_type(path);

	/* Set cache headers based on dev mode */
	const char *cache_header = server->dev
	    ? "Cache-Control: no-store\r\n"
	    : "Cache-Control: public, max-age=3600\r\n";

	/* Send response */
	mg_printf(conn,
	          "HTTP/1.1 200 OK\r\n"
	          "Content-Type: %s\r\n"
	          "Content-Length: %zu\r\n"
	          "%s"
	          "Connection: close\r\n"
	          "\r\n",
	          mime_type, size, cache_header);

	mg_write(conn, data, size);
	free(data);

	hbf_log_debug("Served: %s (%zu bytes, %s)", path, size, mime_type);
	return 200;
}

/* Health check handler */
static int health_handler(struct mg_connection *conn, void *cbdata)
{
	const char *response = "{\"status\":\"ok\"}";

	(void)cbdata;

	mg_printf(conn,
	          "HTTP/1.1 200 OK\r\n"
	          "Content-Type: application/json\r\n"
	          "Content-Length: %zu\r\n"
	          "Connection: close\r\n"
	          "\r\n%s",
	          strlen(response), response);

	return 200;
}

hbf_server_t *hbf_server_create(int port, int dev, sqlite3 *db)
{
	hbf_server_t *server;

	if (!db) {
		hbf_log_error("NULL database handle");
		return NULL;
	}

	server = calloc(1, sizeof(hbf_server_t));
	if (!server) {
		hbf_log_error("Failed to allocate server");
		return NULL;
	}

	server->port = port;
	server->dev = dev;  /* Store dev flag */
	server->db = db;
	server->ctx = NULL;

	return server;
}

int hbf_server_start(hbf_server_t *server)
{
	char port_str[16];
	const char *options[] = {
		"listening_ports", port_str,
		"num_threads", "4",
		NULL
	};

	if (!server) {
		return -1;
	}

	snprintf(port_str, sizeof(port_str), "%d", server->port);

	server->ctx = mg_start(NULL, 0, options);
	if (!server->ctx) {
		hbf_log_error("Failed to start HTTP server");
		return -1;
	}

	/* Register handlers */
	mg_set_request_handler(server->ctx, "/health", health_handler, server);
	mg_set_request_handler(server->ctx, "/static/**", static_handler, server);
	mg_set_request_handler(server->ctx, "**", hbf_qjs_request_handler, server);

	hbf_log_info("HTTP server listening at http://localhost:%d/", server->port);
	return 0;
}

void hbf_server_stop(hbf_server_t *server)
{
	if (server && server->ctx) {
		mg_stop(server->ctx);
		server->ctx = NULL;
		hbf_log_info("HTTP server stopped");
	}
}

void hbf_server_destroy(hbf_server_t *server)
{
	if (server) {
		if (server->ctx) {
			hbf_server_stop(server);
		}
		free(server);
	}
}
