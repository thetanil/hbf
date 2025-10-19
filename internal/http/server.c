/* SPDX-License-Identifier: MIT */
#include "server.h"
#include "../core/log.h"
#include <civetweb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct hbf_server {
	struct mg_context *ctx;
	time_t start_time;
	int port;
};

/* Forward declarations */
static int health_handler(struct mg_connection *conn, void *cbdata);
static int notfound_handler(struct mg_connection *conn, void *cbdata);

hbf_server_t *hbf_server_start(const hbf_config_t *config)
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

	server = calloc(1, sizeof(hbf_server_t));
	if (!server) {
		hbf_log_error("Failed to allocate server structure");
		return NULL;
	}

	server->port = config->port;
	server->start_time = time(NULL);

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

	/* Register handlers */
	mg_set_request_handler(server->ctx, "/health", health_handler, server);
	mg_set_request_handler(server->ctx, "**", notfound_handler, server);

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

/* 404 handler for unmatched routes */
static int notfound_handler(struct mg_connection *conn, void *cbdata)
{
	const struct mg_request_info *ri;
	char response[256];

	(void)cbdata;

	ri = mg_get_request_info(conn);

	snprintf(response, sizeof(response),
		"{\"error\":\"Not Found\",\"path\":\"%s\"}",
		ri->local_uri);

	mg_printf(conn,
		"HTTP/1.1 404 Not Found\r\n"
		"Content-Type: application/json\r\n"
		"Content-Length: %d\r\n"
		"Connection: close\r\n"
		"\r\n"
		"%s",
		(int)strlen(response),
		response);

	return 404;
}
