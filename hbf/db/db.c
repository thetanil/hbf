/* SPDX-License-Identifier: MIT */
#include "db.h"
#include "migrate.h"
#include "overlay_fs.h"
#include "hbf/shell/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declarations for embedded asset bundle symbols */
/* These are provided by the asset packer at binary level */
extern const unsigned char assets_blob[];
extern const size_t assets_blob_len;

#define HBF_DB_PATH "./hbf.db"
#define HBF_DB_INMEM ":memory:"

/* NOLINTNEXTLINE(readability-function-cognitive-complexity) - Complex initialization logic */
int hbf_db_init(int inmem, sqlite3 **db)
{
	int rc;
	const char *db_path;
	migrate_status_t migrate_status;

	if (!db) {
		hbf_log_error("NULL database handle pointer");
		return -1;
	}

	*db = NULL;

	/* Open database (on-disk or in-memory based on flag) */
	db_path = inmem ? HBF_DB_INMEM : HBF_DB_PATH;
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

	hbf_log_info("Opened database: %s", db_path);

	/* Configure database */
	if (!inmem) {
		rc = sqlite3_exec(*db, "PRAGMA journal_mode=WAL", NULL, NULL, NULL);
		if (rc != SQLITE_OK) {
			hbf_log_warn("Failed to enable WAL mode: %s", sqlite3_errmsg(*db));
		}
	}

	rc = sqlite3_exec(*db, "PRAGMA foreign_keys=ON", NULL, NULL, NULL);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to enable foreign keys: %s",
		              sqlite3_errmsg(*db));
		sqlite3_close(*db);
		*db = NULL;
		return -1;
	}

	/* Verify overlay_fs schema exists */
	rc = overlay_fs_check_schema(*db);
	if (rc != 0) {
		hbf_log_error("FATAL: overlay_fs schema is missing from database");
		sqlite3_close(*db);
		*db = NULL;
		return -1;
	}

	/* Migrate embedded assets (idempotent) */
	migrate_status = overlay_fs_migrate_assets(*db, assets_blob, assets_blob_len);
	if (migrate_status == MIGRATE_ERR_ALREADY_APPLIED) {
		hbf_log_info("Asset bundle already migrated (skipped)");
	} else if (migrate_status != MIGRATE_OK) {
		hbf_log_error("Failed to migrate assets: status %d", migrate_status);
		sqlite3_close(*db);
		*db = NULL;
		return -1;
	} else {
		hbf_log_info("Successfully migrated asset bundle");
	}

	/* Initialize overlay_fs global database handle */
	overlay_fs_init_global(*db);

	return 0;
}

void hbf_db_close(sqlite3 *db)
{
	if (db) {
		sqlite3_close(db);
		hbf_log_debug("Closed database");
	}
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

	/* Read from latest_files view (sqlar data was migrated to versioned filesystem) */
	sql = "SELECT data FROM latest_files WHERE path = ?";

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
		hbf_log_debug("File not found in latest_files: %s", path);
		return -1;
	}

	/* Get data (already uncompressed in versioned filesystem) */
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

	hbf_log_debug("Read file '%s' from latest_files (%zu bytes)", path, *size);
	return 0;
}

int hbf_db_read_file(sqlite3 *db, const char *path, int use_overlay,
                     unsigned char **data, size_t *size)
{
	sqlite3_stmt *stmt;
	const char *sql;
	int rc;
	const void *blob_data;
	int blob_size;

	if (!db || !path || !data || !size) {
		hbf_log_error("NULL parameter in hbf_db_read_file");
		return -1;
	}

	*data = NULL;
	*size = 0;

	/* Always read from latest_files view (which contains migrated SQLAR data + overlays) */
	/* SQLAR table is dropped after migration, so all data is now in versioned filesystem */
	(void)use_overlay; /* Suppress unused parameter warning */
	sql = "SELECT data FROM latest_files WHERE path = ?";

	rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to prepare file read: %s", sqlite3_errmsg(db));
		return -1;
	}

	rc = sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to bind path: %s", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return -1;
	}

	rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW) {
		/* File not found */
		sqlite3_finalize(stmt);
		hbf_log_debug("File not found in latest_files: %s", path);
		return -1;
	}

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
		hbf_log_error("Failed to allocate memory for file");
		return -1;
	}

	hbf_log_debug("Read file '%s' from latest_files (%zu bytes)", path, *size);
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

	/* Use latest_files view instead of sqlar table (migrated data) */
	sql = "SELECT 1 FROM latest_files WHERE path = ?";

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
