/* SPDX-License-Identifier: MIT */
#include "schema.h"
#include "db.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define TEST_DB_PATH "/tmp/hbf_schema_test.db"

static void cleanup_test_db(void)
{
	unlink(TEST_DB_PATH);
	unlink("/tmp/hbf_schema_test.db-wal");
	unlink("/tmp/hbf_schema_test.db-shm");
}

static void test_schema_init(void)
{
	sqlite3 *db = NULL;
	int ret;

	cleanup_test_db();

	ret = hbf_db_open(TEST_DB_PATH, &db);
	assert(ret == 0);

	ret = hbf_db_init_schema(db);
	assert(ret == 0);

	hbf_db_close(db);
	cleanup_test_db();

	printf("  ✓ Initialize schema\n");
}

static void test_schema_tables_created(void)
{
	sqlite3 *db = NULL;
	sqlite3_stmt *stmt;
	int ret;
	int count;

	cleanup_test_db();

	ret = hbf_db_open(TEST_DB_PATH, &db);
	assert(ret == 0);

	ret = hbf_db_init_schema(db);
	assert(ret == 0);

	/* Check that core tables exist */
	const char *expected_tables[] = {
		"nodes",
		"edges",
		"tags",
		"_hbf_users",
		"_hbf_sessions",
		"_hbf_table_permissions",
		"_hbf_row_policies",
		"_hbf_config",
		"_hbf_audit_log",
		"_hbf_schema_version",
		"nodes_fts"
	};
	int num_tables = sizeof(expected_tables) / sizeof(expected_tables[0]);

	for (int i = 0; i < num_tables; i++) {
		char query[256];
		snprintf(query, sizeof(query),
			"SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='%s'",
			expected_tables[i]);

		ret = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
		assert(ret == SQLITE_OK);

		ret = sqlite3_step(stmt);
		assert(ret == SQLITE_ROW);

		count = sqlite3_column_int(stmt, 0);
		sqlite3_finalize(stmt);

		assert(count == 1);
	}

	hbf_db_close(db);
	cleanup_test_db();

	printf("  ✓ All tables created\n");
}

static void test_schema_foreign_keys_work(void)
{
	sqlite3 *db = NULL;
	int ret;

	cleanup_test_db();

	ret = hbf_db_open(TEST_DB_PATH, &db);
	assert(ret == 0);

	ret = hbf_db_init_schema(db);
	assert(ret == 0);

	/* Insert a node */
	ret = hbf_db_exec(db,
		"INSERT INTO nodes (type, body) VALUES ('test', '{\"name\": \"test\"}')");
	assert(ret == 0);

	int64_t node_id = hbf_db_last_insert_id(db);

	/* Insert a tag referencing the node */
	char sql[256];
	snprintf(sql, sizeof(sql),
		"INSERT INTO tags (tag, node_id) VALUES ('test-tag', %lld)",
		(long long)node_id);
	ret = hbf_db_exec(db, sql);
	assert(ret == 0);

	/* Try to delete the node - should fail due to foreign key constraint */
	snprintf(sql, sizeof(sql), "DELETE FROM nodes WHERE id = %lld", (long long)node_id);
	ret = hbf_db_exec(db, sql);
	/* Should fail because ON DELETE CASCADE will attempt to cascade, but this tests FK is enforced */
	/* Actually, with CASCADE it will succeed, so let's test without cascade */

	/* Better test: try to insert a tag with non-existent node_id */
	ret = hbf_db_exec(db, "INSERT INTO tags (tag, node_id) VALUES ('bad-tag', 99999)");
	assert(ret == -1); /* Should fail */

	hbf_db_close(db);
	cleanup_test_db();

	printf("  ✓ Foreign key constraints enforced\n");
}

static void test_schema_config_defaults(void)
{
	sqlite3 *db = NULL;
	sqlite3_stmt *stmt;
	int ret;
	int count;

	cleanup_test_db();

	ret = hbf_db_open(TEST_DB_PATH, &db);
	assert(ret == 0);

	ret = hbf_db_init_schema(db);
	assert(ret == 0);

	/* Check that default config values were inserted */
	ret = sqlite3_prepare_v2(db,
		"SELECT COUNT(*) FROM _hbf_config",
		-1, &stmt, NULL);
	assert(ret == SQLITE_OK);

	ret = sqlite3_step(stmt);
	assert(ret == SQLITE_ROW);

	count = sqlite3_column_int(stmt, 0);
	sqlite3_finalize(stmt);

	assert(count >= 5); /* At least 5 default config values */

	/* Check specific config value */
	ret = sqlite3_prepare_v2(db,
		"SELECT value FROM _hbf_config WHERE key = 'qjs_mem_mb'",
		-1, &stmt, NULL);
	assert(ret == SQLITE_OK);

	ret = sqlite3_step(stmt);
	assert(ret == SQLITE_ROW);

	const char *value = (const char *)sqlite3_column_text(stmt, 0);
	assert(strcmp(value, "64") == 0);
	sqlite3_finalize(stmt);

	hbf_db_close(db);
	cleanup_test_db();

	printf("  ✓ Default configuration values inserted\n");
}

