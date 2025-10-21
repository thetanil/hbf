/* SPDX-License-Identifier: MIT */
#include "manager.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Test counter */
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_STORAGE_DIR "/tmp/hbf_test_henv"

/* Cleanup helper */
static void cleanup_test_dir(void)
{
	char cmd[512];
	snprintf(cmd, sizeof(cmd), "rm -rf %s", TEST_STORAGE_DIR);
	system(cmd);
}

/* Test helper macros */
#define RUN_TEST(name)                                                         \
	do {                                                                   \
		printf("Running test: %s\n", #name);                           \
		cleanup_test_dir();                                            \
		test_##name();                                                 \
	} while (0)

#define ASSERT_TRUE(cond, msg)                                                 \
	do {                                                                   \
		if (!(cond)) {                                                 \
			printf("  FAIL: %s\n", msg);                           \
			tests_failed++;                                        \
			return;                                                \
		}                                                              \
	} while (0)

#define ASSERT_EQ(a, b, msg)                                                   \
	do {                                                                   \
		if ((a) != (b)) {                                              \
			printf("  FAIL: %s (expected %d, got %d)\n", msg, (int)(b), (int)(a)); \
			tests_failed++;                                        \
			return;                                                \
		}                                                              \
	} while (0)

#define ASSERT_STREQ(a, b, msg)                                                \
	do {                                                                   \
		if (strcmp((a), (b)) != 0) {                                   \
			printf("  FAIL: %s (expected '%s', got '%s')\n", msg, b, a); \
			tests_failed++;                                        \
			return;                                                \
		}                                                              \
	} while (0)

#define TEST_PASS()                                                            \
	do {                                                                   \
		printf("  PASS\n");                                            \
		tests_passed++;                                                \
	} while (0)

/* Test 1: Initialize and shutdown henv manager */
static void test_init_shutdown(void)
{
	int ret;

	ret = hbf_henv_init(TEST_STORAGE_DIR, 10);
	ASSERT_EQ(ret, 0, "henv_init should succeed");

	/* Check that storage directory was created */
	struct stat st;
	ret = stat(TEST_STORAGE_DIR, &st);
	ASSERT_EQ(ret, 0, "storage_dir should exist");
	ASSERT_TRUE(S_ISDIR(st.st_mode), "storage_dir should be a directory");

	hbf_henv_shutdown();
	TEST_PASS();
}

/* Test 2: Create user pod */
static void test_create_user_pod(void)
{
	int ret;
	char pod_dir[512];
	char db_path[512];
	struct stat st;

	ret = hbf_henv_init(TEST_STORAGE_DIR, 10);
	ASSERT_EQ(ret, 0, "henv_init should succeed");

	ret = hbf_henv_create_user_pod("abc12345");
	ASSERT_EQ(ret, 0, "create_user_pod should succeed");

	/* Check directory created with mode 0700 */
	snprintf(pod_dir, sizeof(pod_dir), "%s/abc12345", TEST_STORAGE_DIR);
	ret = stat(pod_dir, &st);
	ASSERT_EQ(ret, 0, "user pod directory should exist");
	ASSERT_TRUE(S_ISDIR(st.st_mode), "user pod should be a directory");
	ASSERT_EQ(st.st_mode & 0777, 0700, "user pod should have mode 0700");

	/* Check database file created with mode 0600 */
	snprintf(db_path, sizeof(db_path), "%s/abc12345/index.db", TEST_STORAGE_DIR);
	ret = stat(db_path, &st);
	ASSERT_EQ(ret, 0, "database file should exist");
	ASSERT_TRUE(S_ISREG(st.st_mode), "database should be a regular file");
	ASSERT_EQ(st.st_mode & 0777, 0600, "database should have mode 0600");

	hbf_henv_shutdown();
	TEST_PASS();
}

/* Test 3: Hash collision detection */
static void test_hash_collision(void)
{
	int ret;

	ret = hbf_henv_init(TEST_STORAGE_DIR, 10);
	ASSERT_EQ(ret, 0, "henv_init should succeed");

	ret = hbf_henv_create_user_pod("xyz98765");
	ASSERT_EQ(ret, 0, "first create_user_pod should succeed");

	ret = hbf_henv_create_user_pod("xyz98765");
	ASSERT_EQ(ret, -1, "second create_user_pod should fail (collision)");

	hbf_henv_shutdown();
	TEST_PASS();
}

/* Test 4: User exists check */
static void test_user_exists(void)
{
	int ret;
	bool exists;

	ret = hbf_henv_init(TEST_STORAGE_DIR, 10);
	ASSERT_EQ(ret, 0, "henv_init should succeed");

	exists = hbf_henv_user_exists("test1234");
	ASSERT_TRUE(!exists, "user should not exist before creation");

	ret = hbf_henv_create_user_pod("test1234");
	ASSERT_EQ(ret, 0, "create_user_pod should succeed");

	exists = hbf_henv_user_exists("test1234");
	ASSERT_TRUE(exists, "user should exist after creation");

	hbf_henv_shutdown();
	TEST_PASS();
}

/* Test 5: Open user pod database */
static void test_open_user_pod(void)
{
	int ret;
	sqlite3 *db = NULL;

	ret = hbf_henv_init(TEST_STORAGE_DIR, 10);
	ASSERT_EQ(ret, 0, "henv_init should succeed");

	ret = hbf_henv_create_user_pod("open1234");
	ASSERT_EQ(ret, 0, "create_user_pod should succeed");

	ret = hbf_henv_open("open1234", &db);
	ASSERT_EQ(ret, 0, "henv_open should succeed");
	ASSERT_TRUE(db != NULL, "database handle should not be NULL");

	hbf_henv_shutdown();
	TEST_PASS();
}

/* Test 6: Connection caching (open same henv twice) */
static void test_connection_caching(void)
{
	int ret;
	sqlite3 *db1 = NULL;
	sqlite3 *db2 = NULL;

	ret = hbf_henv_init(TEST_STORAGE_DIR, 10);
	ASSERT_EQ(ret, 0, "henv_init should succeed");

	ret = hbf_henv_create_user_pod("cache123");
	ASSERT_EQ(ret, 0, "create_user_pod should succeed");

	ret = hbf_henv_open("cache123", &db1);
	ASSERT_EQ(ret, 0, "first henv_open should succeed");
	ASSERT_TRUE(db1 != NULL, "first database handle should not be NULL");

	ret = hbf_henv_open("cache123", &db2);
	ASSERT_EQ(ret, 0, "second henv_open should succeed");
	ASSERT_TRUE(db2 != NULL, "second database handle should not be NULL");

	/* Should return same cached connection */
	ASSERT_TRUE(db1 == db2, "should return same cached connection");

	hbf_henv_shutdown();
	TEST_PASS();
}

/* Test 7: Open non-existent user pod */
static void test_open_nonexistent(void)
{
	int ret;
	sqlite3 *db = NULL;

	ret = hbf_henv_init(TEST_STORAGE_DIR, 10);
	ASSERT_EQ(ret, 0, "henv_init should succeed");

	ret = hbf_henv_open("noexist1", &db);
	ASSERT_EQ(ret, -1, "henv_open should fail for non-existent pod");

	hbf_henv_shutdown();
	TEST_PASS();
}

/* Test 8: Multiple user pods */
static void test_multiple_pods(void)
{
	int ret;
	sqlite3 *db1 = NULL;
	sqlite3 *db2 = NULL;

	ret = hbf_henv_init(TEST_STORAGE_DIR, 10);
	ASSERT_EQ(ret, 0, "henv_init should succeed");

	ret = hbf_henv_create_user_pod("user0001");
	ASSERT_EQ(ret, 0, "create first user pod should succeed");

	ret = hbf_henv_create_user_pod("user0002");
	ASSERT_EQ(ret, 0, "create second user pod should succeed");

	ret = hbf_henv_open("user0001", &db1);
	ASSERT_EQ(ret, 0, "open first pod should succeed");

	ret = hbf_henv_open("user0002", &db2);
	ASSERT_EQ(ret, 0, "open second pod should succeed");

	ASSERT_TRUE(db1 != db2, "different pods should have different handles");

	hbf_henv_shutdown();
	TEST_PASS();
}

/* Test 9: Close all connections */
static void test_close_all(void)
{
	int ret;
	sqlite3 *db1 = NULL;
	sqlite3 *db1_after = NULL;

	ret = hbf_henv_init(TEST_STORAGE_DIR, 10);
	ASSERT_EQ(ret, 0, "henv_init should succeed");

	ret = hbf_henv_create_user_pod("close001");
	ASSERT_EQ(ret, 0, "create user pod should succeed");

	ret = hbf_henv_open("close001", &db1);
	ASSERT_EQ(ret, 0, "first open should succeed");

	hbf_henv_close_all();

	/* After close_all, opening should return a new handle */
	ret = hbf_henv_open("close001", &db1_after);
	ASSERT_EQ(ret, 0, "open after close_all should succeed");
	ASSERT_TRUE(db1_after != NULL, "new handle should not be NULL");
	/* Note: comparing pointers may not work after free, but this tests the logic */

	hbf_henv_shutdown();
	TEST_PASS();
}

/* Test 10: Invalid user hash (wrong length) */
static void test_invalid_hash_length(void)
{
	int ret;

	ret = hbf_henv_init(TEST_STORAGE_DIR, 10);
	ASSERT_EQ(ret, 0, "henv_init should succeed");

	ret = hbf_henv_create_user_pod("short");
	ASSERT_EQ(ret, -1, "create_user_pod with short hash should fail");

	ret = hbf_henv_create_user_pod("toolonghashhh");
	ASSERT_EQ(ret, -1, "create_user_pod with long hash should fail");

	hbf_henv_shutdown();
	TEST_PASS();
}

/* Test 11: Get database path */
static void test_get_db_path(void)
{
	int ret;
	char path[256];

	ret = hbf_henv_init(TEST_STORAGE_DIR, 10);
	ASSERT_EQ(ret, 0, "henv_init should succeed");

	ret = hbf_henv_get_db_path("path1234", path);
	ASSERT_EQ(ret, 0, "get_db_path should succeed");

	/* Expected path format: {storage_dir}/{user_hash}/index.db */
	char expected[256];
	snprintf(expected, sizeof(expected), "%s/path1234/index.db", TEST_STORAGE_DIR);
	ASSERT_STREQ(path, expected, "path should match expected format");

	hbf_henv_shutdown();
	TEST_PASS();
}

/* Test 12: Schema initialized in new pod */
static void test_schema_initialization(void)
{
	int ret;
	sqlite3 *db = NULL;
	sqlite3_stmt *stmt = NULL;

	ret = hbf_henv_init(TEST_STORAGE_DIR, 10);
	ASSERT_EQ(ret, 0, "henv_init should succeed");

	ret = hbf_henv_create_user_pod("schema01");
	ASSERT_EQ(ret, 0, "create_user_pod should succeed");

	ret = hbf_henv_open("schema01", &db);
	ASSERT_EQ(ret, 0, "henv_open should succeed");

	/* Check that nodes table exists */
	const char *sql = "SELECT name FROM sqlite_master WHERE type='table' AND name='nodes'";
	ret = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	ASSERT_EQ(ret, SQLITE_OK, "prepare should succeed");

	ret = sqlite3_step(stmt);
	ASSERT_EQ(ret, SQLITE_ROW, "nodes table should exist");

	sqlite3_finalize(stmt);
	hbf_henv_shutdown();
	TEST_PASS();
}

int main(void)
{
	printf("Starting henv manager tests...\n\n");

	RUN_TEST(init_shutdown);
	RUN_TEST(create_user_pod);
	RUN_TEST(hash_collision);
	RUN_TEST(user_exists);
	RUN_TEST(open_user_pod);
	RUN_TEST(connection_caching);
	RUN_TEST(open_nonexistent);
	RUN_TEST(multiple_pods);
	RUN_TEST(close_all);
	RUN_TEST(invalid_hash_length);
	RUN_TEST(get_db_path);
	RUN_TEST(schema_initialization);

	printf("\n");
	printf("Tests passed: %d\n", tests_passed);
	printf("Tests failed: %d\n", tests_failed);

	cleanup_test_dir();

	return tests_failed > 0 ? 1 : 0;
}
