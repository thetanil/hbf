/* SPDX-License-Identifier: MIT */
#include "overlay_fs.h"
#include "hbf/shell/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Embedded schema (from schema.sql) */
extern const unsigned char fs_db_data[];
extern const unsigned long fs_db_len;

/* Global database handle for filesystem operations */
/* Set by overlay_fs_init_global(), used by overlay_fs_read_file/overlay_fs_write_file */
static sqlite3 *g_overlay_db = NULL;

static int exec_sql_file(sqlite3 *db, const char *sql)
{
	char *errmsg = NULL;
	int rc = sqlite3_exec(db, sql, NULL, NULL, &errmsg);

	if (rc != SQLITE_OK) {
		hbf_log_error("SQL error: %s", errmsg);
		sqlite3_free(errmsg);
		return -1;
	}
	return 0;
}

int overlay_fs_init(const char *db_path, sqlite3 **db)
{
	int rc;

	if (!db_path || !db) {
		hbf_log_error("overlay_fs_init: invalid arguments");
		return -1;
	}

	/* Open database */
	rc = sqlite3_open(db_path, db);
	if (rc != SQLITE_OK) {
		hbf_log_error("Cannot open database: %s", sqlite3_errmsg(*db));
		return -1;
	}

	/* Enable foreign keys and set pragmas */
	if (exec_sql_file(*db, "PRAGMA foreign_keys=ON;") < 0)
		goto err;
	if (exec_sql_file(*db, "PRAGMA journal_mode=WAL;") < 0)
		goto err;
	if (exec_sql_file(*db, "PRAGMA synchronous=NORMAL;") < 0)
		goto err;

	/* Load embedded schema */
	const char *schema =
		"CREATE TABLE IF NOT EXISTS file_versions (\n"
		"    file_id         INTEGER NOT NULL,\n"
		"    path            TEXT NOT NULL,\n"
		"    version_number  INTEGER NOT NULL,\n"
		"    mtime           INTEGER NOT NULL,\n"
		"    size            INTEGER NOT NULL,\n"
		"    data            BLOB NOT NULL,\n"
		"    PRIMARY KEY (file_id, version_number)\n"
		") WITHOUT ROWID;\n"
		"\n"
		"CREATE INDEX IF NOT EXISTS idx_file_versions_path\n"
		"    ON file_versions(path);\n"
		"\n"
		"CREATE INDEX IF NOT EXISTS idx_file_versions_file_id_version\n"
		"    ON file_versions(file_id, version_number DESC);\n"
		"\n"
		"-- Covering index so metadata queries avoid reading BLOB pages\n"
		"CREATE INDEX IF NOT EXISTS idx_file_versions_latest_cover\n"
		"    ON file_versions(file_id, version_number DESC, path, mtime, size);\n"
		"\n"
		"CREATE TABLE IF NOT EXISTS file_ids (\n"
		"    file_id  INTEGER PRIMARY KEY AUTOINCREMENT,\n"
		"    path     TEXT NOT NULL UNIQUE\n"
		");\n";

	if (exec_sql_file(*db, schema) < 0)
		goto err;

	return 0;

err:
	sqlite3_close(*db);
	*db = NULL;
	return -1;
}

