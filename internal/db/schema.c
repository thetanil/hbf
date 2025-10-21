/* SPDX-License-Identifier: MIT */
#include "schema.h"
#include "db.h"
#include "../core/log.h"
#include <stdlib.h>
#include <string.h>

/* Embedded schema SQL - generated from schema.sql by Bazel */
extern const char hbf_schema_sql[];
extern const unsigned long hbf_schema_sql_len;

int hbf_db_init_schema(sqlite3 *db)
{
	int rc;

	if (!db) {
		hbf_log_error("NULL database handle");
		return -1;
	}

	if (hbf_schema_sql_len == 0) {
		hbf_log_error("Empty embedded schema SQL");
		return -1;
	}

	hbf_log_debug("Initializing database schema (%lu bytes)", hbf_schema_sql_len);

	/* Execute schema SQL */
	rc = hbf_db_exec(db, hbf_schema_sql);

	if (rc != 0) {
		hbf_log_error("Failed to initialize schema: %s", hbf_db_error(db));
		return -1;
	}

	hbf_log_info("Database schema initialized successfully");
	return 0;
}

int hbf_db_get_schema_version(sqlite3 *db)
{
	sqlite3_stmt *stmt;
	int version;
	int rc;

	if (!db) {
		return -1;
	}

	/* Check if schema_version table exists first */
	rc = hbf_db_is_initialized(db);
	if (rc <= 0) {
		return rc;
	}

	/* Get latest schema version */
	rc = sqlite3_prepare_v2(db,
		"SELECT MAX(version) FROM _hbf_schema_version",
		-1, &stmt, NULL);

	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to prepare version query: %s", sqlite3_errmsg(db));
		return -1;
	}

	rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW) {
		sqlite3_finalize(stmt);
		hbf_log_error("Failed to get schema version: %s", sqlite3_errmsg(db));
		return -1;
	}

	version = sqlite3_column_int(stmt, 0);
	sqlite3_finalize(stmt);

	return version;
}

int hbf_db_is_initialized(sqlite3 *db)
{
	sqlite3_stmt *stmt;
	int exists;
	int rc;

	if (!db) {
		return -1;
	}

	/* Check if _hbf_schema_version table exists */
	rc = sqlite3_prepare_v2(db,
		"SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='_hbf_schema_version'",
		-1, &stmt, NULL);

	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to prepare table check: %s", sqlite3_errmsg(db));
		return -1;
	}

	rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW) {
		sqlite3_finalize(stmt);
		hbf_log_error("Failed to check schema table: %s", sqlite3_errmsg(db));
		return -1;
	}

	exists = sqlite3_column_int(stmt, 0);
	sqlite3_finalize(stmt);

	return exists > 0 ? 1 : 0;
}