static void test_schema_version(void)
{
	sqlite3 *db = NULL;
	int version;
	int is_init;
	int ret;

	cleanup_test_db();

	ret = hbf_db_open(TEST_DB_PATH, &db);
	assert(ret == 0);

	/* Check not initialized */
	is_init = hbf_db_is_initialized(db);
	assert(is_init == 0);

	/* Initialize schema */
	ret = hbf_db_init_schema(db);
	assert(ret == 0);

	/* Check initialized */
	is_init = hbf_db_is_initialized(db);
	assert(is_init == 1);

	/* Get schema version */
	version = hbf_db_get_schema_version(db);
	assert(version == 1);

	hbf_db_close(db);
	cleanup_test_db();

	printf("  ✓ Schema version tracking\n");
}

static void test_schema_fts5(void)
{
	sqlite3 *db = NULL;
	sqlite3_stmt *stmt;
	int ret;

	cleanup_test_db();

	ret = hbf_db_open(TEST_DB_PATH, &db);
	assert(ret == 0);

	ret = hbf_db_init_schema(db);
	assert(ret == 0);

	/* Insert a node with content */
	ret = hbf_db_exec(db,
		"INSERT INTO nodes (type, body) VALUES ('document', "
		"'{\"name\": \"test\", \"content\": \"The quick brown fox jumps over the lazy dog\"}')");
	assert(ret == 0);

	/* Search using FTS5 */
	ret = sqlite3_prepare_v2(db,
		"SELECT rowid FROM nodes_fts WHERE content MATCH 'quick'",
		-1, &stmt, NULL);
	assert(ret == SQLITE_OK);

	ret = sqlite3_step(stmt);
	assert(ret == SQLITE_ROW);

	int64_t node_id = sqlite3_column_int64(stmt, 0);
	assert(node_id > 0); /* Just verify we got a valid node ID */
	sqlite3_finalize(stmt);

	/* Search for non-existent term */
	ret = sqlite3_prepare_v2(db,
		"SELECT rowid FROM nodes_fts WHERE content MATCH 'nonexistent'",
		-1, &stmt, NULL);
	assert(ret == SQLITE_OK);

	ret = sqlite3_step(stmt);
	assert(ret == SQLITE_DONE); /* No results */
	sqlite3_finalize(stmt);

	hbf_db_close(db);
	cleanup_test_db();

	printf("  ✓ FTS5 full-text search\n");
}

static void test_schema_fts5_triggers(void)
{
	sqlite3 *db = NULL;
	sqlite3_stmt *stmt;
	int ret;
	int count;

	cleanup_test_db();

	ret = hbf_db_open(TEST_DB_PATH, &db);
	assert(ret == 0);

	ret = hbf_db_init_schema(db);
	assert(ret == 0);

	/* Insert a node */
	ret = hbf_db_exec(db,
		"INSERT INTO nodes (type, body) VALUES ('document', "
		"'{\"name\": \"test\", \"content\": \"original content\"}')");
	assert(ret == 0);

	int64_t node_id = hbf_db_last_insert_id(db);

	/* Check FTS index has the content */
	ret = sqlite3_prepare_v2(db,
		"SELECT COUNT(*) FROM nodes_fts WHERE content MATCH 'original'",
		-1, &stmt, NULL);
	assert(ret == SQLITE_OK);
	ret = sqlite3_step(stmt);
	assert(ret == SQLITE_ROW);
	count = sqlite3_column_int(stmt, 0);
	assert(count == 1);
	sqlite3_finalize(stmt);

	/* Update the node content */
	char sql[256];
	snprintf(sql, sizeof(sql),
		"UPDATE nodes SET body = '{\"name\": \"test\", \"content\": \"updated content\"}' "
		"WHERE id = %lld", (long long)node_id);
	ret = hbf_db_exec(db, sql);
	assert(ret == 0);

	/* Check FTS index updated */
	ret = sqlite3_prepare_v2(db,
		"SELECT COUNT(*) FROM nodes_fts WHERE content MATCH 'updated'",
		-1, &stmt, NULL);
	assert(ret == SQLITE_OK);
	ret = sqlite3_step(stmt);
	assert(ret == SQLITE_ROW);
	count = sqlite3_column_int(stmt, 0);
	assert(count == 1);
	sqlite3_finalize(stmt);

	/* Old content should not be in index */
	ret = sqlite3_prepare_v2(db,
		"SELECT COUNT(*) FROM nodes_fts WHERE content MATCH 'original'",
		-1, &stmt, NULL);
	assert(ret == SQLITE_OK);
	ret = sqlite3_step(stmt);
	assert(ret == SQLITE_ROW);
	count = sqlite3_column_int(stmt, 0);
	assert(count == 0);
	sqlite3_finalize(stmt);

	/* Delete the node */
	snprintf(sql, sizeof(sql), "DELETE FROM nodes WHERE id = %lld", (long long)node_id);
	ret = hbf_db_exec(db, sql);
	assert(ret == 0);

	/* Check FTS index deleted */
	ret = sqlite3_prepare_v2(db,
		"SELECT COUNT(*) FROM nodes_fts WHERE content MATCH 'updated'",
		-1, &stmt, NULL);
	assert(ret == SQLITE_OK);
	ret = sqlite3_step(stmt);
	assert(ret == SQLITE_ROW);
	count = sqlite3_column_int(stmt, 0);
	assert(count == 0);
	sqlite3_finalize(stmt);

	hbf_db_close(db);
	cleanup_test_db();

	printf("  ✓ FTS5 triggers (insert, update, delete)\n");
}

