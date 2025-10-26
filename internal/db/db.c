/* SPDX-License-Identifier: MIT */
#include "db.h"
#include "../core/log.h"
#include "fs_embedded.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HBF_DB_PATH "./hbf.db"
#define HBF_DB_INMEM ":memory:"

/* SQLAR extension initialization */
extern int sqlite3_sqlar_init(sqlite3 *db, char **pzErrMsg, const void *pApi);

int hbf_db_init(int inmem, sqlite3 **db, sqlite3 **fs_db)
{
	int rc;
	const char *db_path;
	unsigned char *fs_data_copy;
	int db_exists;
	int has_sqlar;

	if (!db || !fs_db) {
		hbf_log_error("NULL database handle pointers");
		return -1;
	}

	*db = NULL;
	*fs_db = NULL;

	/* Check if database file exists (only matters for non-inmem) */
	db_path = inmem ? HBF_DB_INMEM : HBF_DB_PATH;
	if (inmem) {
		db_exists = 0;
	} else {
		FILE *f = fopen(HBF_DB_PATH, "r");
		if (f) {
			fclose(f);
			db_exists = 1;
		} else {
			db_exists = 0;
		}
	}

	/* If database doesn't exist, copy embedded fs.db to filesystem */
	if (!db_exists && !inmem) {
		hbf_log_info("Database not found, creating from embedded template");

		/* Write embedded fs.db to hbf.db */
		FILE *out = fopen(HBF_DB_PATH, "wb");
		if (!out) {
			hbf_log_error("Failed to create database file: %s", HBF_DB_PATH);
			return -1;
		}

		size_t written = fwrite(fs_db_data, 1, fs_db_len, out);
		fclose(out);

		if (written != fs_db_len) {
			hbf_log_error("Failed to write complete database file");
			return -1;
		}

		hbf_log_info("Created database from embedded template (%lu bytes)", fs_db_len);
	}

	/* Open main database */
	rc = sqlite3_open(db_path, db);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to open database '%s': %s",
		              db_path, sqlite3_errmsg(*db));
		if (*db) {
			sqlite3_close(*db);
			*db = NULL;
		}
		return -1;
	}

	/* If inmem and database doesn't exist, deserialize embedded fs.db into it */
	if (inmem && !db_exists) {
		sqlite3_close(*db);
		*db = NULL;

		rc = sqlite3_open(HBF_DB_INMEM, db);
		if (rc != SQLITE_OK) {
			hbf_log_error("Failed to open in-memory database: %s",
			              sqlite3_errmsg(*db));
			if (*db) {
				sqlite3_close(*db);
				*db = NULL;
			}
			return -1;
		}

		fs_data_copy = sqlite3_malloc64(fs_db_len);
		if (!fs_data_copy) {
			hbf_log_error("Failed to allocate memory for embedded database");
			sqlite3_close(*db);
			*db = NULL;
			return -1;
		}
		memcpy(fs_data_copy, fs_db_data, fs_db_len);

		rc = sqlite3_deserialize(
			*db,
			"main",
			fs_data_copy,
			(sqlite3_int64)fs_db_len,
			(sqlite3_int64)fs_db_len,
			SQLITE_DESERIALIZE_FREEONCLOSE
		);

		if (rc != SQLITE_OK) {
			hbf_log_error("Failed to deserialize embedded database: %s",
			              sqlite3_errmsg(*db));
			sqlite3_free(fs_data_copy);
			sqlite3_close(*db);
			*db = NULL;
			return -1;
		}

		hbf_log_info("Loaded embedded database into memory (%lu bytes)", fs_db_len);
	}

	/* Configure main database */
	rc = sqlite3_exec(*db, "PRAGMA journal_mode=WAL", NULL, NULL, NULL);
	if (rc != SQLITE_OK) {
		hbf_log_warn("Failed to enable WAL mode: %s", sqlite3_errmsg(*db));
	}

	rc = sqlite3_exec(*db, "PRAGMA foreign_keys=ON", NULL, NULL, NULL);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to enable foreign keys: %s",
		              sqlite3_errmsg(*db));
		sqlite3_close(*db);
		*db = NULL;
		return -1;
	}

	/* Initialize SQLAR extension on main database */
	rc = sqlite3_sqlar_init(*db, NULL, NULL);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to initialize SQLAR extension on main db: %s",
		              sqlite3_errmsg(*db));
		sqlite3_close(*db);
		*db = NULL;
		return -1;
	}

	hbf_log_info("Opened main database: %s", db_path);

	/* Check for sqlar table - fatal error if missing */
	has_sqlar = hbf_db_check_sqlar_table(*db);
	if (has_sqlar < 0) {
		hbf_log_error("Failed to check for sqlar table");
		sqlite3_close(*db);
		*db = NULL;
		return -1;
	}

	if (has_sqlar == 0) {
		hbf_log_error("FATAL: Database exists but is missing sqlar table - database is corrupt or invalid");
		sqlite3_close(*db);
		*db = NULL;
		return -1;
	}

	/* Initialize filesystem database (embedded SQLAR) for backwards compatibility */
	rc = sqlite3_open(HBF_DB_INMEM, fs_db);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to open in-memory filesystem database: %s",
		              sqlite3_errmsg(*fs_db));
		if (*fs_db) {
			sqlite3_close(*fs_db);
			*fs_db = NULL;
		}
		sqlite3_close(*db);
		*db = NULL;
		return -1;
	}

	/* Copy embedded data (use sqlite3_malloc for FREEONCLOSE compatibility) */
	fs_data_copy = sqlite3_malloc64(fs_db_len);
	if (!fs_data_copy) {
		hbf_log_error("Failed to allocate memory for embedded database");
		sqlite3_close(*fs_db);
		sqlite3_close(*db);
		*fs_db = NULL;
		*db = NULL;
		return -1;
	}
	memcpy(fs_data_copy, fs_db_data, fs_db_len);

	/* Load embedded database using deserialize API */
	rc = sqlite3_deserialize(
		*fs_db,
		"main",
		fs_data_copy,
		(sqlite3_int64)fs_db_len,
		(sqlite3_int64)fs_db_len,
		SQLITE_DESERIALIZE_FREEONCLOSE
	);

	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to deserialize embedded database: %s",
		              sqlite3_errmsg(*fs_db));
		sqlite3_free(fs_data_copy);
		sqlite3_close(*fs_db);
		sqlite3_close(*db);
		*fs_db = NULL;
		*db = NULL;
		return -1;
	}

	/* Initialize SQLAR extension functions */
	rc = sqlite3_sqlar_init(*fs_db, NULL, NULL);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to initialize SQLAR extension: %s",
		              sqlite3_errmsg(*fs_db));
		sqlite3_close(*fs_db);
		sqlite3_close(*db);
		*fs_db = NULL;
		*db = NULL;
		return -1;
	}

	hbf_log_info("Loaded embedded filesystem database (%lu bytes)", fs_db_len);

	return 0;
}

