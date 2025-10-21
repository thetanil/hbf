/* SPDX-License-Identifier: MIT */
#include "db.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define TEST_DB_PATH "/tmp/hbf_db_test.db"

static void cleanup_test_db(void)
{
	unlink(TEST_DB_PATH);
	unlink("/tmp/hbf_db_test.db-wal");
	unlink("/tmp/hbf_db_test.db-shm");
}

static void test_db_open_close(void)
{
	sqlite3 *db = NULL;
	int ret;

	cleanup_test_db();

	ret = hbf_db_open(TEST_DB_PATH, &db);
	assert(ret == 0);
	assert(db != NULL);

	hbf_db_close(db);
	cleanup_test_db();

	printf("  ✓ Open and close database\n");
}

static void test_db_open_null_args(void)
{
	sqlite3 *db = NULL;
	int ret;

	ret = hbf_db_open(NULL, &db);
	assert(ret == -1);

	ret = hbf_db_open(TEST_DB_PATH, NULL);
	assert(ret == -1);

	printf("  ✓ NULL argument handling\n");
}

static void test_db_pragmas(void)
{
	sqlite3 *db = NULL;
	sqlite3_stmt *stmt;
	int ret;
	const char *text;

	cleanup_test_db();

	ret = hbf_db_open(TEST_DB_PATH, &db);
	assert(ret == 0);
	assert(db != NULL);

	/* Check foreign_keys pragma */
	ret = sqlite3_prepare_v2(db, "PRAGMA foreign_keys", -1, &stmt, NULL);
	assert(ret == SQLITE_OK);
	ret = sqlite3_step(stmt);
	assert(ret == SQLITE_ROW);
	assert(sqlite3_column_int(stmt, 0) == 1);
	sqlite3_finalize(stmt);

	/* Check journal_mode pragma */
	ret = sqlite3_prepare_v2(db, "PRAGMA journal_mode", -1, &stmt, NULL);
	assert(ret == SQLITE_OK);
	ret = sqlite3_step(stmt);
	assert(ret == SQLITE_ROW);
	text = (const char *)sqlite3_column_text(stmt, 0);
	assert(strcmp(text, "wal") == 0);
	sqlite3_finalize(stmt);

	/* Check synchronous pragma */
	ret = sqlite3_prepare_v2(db, "PRAGMA synchronous", -1, &stmt, NULL);
	assert(ret == SQLITE_OK);
	ret = sqlite3_step(stmt);
	assert(ret == SQLITE_ROW);
	assert(sqlite3_column_int(stmt, 0) == 1); /* NORMAL = 1 */
	sqlite3_finalize(stmt);

	hbf_db_close(db);
	cleanup_test_db();

	printf("  ✓ Database pragmas configured correctly\n");
}

static void test_db_exec(void)
{
	sqlite3 *db = NULL;
	int ret;

	cleanup_test_db();

	ret = hbf_db_open(TEST_DB_PATH, &db);
	assert(ret == 0);

	/* Create a simple table */
	ret = hbf_db_exec(db, "CREATE TABLE test (id INTEGER PRIMARY KEY, value TEXT)");
	assert(ret == 0);

	/* Insert data */
	ret = hbf_db_exec(db, "INSERT INTO test (value) VALUES ('hello')");
	assert(ret == 0);

	/* Invalid SQL should fail */
	ret = hbf_db_exec(db, "INVALID SQL");
	assert(ret == -1);

	hbf_db_close(db);
	cleanup_test_db();

	printf("  ✓ Execute SQL statements\n");
}

static void test_db_transactions(void)
{
	sqlite3 *db = NULL;
	int ret;

	cleanup_test_db();

	ret = hbf_db_open(TEST_DB_PATH, &db);
	assert(ret == 0);

	ret = hbf_db_exec(db, "CREATE TABLE test (id INTEGER PRIMARY KEY, value TEXT)");
	assert(ret == 0);

	/* Begin transaction */
	ret = hbf_db_begin(db);
	assert(ret == 0);

	/* Insert data */
	ret = hbf_db_exec(db, "INSERT INTO test (value) VALUES ('test1')");
	assert(ret == 0);

	/* Commit transaction */
	ret = hbf_db_commit(db);
	assert(ret == 0);

	/* Begin another transaction and rollback */
	ret = hbf_db_begin(db);
	assert(ret == 0);

	ret = hbf_db_exec(db, "INSERT INTO test (value) VALUES ('test2')");
	assert(ret == 0);

	ret = hbf_db_rollback(db);
	assert(ret == 0);

	hbf_db_close(db);
	cleanup_test_db();

	printf("  ✓ Transaction management (begin, commit, rollback)\n");
}

static void test_db_last_insert_id(void)
{
	sqlite3 *db = NULL;
	int64_t id;
	int ret;

	cleanup_test_db();

	ret = hbf_db_open(TEST_DB_PATH, &db);
	assert(ret == 0);

	ret = hbf_db_exec(db, "CREATE TABLE test (id INTEGER PRIMARY KEY, value TEXT)");
	assert(ret == 0);

	ret = hbf_db_exec(db, "INSERT INTO test (value) VALUES ('test')");
	assert(ret == 0);

	id = hbf_db_last_insert_id(db);
	assert(id == 1);

	ret = hbf_db_exec(db, "INSERT INTO test (value) VALUES ('test2')");
	assert(ret == 0);

	id = hbf_db_last_insert_id(db);
	assert(id == 2);

	hbf_db_close(db);
	cleanup_test_db();

	printf("  ✓ Last insert ID retrieval\n");
}

static void test_db_changes(void)
{
	sqlite3 *db = NULL;
	int changes;
	int ret;

	cleanup_test_db();

	ret = hbf_db_open(TEST_DB_PATH, &db);
	assert(ret == 0);

	ret = hbf_db_exec(db, "CREATE TABLE test (id INTEGER PRIMARY KEY, value TEXT)");
	assert(ret == 0);

	ret = hbf_db_exec(db, "INSERT INTO test (value) VALUES ('test1')");
	assert(ret == 0);
	changes = hbf_db_changes(db);
	assert(changes == 1);

	ret = hbf_db_exec(db, "INSERT INTO test (value) VALUES ('test2')");
	assert(ret == 0);
	ret = hbf_db_exec(db, "INSERT INTO test (value) VALUES ('test3')");
	assert(ret == 0);

	ret = hbf_db_exec(db, "UPDATE test SET value = 'updated' WHERE 1=1");
	assert(ret == 0);
	changes = hbf_db_changes(db);
	assert(changes == 3);

	hbf_db_close(db);
	cleanup_test_db();

	printf("  ✓ Row change count\n");
}

int main(void)
{
	printf("Running database tests:\n");

	test_db_open_close();
	test_db_open_null_args();
	test_db_pragmas();
	test_db_exec();
	test_db_transactions();
	test_db_last_insert_id();
	test_db_changes();

	printf("\nAll database tests passed!\n");
	return 0;
}
