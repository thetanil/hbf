/* SPDX-License-Identifier: MIT */
#ifndef HBF_HTTP_SERVER_H
#define HBF_HTTP_SERVER_H

#include <sqlite3.h>

/* Forward declaration for EWS server */
struct Server;

/* HTTP server structure */
typedef struct hbf_server {
	int port;
	sqlite3 *db;       /* Main database (contains SQLAR) */
	struct Server *ews_server;
} hbf_server_t;

/*
 * Create HTTP server
 *
 * @param port: HTTP server port
 * @param db: Main database handle (contains SQLAR)
 * @return Server instance or NULL on error
 */
hbf_server_t *hbf_server_create(int port, sqlite3 *db);

/*
 * Start HTTP server
 *
 * @param server: Server instance
 * @return 0 on success, -1 on error
 */
int hbf_server_start(hbf_server_t *server);

/*
 * Stop HTTP server
 *
 * @param server: Server instance
 */
void hbf_server_stop(hbf_server_t *server);

/*
 * Destroy HTTP server
 *
 * @param server: Server instance
 */
void hbf_server_destroy(hbf_server_t *server);

#endif /* HBF_HTTP_SERVER_H */