void hbf_db_close(sqlite3 *db, sqlite3 *fs_db)
{
	if (fs_db) {
		sqlite3_close(fs_db);
		hbf_log_debug("Closed filesystem database");
	}

	if (db) {
		sqlite3_close(db);
		hbf_log_debug("Closed main database");
	}
}

int hbf_db_read_file(sqlite3 *fs_db, const char *path,
                     unsigned char **data, size_t *size)
{
	sqlite3_stmt *stmt;
	const char *sql;
	int rc;
	const void *blob_data;
	int blob_size;

	if (!fs_db || !path || !data || !size) {
		hbf_log_error("NULL parameter in hbf_db_read_file");
		return -1;
	}

	*data = NULL;
	*size = 0;

	/* Use sqlar_uncompress to get uncompressed data */
	sql = "SELECT sqlar_uncompress(data, sz) FROM sqlar WHERE name = ?";

	rc = sqlite3_prepare_v2(fs_db, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to prepare file read query: %s",
		              sqlite3_errmsg(fs_db));
		return -1;
	}

	rc = sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to bind file path: %s",
		              sqlite3_errmsg(fs_db));
		sqlite3_finalize(stmt);
		return -1;
	}

	rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW) {
		/* File not found */
		sqlite3_finalize(stmt);
		hbf_log_debug("File not found in archive: %s", path);
		return -1;
	}

	/* Get uncompressed data */
	blob_data = sqlite3_column_blob(stmt, 0);
	blob_size = sqlite3_column_bytes(stmt, 0);

	if (blob_size > 0 && blob_data) {
		*data = malloc((size_t)blob_size);
		if (*data) {
			memcpy(*data, blob_data, (size_t)blob_size);
			*size = (size_t)blob_size;
		}
	}

	sqlite3_finalize(stmt);

	if (!*data && blob_size > 0) {
		hbf_log_error("Failed to allocate memory for file data");
		return -1;
	}

	hbf_log_debug("Read file '%s' (%zu bytes)", path, *size);
	return 0;
}