int overlay_fs_migrate_sqlar(sqlite3 *db)
{
	sqlite3_stmt *stmt = NULL;
	int rc;

	if (!db) {
		hbf_log_error("overlay_fs_migrate_sqlar: invalid db");
		return -1;
	}

	/* Check if sqlar table exists */
	const char *check_sql =
		"SELECT name FROM sqlite_master WHERE type='table' AND name='sqlar'";
	rc = sqlite3_prepare_v2(db, check_sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to check sqlar table: %s", sqlite3_errmsg(db));
		return -1;
	}

	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	if (rc != SQLITE_ROW) {
		/* No sqlar table, nothing to migrate */
		return 0;
	}

	hbf_log_info("Migrating SQLAR archive to file_versions...");

	/* Begin transaction */
	if (exec_sql_file(db, "BEGIN TRANSACTION;") < 0)
		return -1;

	/* Read from sqlar and insert into file_versions */
	/* Use sqlar_uncompress to decompress data before migration */
	const char *select_sql =
		"SELECT name, sqlar_uncompress(data, sz) FROM sqlar";

	rc = sqlite3_prepare_v2(db, select_sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to prepare select: %s", sqlite3_errmsg(db));
		exec_sql_file(db, "ROLLBACK;");
		return -1;
	}

	int file_count = 0;

	while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		const char *name = (const char *)sqlite3_column_text(stmt, 0);
		const void *data = sqlite3_column_blob(stmt, 1);
		int data_size = sqlite3_column_bytes(stmt, 1);

		if (!name) {
			hbf_log_debug("Skipping entry with NULL name");
			continue;
		}

		/* Handle empty files or directories (data can be NULL for size 0) */
		if (data_size == 0 || !data) {
			/* Skip directories and empty entries */
			hbf_log_debug("Skipping empty entry: %s", name);
			continue;
		}

		if (data_size < 0) {
			hbf_log_error("Invalid data size for: %s", name);
			continue;
		}

		/* Write decompressed data to file_versions using overlay_fs_write */
		if (overlay_fs_write(db, name, data, (size_t)data_size) < 0) {
			hbf_log_error("Failed to migrate file: %s", name);
			sqlite3_finalize(stmt);
			exec_sql_file(db, "ROLLBACK;");
			return -1;
		}
		file_count++;
	}

	sqlite3_finalize(stmt);

	if (rc != SQLITE_DONE) {
		hbf_log_error("Error reading sqlar: %s", sqlite3_errmsg(db));
		exec_sql_file(db, "ROLLBACK;");
		return -1;
	}

	hbf_log_info("Migrated %d files from SQLAR", file_count);

	/* Drop sqlar table */
	if (exec_sql_file(db, "DROP TABLE IF EXISTS sqlar;") < 0) {
		exec_sql_file(db, "ROLLBACK;");
		return -1;
	}

	/* Commit transaction */
	if (exec_sql_file(db, "COMMIT;") < 0)
		return -1;

	/* VACUUM to reclaim space */
	hbf_log_info("Vacuuming database...");
	if (exec_sql_file(db, "VACUUM;") < 0)
		return -1;

	return 0;
}

int overlay_fs_read(sqlite3 *db, const char *path,
                    unsigned char **data, size_t *size)
{
	sqlite3_stmt *stmt = NULL;
	int rc;

	if (!db || !path || !data || !size) {
		hbf_log_error("overlay_fs_read: invalid arguments");
		return -1;
	}

	*data = NULL;
	*size = 0;

	/* Fast query using file_id and MAX(version_number) */
	const char *sql =
		"WITH file_info AS (\n"
		"    SELECT file_id FROM file_ids WHERE path = ?\n"
		")\n"
		"SELECT fv.data\n"
		"FROM file_versions fv, file_info fi\n"
		"WHERE fv.file_id = fi.file_id\n"
		"ORDER BY fv.version_number DESC\n"
		"LIMIT 1";

	rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to prepare read: %s", sqlite3_errmsg(db));
		return -1;
	}

	sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);

	rc = sqlite3_step(stmt);
	if (rc == SQLITE_ROW) {
		const void *blob = sqlite3_column_blob(stmt, 0);
		int blob_size = sqlite3_column_bytes(stmt, 0);

		if (blob_size > 0) {
			*data = malloc((size_t)blob_size);
			if (!*data) {
				hbf_log_error("Memory allocation failed");
				sqlite3_finalize(stmt);
				return -1;
			}
			memcpy(*data, blob, (size_t)blob_size);
			*size = (size_t)blob_size;
		} else {
			/* Empty file - allocate minimal buffer */
			*data = malloc(1);
			if (!*data) {
				hbf_log_error("Memory allocation failed");
				sqlite3_finalize(stmt);
				return -1;
			}
			*size = 0;
		}
		sqlite3_finalize(stmt);
		return 0;
	} else if (rc == SQLITE_DONE) {
		/* File not found */
		sqlite3_finalize(stmt);
		return -1;
	} else {
		hbf_log_error("Error reading file: %s", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return -1;
	}
}

