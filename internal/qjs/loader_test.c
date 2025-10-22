/* QuickJS loader tests */
#include "internal/qjs/loader.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "internal/core/log.h"
#include "internal/db/db.h"
#include "internal/db/schema.h"
#include "internal/qjs/engine.h"

#define TEST_DB ":memory:"

static sqlite3 *setup_test_db(void)
{
	sqlite3 *db;
	int ret;

	ret = hbf_db_open(TEST_DB, &db);
	assert(ret == 0);

	ret = hbf_db_init_schema(db);
	assert(ret == 0);

	return db;
}

static void teardown_test_db(sqlite3 *db)
{
	hbf_db_close(db);
}

static void insert_test_script(sqlite3 *db, const char *name,
				const char *content)
{
	const char *sql =
		"INSERT INTO nodes (type, body) VALUES ('script', json_object('name', ?, 'content', ?))";
	sqlite3_stmt *stmt;
	int ret;

	ret = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	assert(ret == SQLITE_OK);

	ret = sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
	assert(ret == SQLITE_OK);

	ret = sqlite3_bind_text(stmt, 2, content, -1, SQLITE_STATIC);
	assert(ret == SQLITE_OK);

	ret = sqlite3_step(stmt);
	assert(ret == SQLITE_DONE);

	sqlite3_finalize(stmt);
}

static void test_load_server_js(void)
{
	sqlite3 *db;
	hbf_qjs_ctx_t *ctx;
	int ret;
	const char *script_content = "var x = 42; // server.js loaded";

	/* Setup */
	db = setup_test_db();
	insert_test_script(db, "server.js", script_content);

	ret = hbf_qjs_init(64, 5000);
	assert(ret == 0);

	ctx = hbf_qjs_ctx_create();
	assert(ctx != NULL);

	/* Test loading server.js */
	ret = hbf_qjs_load_server_js(ctx, db);
	assert(ret == 0);

	/* Cleanup */
	hbf_qjs_ctx_destroy(ctx);
	hbf_qjs_shutdown();
	teardown_test_db(db);

	printf("  ✓ Load server.js from database\n");
}

static void test_load_missing_script(void)
{
	sqlite3 *db;
	hbf_qjs_ctx_t *ctx;
	int ret;

	/* Setup without inserting script */
	db = setup_test_db();

	ret = hbf_qjs_init(64, 5000);
	assert(ret == 0);

	ctx = hbf_qjs_ctx_create();
	assert(ctx != NULL);

	/* Should fail - script not found */
	ret = hbf_qjs_load_server_js(ctx, db);
	assert(ret != 0);

	/* Cleanup */
	hbf_qjs_ctx_destroy(ctx);
	hbf_qjs_shutdown();
	teardown_test_db(db);

	printf("  ✓ Handle missing script\n");
}

static void test_load_script_with_error(void)
{
	sqlite3 *db;
	hbf_qjs_ctx_t *ctx;
	int ret;
	const char *bad_script = "syntax error!";

	/* Setup */
	db = setup_test_db();
	insert_test_script(db, "bad.js", bad_script);

	ret = hbf_qjs_init(64, 5000);
	assert(ret == 0);

	ctx = hbf_qjs_ctx_create();
	assert(ctx != NULL);

	/* Should fail - syntax error */
	ret = hbf_qjs_load_script(ctx, db, "bad.js");
	assert(ret != 0);

	/* Cleanup */
	hbf_qjs_ctx_destroy(ctx);
	hbf_qjs_shutdown();
	teardown_test_db(db);

	printf("  ✓ Handle script with syntax error\n");
}

static void test_load_script_by_name(void)
{
	sqlite3 *db;
	hbf_qjs_ctx_t *ctx;
	int ret;
	const char *script_content = "var x = 42;";

	/* Setup */
	db = setup_test_db();
	insert_test_script(db, "test.js", script_content);

	ret = hbf_qjs_init(64, 5000);
	assert(ret == 0);

	ctx = hbf_qjs_ctx_create();
	assert(ctx != NULL);

	/* Load specific script */
	ret = hbf_qjs_load_script(ctx, db, "test.js");
	assert(ret == 0);

	/* Cleanup */
	hbf_qjs_ctx_destroy(ctx);
	hbf_qjs_shutdown();
	teardown_test_db(db);

	printf("  ✓ Load script by name\n");
}

int main(void)
{
	/* Initialize logging */
	hbf_log_set_level(HBF_LOG_WARN);

	printf("QuickJS Loader Tests:\n");

	test_load_server_js();
	test_load_missing_script();
	test_load_script_with_error();
	test_load_script_by_name();

	printf("\nAll loader tests passed!\n");
	return 0;
}
