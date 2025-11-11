/* SPDX-License-Identifier: MIT */
#include "server.h"
#include "hbf/http/handler.h"
#include "hbf/shell/log.h"
#include "hbf/db/overlay_fs.h"
#include "hbf/db/db.h"
#include "libhttp.h"
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

/* Static file handler - serves files from database */
static int static_handler(struct lh_con_t *conn, void *cbdata)
{
	const struct lh_rqi_t *ri = lh_get_request_info(conn);
	(void)cbdata;
	const char *uri;
	char path[512];
	unsigned char *data = NULL;
	size_t size;
	const char *mime_type;
	int ret;

	if (!ri) {
		lh_send_http_error(conn, 500, "Internal error");
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

	/* Read file from database */
	/* NO MUTEX - static file serving stays parallel */
	ret = overlay_fs_read_file(path, 1, &data, &size);
	if (ret != 0) {
		hbf_log_debug("File not found: %s", path);
		lh_send_http_error(conn, 404, "Not Found");
		return 404;
	}

	/* Determine MIME type */
	mime_type = get_mime_type(path);

	/* Send response */
	lh_printf(conn,
	          "HTTP/1.1 200 OK\r\n"
	          "Content-Type: %s\r\n"
	          "Content-Length: %zu\r\n"
	          "Cache-Control: public, max-age=3600\r\n"
	          "Connection: close\r\n"
	          "\r\n",
	          mime_type, size);

	lh_write(conn, data, size);
	free(data);

	hbf_log_debug("Served: %s (%zu bytes, %s)", path, size, mime_type);
	return 200;
}

/* Health check handler */
static int health_handler(struct lh_con_t *conn, void *cbdata)
{
	const char *response = "{\"status\":\"ok\"}";

	(void)cbdata;

	lh_printf(conn,
	          "HTTP/1.1 200 OK\r\n"
	          "Content-Type: application/json\r\n"
	          "Content-Length: %zu\r\n"
	          "Connection: close\r\n"
	          "\r\n%s",
	          strlen(response), response);

	return 200;
}

hbf_server_t *hbf_server_create(int port, sqlite3 *db)
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

	server->ctx = lh_start(NULL, 0, options);
	if (!server->ctx) {
		hbf_log_error("Failed to start HTTP server on port %d", server->port);
		hbf_log_error("Port %d may already be in use. Try:", server->port);
		hbf_log_error("  - Kill existing process: pkill -f hbf");
		hbf_log_error("  - Use different port: --port <number>");
		hbf_log_error("  - Check what's using port: lsof -i :%d", server->port);
		return -1;
	}

	/* Register handlers */
	lh_set_request_handler(server->ctx, "/health", health_handler, server);
	lh_set_request_handler(server->ctx, "/static/**", static_handler, server);
	lh_set_request_handler(server->ctx, "**", hbf_qjs_request_handler, server);

	hbf_log_info("HTTP server listening at http://localhost:%d/", server->port);
	return 0;
}

void hbf_server_stop(hbf_server_t *server)
{
	if (server && server->ctx) {
		lh_stop(server->ctx);
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