int overlay_fs_write(sqlite3 *db, const char *path,
                     const unsigned char *data, size_t size)
{
	sqlite3_stmt *stmt = NULL;
	int rc;
	int file_id = -1;
	int next_version = 1;

	if (!db || !path) {
		hbf_log_error("overlay_fs_write: invalid arguments");
		return -1;
	}

	/* Allow empty files (data can be NULL or empty string if size is 0) */
	if (size > 0 && !data) {
		hbf_log_error("overlay_fs_write: data is NULL but size > 0");
		return -1;
	}

	/* Get or create file_id */
	const char *get_id_sql = "SELECT file_id FROM file_ids WHERE path = ?";

	rc = sqlite3_prepare_v2(db, get_id_sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to prepare get_id: %s", sqlite3_errmsg(db));
		return -1;
	}

	sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
	rc = sqlite3_step(stmt);

	if (rc == SQLITE_ROW) {
		file_id = sqlite3_column_int(stmt, 0);
	}
	sqlite3_finalize(stmt);

	if (file_id < 0) {
		/* Create new file_id */
		const char *insert_id_sql = "INSERT INTO file_ids (path) VALUES (?)";

		rc = sqlite3_prepare_v2(db, insert_id_sql, -1, &stmt, NULL);
		if (rc != SQLITE_OK) {
			hbf_log_error("Failed to prepare insert_id: %s", sqlite3_errmsg(db));
			return -1;
		}

		sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
		rc = sqlite3_step(stmt);
		sqlite3_finalize(stmt);

		if (rc != SQLITE_DONE) {
			hbf_log_error("Failed to insert file_id: %s", sqlite3_errmsg(db));
			return -1;
		}

		file_id = (int)sqlite3_last_insert_rowid(db);
	} else {
		/* Get next version number */
		const char *get_version_sql =
			"SELECT MAX(version_number) FROM file_versions WHERE file_id = ?";
		rc = sqlite3_prepare_v2(db, get_version_sql, -1, &stmt, NULL);
		if (rc != SQLITE_OK) {
			hbf_log_error("Failed to prepare get_version: %s", sqlite3_errmsg(db));
			return -1;
		}

		sqlite3_bind_int(stmt, 1, file_id);
		rc = sqlite3_step(stmt);

		if (rc == SQLITE_ROW) {
			next_version = sqlite3_column_int(stmt, 0) + 1;
		}
		sqlite3_finalize(stmt);
	}

	/* Insert new version */
	const char *insert_sql =
		"INSERT INTO file_versions (file_id, path, version_number, mtime, size, data) "
		"VALUES (?, ?, ?, ?, ?, ?)";

	rc = sqlite3_prepare_v2(db, insert_sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to prepare insert: %s", sqlite3_errmsg(db));
		return -1;
	}

	time_t now = time(NULL);

	sqlite3_bind_int(stmt, 1, file_id);
	sqlite3_bind_text(stmt, 2, path, -1, SQLITE_STATIC);
	sqlite3_bind_int(stmt, 3, next_version);
	sqlite3_bind_int64(stmt, 4, (sqlite3_int64)now);
	sqlite3_bind_int64(stmt, 5, (sqlite3_int64)size);
	sqlite3_bind_blob(stmt, 6, data, (int)size, SQLITE_STATIC);

	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	if (rc != SQLITE_DONE) {
		hbf_log_error("Failed to insert version: %s", sqlite3_errmsg(db));
		return -1;
	}

	return 0;
}

int overlay_fs_exists(sqlite3 *db, const char *path)
{
	sqlite3_stmt *stmt = NULL;
	int rc;

	if (!db || !path) {
		hbf_log_error("overlay_fs_exists: invalid arguments");
		return -1;
	}

	const char *sql = "SELECT 1 FROM file_ids WHERE path = ? LIMIT 1";

	rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to prepare exists: %s", sqlite3_errmsg(db));
		return -1;
	}

	sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	if (rc == SQLITE_ROW)
		return 1;
	else if (rc == SQLITE_DONE)
		return 0;
	else
		return -1;
}

