/* SPDX-License-Identifier: MIT */
#ifndef HBF_HTTP_SERVER_H
#define HBF_HTTP_SERVER_H

#include "../core/config.h"
#include <stddef.h>

/*
 * HTTP server context.
 * Opaque structure - implementation details hidden.
 */
typedef struct hbf_server hbf_server_t;

/*
 * Create and start HTTP server.
 *
 * @param config: Configuration including port and log level
 * @return Server instance on success, NULL on error
 */
hbf_server_t *hbf_server_start(const hbf_config_t *config);

/*
 * Stop and destroy HTTP server.
 *
 * @param server: Server instance to stop
 */
void hbf_server_stop(hbf_server_t *server);

/*
 * Get server uptime in seconds.
 *
 * @param server: Server instance
 * @return Uptime in seconds, or 0 if server is NULL
 */
unsigned long hbf_server_uptime(const hbf_server_t *server);

#endif /* HBF_HTTP_SERVER_H */
