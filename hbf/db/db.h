/* SPDX-License-Identifier: MIT */
#ifndef HBF_DB_H
#define HBF_DB_H

#include <sqlite3.h>
#include <stddef.h>

/*
 * Database initialization and management
 *
 * HBF Phase 0: Minimal hypermedia database
 * - Runtime hbf.db (./hbf.db or :memory:)
 * - WAL mode enabled for better concurrency
 * - Foreign keys enabled
 * - Global database handle for process-lifetime access
 */

/*
 * Initialize HBF database
 *
 * Creates or opens ./hbf.db (or in-memory if inmem=1).
 * Enables WAL mode and foreign keys.
 *
 * @param inmem: If 1, create in-memory database; if 0, use ./hbf.db
 * @param db: Output parameter for database handle
 * @return 0 on success, -1 on error
 */
int hbf_db_init(int inmem, sqlite3 **db);

/*
 * Close HBF database handle
 *
 * @param db: Database handle
 */
void hbf_db_close(sqlite3 *db);

/*
 * Get global database handle
 *
 * Returns the database handle set by hbf_db_init.
 * Used by hypermedia handlers to access the database.
 *
 * @return Database handle or NULL if not initialized
 */
sqlite3 *hbf_db_get(void);

#endif /* HBF_DB_H */
