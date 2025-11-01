/* SPDX-License-Identifier: MIT */
/* Overlay filesystem tests */

#include "db.h"
#include "../core/log.h"
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

/* Test 1: Overlay schema tables created */
static void test_overlay_tables_exist(void)
{
	sqlite3 *db = NULL;
	int ret;

	printf("TEST: Overlay tables exist\n");

	ret = hbf_db_init(1, &db);
	assert(ret == 0);
	assert(db != NULL);

	/* Check fs_layer table exists */
	assert(table_exists(db, "fs_layer") == 1);
	printf("  ✓ fs_layer table exists\n");

	/* Check overlay_sqlar table exists */
	assert(table_exists(db, "overlay_sqlar") == 1);
	printf("  ✓ overlay_sqlar table exists\n");

	hbf_db_close(db);
	printf("PASS: Overlay tables exist\n\n");
}

/* Test 2: Trigger updates overlay_sqlar on insert */
static void test_overlay_trigger(void)
{
	sqlite3 *db = NULL;
	sqlite3_stmt *stmt;
	const char *test_content = "modified content";
	const char *test_name = "static/test.html";
	int ret;
	int rc;
	int count;

	printf("TEST: Overlay trigger updates overlay_sqlar\n");

	ret = hbf_db_init(1, &db);
	assert(ret == 0);
	assert(db != NULL);

	/* Insert a modify operation into fs_layer */
	const char *insert_sql = "INSERT INTO fs_layer (layer_id, name, op, mtime, sz, data) "
	                         "VALUES (1, ?, 'modify', 1234567890, ?, ?)";

	rc = sqlite3_prepare_v2(db, insert_sql, -1, &stmt, NULL);
	assert(rc == SQLITE_OK);

	sqlite3_bind_text(stmt, 1, test_name, -1, SQLITE_STATIC);
	sqlite3_bind_int(stmt, 2, (int)strlen(test_content));
	sqlite3_bind_blob(stmt, 3, test_content, (int)strlen(test_content), SQLITE_STATIC);

	rc = sqlite3_step(stmt);
	assert(rc == SQLITE_DONE);
	sqlite3_finalize(stmt);

	printf("  ✓ Inserted modify operation into fs_layer\n");

	/* Check overlay_sqlar was updated by trigger */
	const char *check_sql = "SELECT COUNT(*) FROM overlay_sqlar WHERE name = ?";
	rc = sqlite3_prepare_v2(db, check_sql, -1, &stmt, NULL);
	assert(rc == SQLITE_OK);

	sqlite3_bind_text(stmt, 1, test_name, -1, SQLITE_STATIC);
	rc = sqlite3_step(stmt);
	assert(rc == SQLITE_ROW);

	count = sqlite3_column_int(stmt, 0);
	assert(count == 1);
	sqlite3_finalize(stmt);

	printf("  ✓ Trigger updated overlay_sqlar\n");

	hbf_db_close(db);
	printf("PASS: Overlay trigger works\n\n");
}

/* Test 3: Read with overlay returns modified content */
static void test_read_with_overlay(void)
{
	sqlite3 *db = NULL;
	sqlite3_stmt *stmt;
	const char *test_content = "overlay content";
	const char *test_name = "hbf/test.js";
	unsigned char *data = NULL;
	size_t size;
	int ret;
	int rc;

	printf("TEST: Read with overlay returns modified content\n");

	ret = hbf_db_init(1, &db);
	assert(ret == 0);
	assert(db != NULL);

	/* First insert base content into sqlar */
	const char *base_content = "base content";
	const char *insert_base = "INSERT INTO sqlar (name, mode, mtime, sz, data) "
	                          "VALUES (?, 33188, 1234567890, ?, ?)";

	rc = sqlite3_prepare_v2(db, insert_base, -1, &stmt, NULL);
	assert(rc == SQLITE_OK);

	sqlite3_bind_text(stmt, 1, test_name, -1, SQLITE_STATIC);
	sqlite3_bind_int(stmt, 2, (int)strlen(base_content));
	sqlite3_bind_blob(stmt, 3, base_content, (int)strlen(base_content), SQLITE_STATIC);

	rc = sqlite3_step(stmt);
	assert(rc == SQLITE_DONE);
	sqlite3_finalize(stmt);

	printf("  ✓ Inserted base content\n");

	/* Now add overlay content */
	const char *insert_overlay = "INSERT INTO fs_layer (layer_id, name, op, mtime, sz, data) "
	                             "VALUES (1, ?, 'modify', 1234567890, ?, ?)";

	rc = sqlite3_prepare_v2(db, insert_overlay, -1, &stmt, NULL);
	assert(rc == SQLITE_OK);

	sqlite3_bind_text(stmt, 1, test_name, -1, SQLITE_STATIC);
	sqlite3_bind_int(stmt, 2, (int)strlen(test_content));
	sqlite3_bind_blob(stmt, 3, test_content, (int)strlen(test_content), SQLITE_STATIC);

	rc = sqlite3_step(stmt);
	assert(rc == SQLITE_DONE);
	sqlite3_finalize(stmt);

	printf("  ✓ Inserted overlay content\n");

	/* Read with overlay enabled */
	ret = hbf_db_read_file(db, test_name, 1, &data, &size);
	assert(ret == 0);
	assert(data != NULL);
	assert(size == strlen(test_content));
	assert(memcmp(data, test_content, size) == 0);

	printf("  ✓ Read returned overlay content (use_overlay=1)\n");

	free(data);
	hbf_db_close(db);
	printf("PASS: Read with overlay works\n\n");
}

