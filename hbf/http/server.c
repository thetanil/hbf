/* SPDX-License-Identifier: MIT */
#include "server.h"
#include "hbf/http/handler.h"
#include "hbf/shell/log.h"
#include "hbf/db/overlay_fs.h"
#include "hbf/db/db.h"

#define EWS_HEADER_ONLY
#include "EmbeddableWebServer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/* Global server instance for EWS callback access */
static hbf_server_t *g_server_instance = NULL;
static pthread_mutex_t g_server_mutex = PTHREAD_MUTEX_INITIALIZER;

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

/* Handle static file requests */
static struct Response *handle_static_request(const struct Request *request)
{
	const char *uri = request->path;
	char path[512];
	unsigned char *data = NULL;
	size_t size;
	const char *mime_type;
	int ret;
	struct Response *response;

	/* Map URL to archive path */
	if (strcmp(uri, "/") == 0) {
		snprintf(path, sizeof(path), "static/index.html");
	} else if (strncmp(uri, "/static/", 8) == 0) {
		/* Remove leading slash for static paths */
		snprintf(path, sizeof(path), "%s", uri + 1);
	} else {
		/* Root-level static files */
		snprintf(path, sizeof(path), "static%s", uri);
	}

	hbf_log_debug("Static request: %s -> %s", uri, path);

	/* Read file from database */
	ret = overlay_fs_read_file(path, 1, &data, &size);
	if (ret != 0) {
		hbf_log_debug("File not found: %s", path);
		return responseAlloc404NotFoundHTML(request);
	}

	/* Determine MIME type */
	mime_type = get_mime_type(path);

	/* Create response */
	response = responseAlloc();
	if (!response) {
		free(data);
		return responseAlloc500InternalErrorHTML(request);
	}

	response->code = 200;
	heapStringSet(&response->mimeType, mime_type);
	heapStringSet(&response->body, (const char *)data);
	response->body.length = size;

	/* Add caching header */
	heapStringSet(&response->additionalHeaders, "Cache-Control: public, max-age=3600\r\n");

	free(data);

	hbf_log_debug("Served: %s (%zu bytes, %s)", path, size, mime_type);
	return response;
}

/* Handle health check requests */
static struct Response *handle_health_request(const struct Request *request)
{
	(void)request;
	return responseAllocJSON("{\"status\":\"ok\"}");
}

/* EWS callback - main request router */
struct Response *createResponseForRequest(const struct Request *request,
                                          struct Connection *connection)
{
	hbf_server_t *server;

	(void)connection;

	if (!request || !request->path) {
		return responseAlloc400BadRequestHTML(request);
	}

	hbf_log_debug("Request: %s %s", request->method ? request->method : "?",
	              request->path);

	/* Get server instance */
	pthread_mutex_lock(&g_server_mutex);
	server = g_server_instance;
	pthread_mutex_unlock(&g_server_mutex);

	if (!server) {
		hbf_log_error("No server instance available");
		return responseAlloc500InternalErrorHTML(request);
	}

	/* Route based on path */
	if (strcmp(request->path, "/health") == 0) {
		return handle_health_request(request);
	}

	/* Static file paths */
	if (strncmp(request->path, "/static/", 8) == 0 ||
	    strcmp(request->path, "/") == 0) {
		return handle_static_request(request);
	}

	/* All other routes go to QuickJS handler */
	return hbf_qjs_handle_ews_request(request, server);
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
	server->ews_server = NULL;

	return server;
}

int hbf_server_start(hbf_server_t *server)
{
	struct sockaddr_in addr;
	int result;

	if (!server) {
		return -1;
	}

	/* Set global server instance for EWS callbacks */
	pthread_mutex_lock(&g_server_mutex);
	g_server_instance = server;
	pthread_mutex_unlock(&g_server_mutex);

	/* Initialize EWS server structure */
	server->ews_server = calloc(1, sizeof(struct Server));
	if (!server->ews_server) {
		hbf_log_error("Failed to allocate EWS server");
		pthread_mutex_lock(&g_server_mutex);
		g_server_instance = NULL;
		pthread_mutex_unlock(&g_server_mutex);
		return -1;
	}

	serverInit(server->ews_server);

	/* Setup IPv4 address */
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons((uint16_t)server->port);

	hbf_log_info("Starting HTTP server on port %d...", server->port);

	/* Start server in background thread */
	/* Note: EWS's acceptConnectionsUntilStopped blocks, so we need to handle this carefully */
	/* For now, we'll use the simple blocking call - main.c will need to handle threading */
	result = acceptConnectionsUntilStopped(server->ews_server,
	                                       (const struct sockaddr *)&addr,
	                                       sizeof(addr));

	if (result != 0) {
		hbf_log_error("Failed to start HTTP server on port %d", server->port);
		hbf_log_error("Port %d may already be in use. Try:", server->port);
		hbf_log_error("  - Kill existing process: pkill -f hbf");
		hbf_log_error("  - Use different port: --port <number>");
		hbf_log_error("  - Check what's using port: lsof -i :%d", server->port);

		serverDeInit(server->ews_server);
		free(server->ews_server);
		server->ews_server = NULL;

		pthread_mutex_lock(&g_server_mutex);
		g_server_instance = NULL;
		pthread_mutex_unlock(&g_server_mutex);

		return -1;
	}

	hbf_log_info("HTTP server listening at http://localhost:%d/", server->port);
	return 0;
}

void hbf_server_stop(hbf_server_t *server)
{
	if (server && server->ews_server) {
		serverStop(server->ews_server);
		serverDeInit(server->ews_server);
		free(server->ews_server);
		server->ews_server = NULL;
		hbf_log_info("HTTP server stopped");

		pthread_mutex_lock(&g_server_mutex);
		g_server_instance = NULL;
		pthread_mutex_unlock(&g_server_mutex);
	}
}

void hbf_server_destroy(hbf_server_t *server)
{
	if (server) {
		if (server->ews_server) {
			hbf_server_stop(server);
		}
		free(server);
	}
}