static void test_schema_document_graph_operations(void)
{
	sqlite3 *db = NULL;
	sqlite3_stmt *stmt;
	int ret;

	cleanup_test_db();

	ret = hbf_db_open(TEST_DB_PATH, &db);
	assert(ret == 0);

	ret = hbf_db_init_schema(db);
	assert(ret == 0);

	/* Insert nodes */
	ret = hbf_db_exec(db,
		"INSERT INTO nodes (type, body) VALUES "
		"('function', '{\"name\": \"main\", \"content\": \"function main() {}\"}'), "
		"('function', '{\"name\": \"helper\", \"content\": \"function helper() {}\"}')");
	assert(ret == 0);

	/* Create edge (main calls helper) */
	ret = hbf_db_exec(db,
		"INSERT INTO edges (src, dst, rel) VALUES (1, 2, 'calls')");
	assert(ret == 0);

	/* Add tags */
	ret = hbf_db_exec(db,
		"INSERT INTO tags (tag, node_id, order_num) VALUES ('module:core', 1, 0)");
	assert(ret == 0);

	/* Query: Find all nodes called by node 1 */
	ret = sqlite3_prepare_v2(db,
		"SELECT dst FROM edges WHERE src = 1 AND rel = 'calls'",
		-1, &stmt, NULL);
	assert(ret == SQLITE_OK);

	ret = sqlite3_step(stmt);
	assert(ret == SQLITE_ROW);
	assert(sqlite3_column_int(stmt, 0) == 2);
	sqlite3_finalize(stmt);

	/* Query: Find all nodes with tag 'module:core' */
	ret = sqlite3_prepare_v2(db,
		"SELECT node_id FROM tags WHERE tag = 'module:core'",
		-1, &stmt, NULL);
	assert(ret == SQLITE_OK);

	ret = sqlite3_step(stmt);
	assert(ret == SQLITE_ROW);
	assert(sqlite3_column_int(stmt, 0) == 1);
	sqlite3_finalize(stmt);

	hbf_db_close(db);
	cleanup_test_db();

	printf("  ✓ Document-graph operations (nodes, edges, tags)\n");
}

static void test_schema_idempotent(void)
{
	sqlite3 *db = NULL;
	int ret;

	cleanup_test_db();

	ret = hbf_db_open(TEST_DB_PATH, &db);
	assert(ret == 0);

	/* Initialize schema twice - should not fail */
	ret = hbf_db_init_schema(db);
	assert(ret == 0);

	ret = hbf_db_init_schema(db);
	assert(ret == 0);

	hbf_db_close(db);
	cleanup_test_db();

	printf("  ✓ Schema initialization is idempotent\n");
}

int main(void)
{
	printf("Running schema tests:\n");

	test_schema_init();
	test_schema_tables_created();
	test_schema_foreign_keys_work();
	test_schema_config_defaults();
	test_schema_version();
	test_schema_fts5();
	test_schema_fts5_triggers();
	test_schema_document_graph_operations();
	test_schema_idempotent();

	printf("\nAll schema tests passed!\n");
	return 0;
}