/* Test 4: Read without overlay returns base content only */
static void test_read_without_overlay(void)
{
	sqlite3 *db = NULL;
	sqlite3_stmt *stmt;
	const char *base_content = "base content";
	const char *overlay_content = "overlay content";
	const char *test_name = "hbf/test2.js";
	unsigned char *data = NULL;
	size_t size;
	int ret;
	int rc;

	printf("TEST: Read without overlay returns base content\n");

	ret = hbf_db_init(1, &db);
	assert(ret == 0);
	assert(db != NULL);

	/* Insert base content */
	const char *insert_base = "INSERT INTO sqlar (name, mode, mtime, sz, data) "
	                          "VALUES (?, 33188, 1234567890, ?, ?)";

	rc = sqlite3_prepare_v2(db, insert_base, -1, &stmt, NULL);
	assert(rc == SQLITE_OK);

	sqlite3_bind_text(stmt, 1, test_name, -1, SQLITE_STATIC);
	sqlite3_bind_int(stmt, 2, (int)strlen(base_content));
	sqlite3_bind_blob(stmt, 3, base_content, (int)strlen(base_content), SQLITE_STATIC);

	rc = sqlite3_step(stmt);
	assert(rc == SQLITE_DONE);
	sqlite3_finalize(stmt);

	printf("  ✓ Inserted base content\n");

	/* Add overlay content */
	const char *insert_overlay = "INSERT INTO fs_layer (layer_id, name, op, mtime, sz, data) "
	                             "VALUES (1, ?, 'modify', 1234567890, ?, ?)";

	rc = sqlite3_prepare_v2(db, insert_overlay, -1, &stmt, NULL);
	assert(rc == SQLITE_OK);

	sqlite3_bind_text(stmt, 1, test_name, -1, SQLITE_STATIC);
	sqlite3_bind_int(stmt, 2, (int)strlen(overlay_content));
	sqlite3_bind_blob(stmt, 3, overlay_content, (int)strlen(overlay_content), SQLITE_STATIC);

	rc = sqlite3_step(stmt);
	assert(rc == SQLITE_DONE);
	sqlite3_finalize(stmt);

	printf("  ✓ Inserted overlay content\n");

	/* Read with overlay DISABLED */
	ret = hbf_db_read_file(db, test_name, 0, &data, &size);
	assert(ret == 0);
	assert(data != NULL);
	assert(size == strlen(base_content));
	assert(memcmp(data, base_content, size) == 0);

	printf("  ✓ Read returned base content (use_overlay=0)\n");

	free(data);
	hbf_db_close(db);
	printf("PASS: Read without overlay works\n\n");
}

/* Test 5: Delete operation creates tombstone */
static void test_overlay_delete(void)
{
	sqlite3 *db = NULL;
	sqlite3_stmt *stmt;
	const char *base_content = "base content";
	const char *test_name = "static/deleted.html";
	unsigned char *data = NULL;
	size_t size;
	int ret;
	int rc;

	printf("TEST: Delete operation creates tombstone\n");

	ret = hbf_db_init(1, &db);
	assert(ret == 0);
	assert(db != NULL);

	/* Insert base content */
	const char *insert_base = "INSERT INTO sqlar (name, mode, mtime, sz, data) "
	                          "VALUES (?, 33188, 1234567890, ?, ?)";

	rc = sqlite3_prepare_v2(db, insert_base, -1, &stmt, NULL);
	assert(rc == SQLITE_OK);

	sqlite3_bind_text(stmt, 1, test_name, -1, SQLITE_STATIC);
	sqlite3_bind_int(stmt, 2, (int)strlen(base_content));
	sqlite3_bind_blob(stmt, 3, base_content, (int)strlen(base_content), SQLITE_STATIC);

	rc = sqlite3_step(stmt);
	assert(rc == SQLITE_DONE);
	sqlite3_finalize(stmt);

	printf("  ✓ Inserted base content\n");

	/* Delete file via overlay */
	const char *delete_sql = "INSERT INTO fs_layer (layer_id, name, op, mtime, sz, data) "
	                         "VALUES (1, ?, 'delete', 1234567890, 0, NULL)";

	rc = sqlite3_prepare_v2(db, delete_sql, -1, &stmt, NULL);
	assert(rc == SQLITE_OK);

	sqlite3_bind_text(stmt, 1, test_name, -1, SQLITE_STATIC);

	rc = sqlite3_step(stmt);
	assert(rc == SQLITE_DONE);
	sqlite3_finalize(stmt);

	printf("  ✓ Inserted delete operation\n");

	/* Try to read with overlay - should fail (file deleted) */
	ret = hbf_db_read_file(db, test_name, 1, &data, &size);
	assert(ret != 0);
	assert(data == NULL);

	printf("  ✓ File not found in overlay (tombstone works)\n");

	/* Read without overlay - should still work (base not modified) */
	ret = hbf_db_read_file(db, test_name, 0, &data, &size);
	assert(ret == 0);
	assert(data != NULL);

	printf("  ✓ File still accessible in base\n");

	free(data);
	hbf_db_close(db);
	printf("PASS: Delete operation works\n\n");
}

int main(void)
{
	/* Initialize logging */
	hbf_log_init(HBF_LOG_INFO);

	printf("=== Overlay Filesystem Tests ===\n\n");

	test_overlay_tables_exist();
	test_overlay_trigger();
	test_read_with_overlay();
	test_read_without_overlay();
	test_overlay_delete();

	printf("=== All tests passed! ===\n");
	return 0;
}
