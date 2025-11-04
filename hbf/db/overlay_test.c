/* SPDX-License-Identifier: MIT */
/* Versioned file system integration tests */

#include "db.h"
#include "hbf/shell/log.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Test helper: check if table exists */
static int table_exists(sqlite3 *db, const char *table_name)
{
	sqlite3_stmt *stmt;
	const char *sql = "SELECT name FROM sqlite_master WHERE type='table' AND name=?";
	int rc;
	int exists;

	rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		return -1;
	}

	sqlite3_bind_text(stmt, 1, table_name, -1, SQLITE_STATIC);
	rc = sqlite3_step(stmt);
	exists = (rc == SQLITE_ROW) ? 1 : 0;
	sqlite3_finalize(stmt);

	return exists;
}

/* Test 1: Versioned file system schema tables created */
static void test_overlay_tables_exist(void)
{
	sqlite3 *db = NULL;
	int ret;

	printf("TEST: Versioned file system tables exist\n");

	ret = hbf_db_init(1, &db);
	assert(ret == 0);
	assert(db != NULL);

	/* Check file_ids table exists */
	assert(table_exists(db, "file_ids") == 1);
	printf("  ✓ file_ids table exists\n");

	/* Check file_versions table exists */
	assert(table_exists(db, "file_versions") == 1);
	printf("  ✓ file_versions table exists\n");

	hbf_db_close(db);
	printf("PASS: Versioned file system tables exist\n\n");
}

/* Test 2: SQLAR migrated to versioned filesystem */
static void test_sqlar_migrated(void)
{
	sqlite3 *db = NULL;
	int ret;

	printf("TEST: SQLAR migrated to versioned filesystem\n");

	ret = hbf_db_init(1, &db);
	assert(ret == 0);
	assert(db != NULL);

	/* Check sqlar table no longer exists after migration */
	assert(table_exists(db, "sqlar") == 0);
	printf("  ✓ sqlar table dropped after migration\n");

	/* Verify we can still read files through versioned filesystem */
	unsigned char *data = NULL;
	size_t size = 0;

	ret = hbf_db_read_file_from_main(db, "hbf/server.js", &data, &size);
	assert(ret == 0);
	assert(data != NULL);
	assert(size > 0);

	printf("  ✓ Read hbf/server.js from versioned filesystem (%zu bytes)\n", size);

	free(data);
	hbf_db_close(db);
	printf("PASS: SQLAR migration successful\n\n");
}

/* Test 3: latest_files view exists */
static void test_latest_files_view(void)
{
	sqlite3 *db = NULL;
	sqlite3_stmt *stmt;
	int ret;
	int rc;

	printf("TEST: latest_files view exists\n");

	ret = hbf_db_init(1, &db);
	assert(ret == 0);
	assert(db != NULL);

	/* Check if view exists */
	const char *check_sql =
		"SELECT name FROM sqlite_master WHERE type='view' AND name='latest_files'";

	rc = sqlite3_prepare_v2(db, check_sql, -1, &stmt, NULL);
	assert(rc == SQLITE_OK);

	rc = sqlite3_step(stmt);
	assert(rc == SQLITE_ROW);
	sqlite3_finalize(stmt);

	printf("  ✓ latest_files view exists\n");

	hbf_db_close(db);
	printf("PASS: latest_files view exists\n\n");
}

/* Test 4: Can insert and query file versions */
static void test_file_versions_basic(void)
{
	sqlite3 *db = NULL;
	sqlite3_stmt *stmt;
	int ret;
	int rc;
	const char *test_path = "test/version.txt";
	const char *test_content = "version 1 content";

	printf("TEST: File versions basic operations\n");

	ret = hbf_db_init(1, &db);
	assert(ret == 0);
	assert(db != NULL);

	/* Insert a file_id */
	const char *insert_id_sql = "INSERT INTO file_ids (path) VALUES (?)";

	rc = sqlite3_prepare_v2(db, insert_id_sql, -1, &stmt, NULL);
	assert(rc == SQLITE_OK);

	sqlite3_bind_text(stmt, 1, test_path, -1, SQLITE_STATIC);
	rc = sqlite3_step(stmt);
	assert(rc == SQLITE_DONE);
	sqlite3_finalize(stmt);

	printf("  ✓ Inserted file_id\n");

	/* Get the file_id */
	const char *get_id_sql = "SELECT file_id FROM file_ids WHERE path = ?";

	rc = sqlite3_prepare_v2(db, get_id_sql, -1, &stmt, NULL);
	assert(rc == SQLITE_OK);

	sqlite3_bind_text(stmt, 1, test_path, -1, SQLITE_STATIC);
	rc = sqlite3_step(stmt);
	assert(rc == SQLITE_ROW);

	int file_id = sqlite3_column_int(stmt, 0);

	sqlite3_finalize(stmt);

	printf("  ✓ Retrieved file_id: %d\n", file_id);

	/* Insert a version */
	const char *insert_version_sql =
		"INSERT INTO file_versions (file_id, path, version_number, mtime, size, data) "
		"VALUES (?, ?, 1, 1234567890, ?, ?)";

	rc = sqlite3_prepare_v2(db, insert_version_sql, -1, &stmt, NULL);
	assert(rc == SQLITE_OK);

	sqlite3_bind_int(stmt, 1, file_id);
	sqlite3_bind_text(stmt, 2, test_path, -1, SQLITE_STATIC);
	sqlite3_bind_int64(stmt, 3, (sqlite3_int64)strlen(test_content));
	sqlite3_bind_blob(stmt, 4, test_content, (int)strlen(test_content), SQLITE_STATIC);

	rc = sqlite3_step(stmt);
	assert(rc == SQLITE_DONE);
	sqlite3_finalize(stmt);

	printf("  ✓ Inserted version 1\n");

	/* Query from latest_files view */
	const char *query_sql = "SELECT path, data FROM latest_files WHERE path = ?";

	rc = sqlite3_prepare_v2(db, query_sql, -1, &stmt, NULL);
	assert(rc == SQLITE_OK);

	sqlite3_bind_text(stmt, 1, test_path, -1, SQLITE_STATIC);
	rc = sqlite3_step(stmt);
	assert(rc == SQLITE_ROW);

	const char *path = (const char *)sqlite3_column_text(stmt, 0);
	const void *data = sqlite3_column_blob(stmt, 1);
	int data_size = sqlite3_column_bytes(stmt, 1);

	assert(strcmp(path, test_path) == 0);
	assert(data_size == (int)strlen(test_content));
	assert(memcmp(data, test_content, (size_t)data_size) == 0);

	sqlite3_finalize(stmt);

	printf("  ✓ Query from latest_files returned correct data\n");

	hbf_db_close(db);
	printf("PASS: File versions basic operations\n\n");
}

int main(void)
{
	/* Initialize logging */
	hbf_log_init(HBF_LOG_INFO);

	printf("=== Versioned File System Integration Tests ===\n\n");

	test_overlay_tables_exist();
	test_sqlar_migrated();
	test_latest_files_view();
	test_file_versions_basic();

	printf("=== All tests passed! ===\n");
	return 0;
}