int overlay_fs_version_count(sqlite3 *db, const char *path)
{
	sqlite3_stmt *stmt = NULL;
	int rc;
	int count = 0;

	if (!db || !path) {
		hbf_log_error("overlay_fs_version_count: invalid arguments");
		return -1;
	}

	const char *sql =
		"SELECT COUNT(*) FROM file_versions fv "
		"JOIN file_ids fi ON fv.file_id = fi.file_id "
		"WHERE fi.path = ?";

	rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to prepare count: %s", sqlite3_errmsg(db));
		return -1;
	}

	sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
	rc = sqlite3_step(stmt);

	if (rc == SQLITE_ROW) {
		count = sqlite3_column_int(stmt, 0);
	}

	sqlite3_finalize(stmt);

	if (rc != SQLITE_ROW)
		return -1;

	return count;
}

void overlay_fs_close(sqlite3 *db)
{
	if (db) {
		sqlite3_close(db);
	}
}

void overlay_fs_init_global(sqlite3 *db)
{
	g_overlay_db = db;
	if (db) {
		hbf_log_info("overlay_fs: Global database handle initialized");
	} else {
		hbf_log_warn("overlay_fs: Global database handle set to NULL");
	}
}

int overlay_fs_read_file(const char *path, int dev,
                         unsigned char **data, size_t *size)
{
	sqlite3_stmt *stmt = NULL;
	int rc;

	if (!g_overlay_db) {
		hbf_log_error("overlay_fs_read_file: global database not initialized");
		return -1;
	}

	if (!path || !data || !size) {
		hbf_log_error("overlay_fs_read_file: invalid arguments");
		return -1;
	}

	*data = NULL;
	*size = 0;

	/* Read from latest_files view (contains migrated SQLAR data + overlays) */
	/* dev parameter reserved for future use */
	(void)dev; /* Suppress unused parameter warning */

	const char *sql = "SELECT data FROM latest_files WHERE path = ?";

	rc = sqlite3_prepare_v2(g_overlay_db, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to prepare file read: %s",
		              sqlite3_errmsg(g_overlay_db));
		return -1;
	}

	sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);

	rc = sqlite3_step(stmt);
	if (rc == SQLITE_ROW) {
		const void *blob = sqlite3_column_blob(stmt, 0);
		int blob_size = sqlite3_column_bytes(stmt, 0);

		if (blob_size > 0) {
			*data = malloc((size_t)blob_size);
			if (!*data) {
				hbf_log_error("Memory allocation failed");
				sqlite3_finalize(stmt);
				return -1;
			}
			memcpy(*data, blob, (size_t)blob_size);
			*size = (size_t)blob_size;
		} else {
			/* Empty file - allocate minimal buffer */
			*data = malloc(1);
			if (!*data) {
				hbf_log_error("Memory allocation failed");
				sqlite3_finalize(stmt);
				return -1;
			}
			*size = 0;
		}
		sqlite3_finalize(stmt);
		hbf_log_debug("overlay_fs: Read file '%s' (%zu bytes)", path, *size);
		return 0;
	} else if (rc == SQLITE_DONE) {
		/* File not found */
		sqlite3_finalize(stmt);
		hbf_log_debug("overlay_fs: File not found: %s", path);
		return -1;
	} else {
		hbf_log_error("Error reading file: %s", sqlite3_errmsg(g_overlay_db));
		sqlite3_finalize(stmt);
		return -1;
	}
}

int overlay_fs_write_file(const char *path,
                          const unsigned char *data, size_t size)
{
	if (!g_overlay_db) {
		hbf_log_error("overlay_fs_write_file: global database not initialized");
		return -1;
	}

	/* Delegate to existing overlay_fs_write implementation */
	return overlay_fs_write(g_overlay_db, path, data, size);
}
