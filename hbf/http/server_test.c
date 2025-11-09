/* SPDX-License-Identifier: MIT */
#include "server.h"
#include "hbf/shell/log.h"
#include "hbf/db/db.h"
#include "hbf/qjs/engine.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void test_server_startup(void)
{
	sqlite3 *db = NULL;
	hbf_server_t *server = NULL;
	int ret;

	/* Initialize database */
	ret = hbf_db_init(1, &db);
	assert(ret == 0);
	assert(db != NULL);

	/* Initialize QuickJS engine */
	ret = hbf_qjs_init(64 * 1024 * 1024, 5000);
	assert(ret == 0);

	/* Create server instance */
	server = hbf_server_create(15309, db);
	assert(server != NULL);

	/* Start server */
	ret = hbf_server_start(server);
	assert(ret == 0);

	printf("  ✓ Server started successfully\n");

	/* Stop server */
	hbf_server_stop(server);
	printf("  ✓ Server stopped successfully\n");

	/* Cleanup */
	hbf_server_destroy(server);
	hbf_qjs_shutdown();
	hbf_db_close(db);

	printf("  ✓ Server startup and shutdown\n");
}

int main(void)
{
	hbf_log_init(hbf_log_parse_level("ERROR"));

	printf("HTTP server tests:\n");

	test_server_startup();

	printf("\nAll HTTP server tests passed!\n");
	return 0;
}
