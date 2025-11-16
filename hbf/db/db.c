/* SPDX-License-Identifier: MIT */
#include "db.h"
#include "hbf/shell/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HBF_DB_PATH "./hbf.db"
#define HBF_DB_INMEM ":memory:"

/* Global database handle for process-lifetime access */
static sqlite3 *g_db = NULL;

int hbf_db_init(int inmem, sqlite3 **db)
{
	int rc;
	const char *db_path;

	if (!db) {
		hbf_log_error("NULL database handle pointer");
		return -1;
	}

	*db = NULL;

	/* Determine database path */
	db_path = inmem ? HBF_DB_INMEM : HBF_DB_PATH;

	/* Open or create database */
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
	rc = sqlite3_exec(*db, "PRAGMA journal_mode=WAL", NULL, NULL, NULL);
	if (rc != SQLITE_OK) {
		hbf_log_warn("Failed to enable WAL mode: %s", sqlite3_errmsg(*db));
	} else {
		hbf_log_info("Enabled WAL mode");
	}

	rc = sqlite3_exec(*db, "PRAGMA foreign_keys=ON", NULL, NULL, NULL);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to enable foreign keys: %s",
		              sqlite3_errmsg(*db));
		sqlite3_close(*db);
		*db = NULL;
		return -1;
	}

	/* Store global database handle */
	g_db = *db;

	hbf_log_info("Database initialized successfully");
	return 0;
}

void hbf_db_close(sqlite3 *db)
{
	if (db) {
		sqlite3_close(db);
		if (g_db == db) {
			g_db = NULL;
		}
		hbf_log_info("Database closed");
	}
}

sqlite3 *hbf_db_get(void)
{
	return g_db;
}
