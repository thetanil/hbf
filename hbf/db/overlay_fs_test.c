/* SPDX-License-Identifier: MIT */
#include "overlay_fs.h"
#include "hbf/shell/log.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Generated from hbf/db/overlay_schema.sql via //hbf/db:overlay_schema_c */
extern const char * const hbf_schema_sql_ptr;
extern const unsigned long hbf_schema_sql_len;

static int open_test_db(sqlite3 **db)
{
	int rc = sqlite3_open(":memory:", db);
	if (rc != SQLITE_OK) return -1;
	char *errmsg = NULL;
    rc = sqlite3_exec(*db, hbf_schema_sql_ptr, NULL, NULL, &errmsg);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "Failed to apply test schema: %s\n", errmsg ? errmsg : "unknown");
		sqlite3_free(errmsg);
		sqlite3_close(*db);
		*db = NULL;
		return -1;
	}
	return 0;
}

static void test_init(void)
{
	sqlite3 *db = NULL;
	int ret;

	ret = open_test_db(&db);
	assert(ret == 0);
	assert(db != NULL);

	overlay_fs_close(db);

	printf("  ✓ Database initialization\n");
}

static void test_write_and_read(void)
{
	sqlite3 *db = NULL;
	unsigned char *data = NULL;
	size_t size = 0;
	int ret;

	ret = open_test_db(&db);
	assert(ret == 0);

	/* Write file */
	const char *content = "Hello, World!";

	ret = overlay_fs_write(db, "test.txt", (const unsigned char *)content,
	                       strlen(content));
	assert(ret == 0);

	/* Read file */
	ret = overlay_fs_read(db, "test.txt", &data, &size);
	assert(ret == 0);
	assert(data != NULL);
	assert(size == strlen(content));
	assert(memcmp(data, content, size) == 0);

	free(data);
	overlay_fs_close(db);

	printf("  ✓ Write and read file\n");
}

static void test_multiple_versions(void)
{
	sqlite3 *db = NULL;
	unsigned char *data = NULL;
	size_t size = 0;
	int ret, count;

	ret = open_test_db(&db);
	assert(ret == 0);

	/* Write version 1 */
	const char *v1 = "Version 1";

	ret = overlay_fs_write(db, "versioned.txt", (const unsigned char *)v1,
	                       strlen(v1));
	assert(ret == 0);

	/* Write version 2 */
	const char *v2 = "Version 2 - Updated";

	ret = overlay_fs_write(db, "versioned.txt", (const unsigned char *)v2,
	                       strlen(v2));
	assert(ret == 0);

	/* Write version 3 */
	const char *v3 = "Version 3 - Final";

	ret = overlay_fs_write(db, "versioned.txt", (const unsigned char *)v3,
	                       strlen(v3));
	assert(ret == 0);

	/* Read latest (should be v3) */
	ret = overlay_fs_read(db, "versioned.txt", &data, &size);
	assert(ret == 0);
	assert(data != NULL);
	assert(size == strlen(v3));
	assert(memcmp(data, v3, size) == 0);
	free(data);

	/* Check version count */
	count = overlay_fs_version_count(db, "versioned.txt");
	assert(count == 3);

	overlay_fs_close(db);

	printf("  ✓ Multiple versions\n");
}

static void test_file_exists(void)
{
	sqlite3 *db = NULL;
	int ret;

	ret = open_test_db(&db);
	assert(ret == 0);

	/* Check non-existent file */
	ret = overlay_fs_exists(db, "nonexistent.txt");
	assert(ret == 0);

	/* Write file */
	const char *content = "Test";

	ret = overlay_fs_write(db, "exists.txt", (const unsigned char *)content,
	                       strlen(content));
	assert(ret == 0);

	/* Check file exists */
	ret = overlay_fs_exists(db, "exists.txt");
	assert(ret == 1);

	overlay_fs_close(db);

	printf("  ✓ File existence check\n");
}

static void test_multiple_files(void)
{
	sqlite3 *db = NULL;
	unsigned char *data = NULL;
	size_t size = 0;
	int ret, i;
	const int num_files = 100;

	ret = open_test_db(&db);
	assert(ret == 0);

	/* Write multiple files */
	for (i = 0; i < num_files; i++) {
		char path[256];
		char content[256];

		snprintf(path, sizeof(path), "file_%d.txt", i);
		snprintf(content, sizeof(content), "Content for file %d", i);

		ret = overlay_fs_write(db, path, (const unsigned char *)content,
		                       strlen(content));
		assert(ret == 0);
	}

	/* Read all files */
	for (i = 0; i < num_files; i++) {
		char path[256];
		char expected[256];

		snprintf(path, sizeof(path), "file_%d.txt", i);
		snprintf(expected, sizeof(expected), "Content for file %d", i);

		ret = overlay_fs_read(db, path, &data, &size);
		assert(ret == 0);
		assert(data != NULL);
		assert(size == strlen(expected));
		assert(memcmp(data, expected, size) == 0);
		free(data);
		data = NULL;
	}

	overlay_fs_close(db);

	printf("  ✓ Multiple files (%d files)\n", num_files);
}

static void test_empty_file(void)
{
	sqlite3 *db = NULL;
	unsigned char *data = NULL;
	size_t size = 0;
	int ret;

	ret = open_test_db(&db);
	assert(ret == 0);

	/* Write empty file */
	ret = overlay_fs_write(db, "empty.txt", (const unsigned char *)"", 0);
	assert(ret == 0);

	/* Read empty file */
	ret = overlay_fs_read(db, "empty.txt", &data, &size);
	assert(ret == 0);
	assert(data != NULL);
	assert(size == 0);

	free(data);
	overlay_fs_close(db);

	printf("  ✓ Empty file\n");
}

static void test_large_file(void)
{
	sqlite3 *db = NULL;
	unsigned char *data = NULL;
	unsigned char *large_data = NULL;
	size_t size = 0;
	int ret, i;
	const size_t large_size = 1024 * 1024; /* 1 MB */

	ret = open_test_db(&db);
	assert(ret == 0);

	/* Create large data */
	large_data = malloc(large_size);
	assert(large_data != NULL);
	for (i = 0; i < (int)large_size; i++) {
		large_data[i] = (unsigned char)(i % 256);
	}

	/* Write large file */
	ret = overlay_fs_write(db, "large.bin", large_data, large_size);
	assert(ret == 0);

	/* Read large file */
	ret = overlay_fs_read(db, "large.bin", &data, &size);
	assert(ret == 0);
	assert(data != NULL);
	assert(size == large_size);
	assert(memcmp(data, large_data, size) == 0);

	free(data);
	free(large_data);
	overlay_fs_close(db);

	printf("  ✓ Large file (1 MB)\n");
}

int main(void)
{
	/* Initialize logging */
	hbf_log_init(HBF_LOG_INFO);

	printf("Running overlay_fs tests...\n\n");

	test_init();
	test_write_and_read();
	test_multiple_versions();
	test_file_exists();
	test_multiple_files();
	test_empty_file();
	test_large_file();

	printf("\n✅ All tests passed\n");
	return 0;
}
