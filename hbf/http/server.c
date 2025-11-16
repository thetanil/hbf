/* SPDX-License-Identifier: MIT */
#include "server.h"
#include "hbf/shell/log.h"
#include "hbf/db/db.h"
#include "civetweb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>



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

/* Stats handler - returns server statistics */
static int stats_handler(struct mg_connection *conn, void *cbdata)
{
	hbf_server_t *server = (hbf_server_t *)cbdata;
	char buffer[1024];
	const struct mg_request_info *ri = mg_get_request_info(conn);
	struct timeval start_time, end_time;
	double avg_time_ms;

	if (!server || !server->ctx || !ri) {
		mg_send_http_error(conn, 500, "Internal error");
		return 500;
	}

	/* Track request start time */
	gettimeofday(&start_time, NULL);

	/* Increment request counter */
	server->request_count++;

	/* Calculate average request time */
	avg_time_ms = server->request_count > 0
		? (server->total_request_time * 1000.0) / (double)server->request_count
		: 0.0;

	/* Build stats HTML */
	snprintf(buffer, sizeof(buffer),
	         "<div>\n"
	         "  <p><strong>Total Requests:</strong> %lu</p>\n"
	         "  <p><strong>Avg Response Time:</strong> %.2f ms</p>\n"
	         "  <p><strong>Thread Pool Size:</strong> 4</p>\n"
	         "  <p><strong>Server:</strong> CivetWeb</p>\n"
	         "  <p><strong>Mode:</strong> Phase 0 (Hypermedia)</p>\n"
	         "</div>\n",
	         server->request_count, avg_time_ms);

	mg_printf(conn,
	          "HTTP/1.1 200 OK\r\n"
	          "Content-Type: text/html\r\n"
	          "Content-Length: %zu\r\n"
	          "Cache-Control: no-cache\r\n"
	          "Connection: close\r\n"
	          "\r\n%s",
	          strlen(buffer), buffer);

	/* Track request end time and update total */
	gettimeofday(&end_time, NULL);
	double elapsed = (double)(end_time.tv_sec - start_time.tv_sec) +
	                 (double)(end_time.tv_usec - start_time.tv_usec) / 1000000.0;
	server->total_request_time += elapsed;

	return 200;
}

/* Homepage handler - hypermedia Phase 0 */
static int homepage_handler(struct mg_connection *conn, void *cbdata)
{
	const char *html =
		"<!DOCTYPE html>\n"
		"<html>\n"
		"<head>\n"
		"  <meta charset=\"utf-8\">\n"
		"  <title>HBF Hypermedia</title>\n"
		"  <script src=\"https://unpkg.com/htmx.org@1.9.10\"></script>\n"
		"  <style>\n"
		"    body { font-family: sans-serif; max-width: 800px; margin: 2em auto; padding: 0 1em; }\n"
		"    .stats { background: #f5f5f5; padding: 1em; border-radius: 4px; margin: 1em 0; }\n"
		"    .stats h2 { margin-top: 0; }\n"
		"    .stats p { margin: 0.5em 0; }\n"
		"  </style>\n"
		"</head>\n"
		"<body>\n"
		"  <h1>HBF Hypermedia System</h1>\n"
		"  <div class=\"stats\">\n"
		"    <h2>Server Stats</h2>\n"
		"    <div id=\"stats\" hx-get=\"/stats\" hx-trigger=\"load, every 2s\"></div>\n"
		"  </div>\n"
		"  <div id=\"link-list\" hx-get=\"/links\" hx-trigger=\"load\"></div>\n"
		"</body>\n"
		"</html>\n";

	(void)cbdata;

	mg_printf(conn,
	          "HTTP/1.1 200 OK\r\n"
	          "Content-Type: text/html\r\n"
	          "Content-Length: %zu\r\n"
	          "Connection: close\r\n"
	          "\r\n%s",
	          strlen(html), html);

	hbf_log_debug("Served hypermedia homepage");
	return 200;
}

/* Links handler - stub returning simple HTML list */
static int links_handler(struct mg_connection *conn, void *cbdata)
{
	const char *html =
		"<ul>\n"
		"  <li>Phase 0: Minimal hypermedia core</li>\n"
		"  <li>Database: WAL mode enabled</li>\n"
		"  <li>Server: CivetWeb running</li>\n"
		"</ul>\n";

	(void)cbdata;

	mg_printf(conn,
	          "HTTP/1.1 200 OK\r\n"
	          "Content-Type: text/html\r\n"
	          "Content-Length: %zu\r\n"
	          "Connection: close\r\n"
	          "\r\n%s",
	          strlen(html), html);

	hbf_log_debug("Served links list");
	return 200;
}

/* Fragment GET handler - stub for Phase 0 */
static int fragment_get_handler(struct mg_connection *conn, void *cbdata)
{
	const struct mg_request_info *ri = mg_get_request_info(conn);
	const char *uri;

	(void)cbdata;

	if (!ri) {
		mg_send_http_error(conn, 500, "Internal error");
		return 500;
	}

	uri = ri->local_uri;

	mg_printf(conn,
	          "HTTP/1.1 200 OK\r\n"
	          "Content-Type: text/html\r\n"
	          "Connection: close\r\n"
	          "\r\n");

	mg_printf(conn,
	          "<div>\n"
	          "  <p>Fragment GET handler stub (Phase 0)</p>\n"
	          "  <p>Requested URI: %s</p>\n"
	          "</div>\n",
	          uri);

	hbf_log_debug("Fragment GET stub: %s", uri);
	return 200;
}

/* Fragment POST handler - stub for Phase 0 */
static int fragment_post_handler(struct mg_connection *conn, void *cbdata)
{
	const char *response = "{\"status\":\"stub\",\"message\":\"Fragment POST handler (Phase 0)\"}";

	(void)cbdata;

	mg_printf(conn,
	          "HTTP/1.1 200 OK\r\n"
	          "Content-Type: application/json\r\n"
	          "Content-Length: %zu\r\n"
	          "Connection: close\r\n"
	          "\r\n%s",
	          strlen(response), response);

	hbf_log_debug("Fragment POST stub");
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
	server->request_count = 0;
	server->total_request_time = 0.0;

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
		hbf_log_error("Failed to start HTTP server on port %d", server->port);
		hbf_log_error("Port %d may already be in use. Try:", server->port);
		hbf_log_error("  - Kill existing process: pkill -f hbf");
		hbf_log_error("  - Use different port: --port <number>");
		hbf_log_error("  - Check what's using port: lsof -i :%d", server->port);
		return -1;
	}

	/* Register handlers - order matters! */
	mg_set_request_handler(server->ctx, "/health", health_handler, server);
	mg_set_request_handler(server->ctx, "/stats", stats_handler, server);
	mg_set_request_handler(server->ctx, "/", homepage_handler, server);
	mg_set_request_handler(server->ctx, "/links", links_handler, server);
	mg_set_request_handler(server->ctx, "/fragment/*", fragment_get_handler, server);
	mg_set_request_handler(server->ctx, "/fragment", fragment_post_handler, server);

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
