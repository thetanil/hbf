/* SPDX-License-Identifier: MIT */
#include "db.h"
#include "../core/log.h"
#include <string.h>

int hbf_db_open(const char *path, sqlite3 **db)
{
	int rc;

	if (!path || !db) {
		return -1;
	}

	/* Open database */
	rc = sqlite3_open(path, db);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to open database %s: %s", path, sqlite3_errmsg(*db));
		sqlite3_close(*db);
		*db = NULL;
		return -1;
	}

	/* Enable foreign keys */
	rc = sqlite3_exec(*db, "PRAGMA foreign_keys=ON", NULL, NULL, NULL);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to enable foreign keys: %s", sqlite3_errmsg(*db));
		sqlite3_close(*db);
		*db = NULL;
		return -1;
	}

	/* Set journal mode to WAL */
	rc = sqlite3_exec(*db, "PRAGMA journal_mode=WAL", NULL, NULL, NULL);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to set WAL mode: %s", sqlite3_errmsg(*db));
		sqlite3_close(*db);
		*db = NULL;
		return -1;
	}

	/* Set synchronous to NORMAL (safe with WAL) */
	rc = sqlite3_exec(*db, "PRAGMA synchronous=NORMAL", NULL, NULL, NULL);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to set synchronous mode: %s", sqlite3_errmsg(*db));
		sqlite3_close(*db);
		*db = NULL;
		return -1;
	}

	hbf_log_debug("Opened database: %s (WAL mode, foreign keys enabled)", path);
	return 0;
}

void hbf_db_close(sqlite3 *db)
{
	if (db) {
		sqlite3_close(db);
	}
}

int hbf_db_exec(sqlite3 *db, const char *sql)
{
	char *errmsg;
	int rc;

	if (!db || !sql) {
		return -1;
	}

	rc = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
	if (rc != SQLITE_OK) {
		hbf_log_error("SQL execution failed: %s", errmsg ? errmsg : "unknown error");
		sqlite3_free(errmsg);
		return -1;
	}

	return 0;
}

const char *hbf_db_error(sqlite3 *db)
{
	if (!db) {
		return "NULL database handle";
	}
	return sqlite3_errmsg(db);
}

int64_t hbf_db_last_insert_id(sqlite3 *db)
{
	if (!db) {
		return -1;
	}
	return (int64_t)sqlite3_last_insert_rowid(db);
}

int hbf_db_changes(sqlite3 *db)
{
	if (!db) {
		return -1;
	}
	return sqlite3_changes(db);
}

int hbf_db_begin(sqlite3 *db)
{
	return hbf_db_exec(db, "BEGIN TRANSACTION");
}

int hbf_db_commit(sqlite3 *db)
{
	return hbf_db_exec(db, "COMMIT");
}

int hbf_db_rollback(sqlite3 *db)
{
	return hbf_db_exec(db, "ROLLBACK");
}
