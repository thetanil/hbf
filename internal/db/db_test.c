/* SPDX-License-Identifier: MIT */
#include "db.h"
#include "../core/log.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void test_db_init_inmem(void)
{
	sqlite3 *db = NULL;
	sqlite3 *fs_db = NULL;
	int ret;

	ret = hbf_db_init(1, &db, &fs_db);
	assert(ret == 0);
	assert(db != NULL);
	assert(fs_db != NULL);

	hbf_db_close(db, fs_db);

	printf("  ✓ In-memory database initialization\n");
}

static void test_db_init_persistent(void)
{
	sqlite3 *db = NULL;
	sqlite3 *fs_db = NULL;
	int ret;

	/* Remove existing test database */
	unlink("./hbf.db");
	unlink("./hbf.db-wal");
	unlink("./hbf.db-shm");

	ret = hbf_db_init(0, &db, &fs_db);
	assert(ret == 0);
	assert(db != NULL);
	assert(fs_db != NULL);

	hbf_db_close(db, fs_db);

	/* Cleanup */
	unlink("./hbf.db");
	unlink("./hbf.db-wal");
	unlink("./hbf.db-shm");

	printf("  ✓ Persistent database initialization\n");
}

static void test_db_read_file(void)
{
	sqlite3 *db = NULL;
	sqlite3 *fs_db = NULL;
	unsigned char *data;
	size_t size;
	int ret;

	ret = hbf_db_init(1, &db, &fs_db);
	assert(ret == 0);

	/* Test reading existing file */
	ret = hbf_db_read_file(fs_db, "hbf/server.js", &data, &size);
	if (ret == 0) {
		assert(data != NULL);
		assert(size > 0);
		free(data);
		printf("  ✓ Read file from archive (server.js, %zu bytes)\n", size);
	} else {
		printf("  ⚠ server.js not found in archive (expected for minimal test)\n");
	}

	/* Test reading non-existent file */
	ret = hbf_db_read_file(fs_db, "nonexistent/file.txt", &data, &size);
	assert(ret == -1);

	printf("  ✓ File read operations\n");

	hbf_db_close(db, fs_db);
}

static void test_db_file_exists(void)
{
	sqlite3 *db = NULL;
	sqlite3 *fs_db = NULL;
	int ret;
	int exists;

	ret = hbf_db_init(1, &db, &fs_db);
	assert(ret == 0);

	/* Test non-existent file */
	exists = hbf_db_file_exists(fs_db, "nonexistent/file.txt");
	assert(exists == 0);

	printf("  ✓ File existence check\n");

	hbf_db_close(db, fs_db);
}

int main(void)
{
	hbf_log_init(hbf_log_parse_level("DEBUG"));

	printf("Database tests:\n");

	test_db_init_inmem();
	test_db_init_persistent();
	test_db_read_file();
	test_db_file_exists();

	printf("\nAll database tests passed!\n");
	return 0;
}