int hbf_db_file_exists(sqlite3 *fs_db, const char *path)
{
	sqlite3_stmt *stmt;
	const char *sql;
	int rc;
	int exists;

	if (!fs_db || !path) {
		hbf_log_error("NULL parameter in hbf_db_file_exists");
		return -1;
	}

	sql = "SELECT 1 FROM sqlar WHERE name = ?";

	rc = sqlite3_prepare_v2(fs_db, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to prepare file exists query: %s",
		              sqlite3_errmsg(fs_db));
		return -1;
	}

	rc = sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to bind file path: %s",
		              sqlite3_errmsg(fs_db));
		sqlite3_finalize(stmt);
		return -1;
	}

	rc = sqlite3_step(stmt);
	exists = (rc == SQLITE_ROW) ? 1 : 0;

	sqlite3_finalize(stmt);
	return exists;
}

int hbf_db_read_file_from_main(sqlite3 *db, const char *path,
                                unsigned char **data, size_t *size)
{
	sqlite3_stmt *stmt;
	const char *sql;
	int rc;
	const void *blob_data;
	int blob_size;

	if (!db || !path || !data || !size) {
		hbf_log_error("NULL parameter in hbf_db_read_file_from_main");
		return -1;
	}

	*data = NULL;
	*size = 0;

	/* Use sqlar_uncompress to get uncompressed data from main DB */
	sql = "SELECT sqlar_uncompress(data, sz) FROM sqlar WHERE name = ?";

	rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to prepare file read query on main DB: %s",
		              sqlite3_errmsg(db));
		return -1;
	}

	rc = sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to bind file path: %s",
		              sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return -1;
	}

	rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW) {
		/* File not found */
		sqlite3_finalize(stmt);
		hbf_log_debug("File not found in main DB archive: %s", path);
		return -1;
	}

	/* Get uncompressed data */
	blob_data = sqlite3_column_blob(stmt, 0);
	blob_size = sqlite3_column_bytes(stmt, 0);

	if (blob_size > 0 && blob_data) {
		*data = malloc((size_t)blob_size);
		if (*data) {
			memcpy(*data, blob_data, (size_t)blob_size);
			*size = (size_t)blob_size;
		}
	}

	sqlite3_finalize(stmt);

	if (!*data && blob_size > 0) {
		hbf_log_error("Failed to allocate memory for file data");
		return -1;
	}

	hbf_log_debug("Read file '%s' from main DB (%zu bytes)", path, *size);
	return 0;
}

int hbf_db_file_exists_in_main(sqlite3 *db, const char *path)
{
	sqlite3_stmt *stmt;
	const char *sql;
	int rc;
	int exists;

	if (!db || !path) {
		hbf_log_error("NULL parameter in hbf_db_file_exists_in_main");
		return -1;
	}

	sql = "SELECT 1 FROM sqlar WHERE name = ?";

	rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to prepare file exists query on main DB: %s",
		              sqlite3_errmsg(db));
		return -1;
	}

	rc = sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to bind file path: %s",
		              sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return -1;
	}

	rc = sqlite3_step(stmt);
	exists = (rc == SQLITE_ROW) ? 1 : 0;

	sqlite3_finalize(stmt);
	return exists;
}

int hbf_db_check_sqlar_table(sqlite3 *db)
{
	sqlite3_stmt *stmt;
	const char *sql;
	int rc;
	int exists;

	if (!db) {
		hbf_log_error("NULL database handle in hbf_db_check_sqlar_table");
		return -1;
	}

	sql = "SELECT name FROM sqlite_master WHERE type='table' AND name='sqlar'";

	rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to check for sqlar table: %s",
		              sqlite3_errmsg(db));
		return -1;
	}

	rc = sqlite3_step(stmt);
	exists = (rc == SQLITE_ROW) ? 1 : 0;
	sqlite3_finalize(stmt);

	return exists;
}
