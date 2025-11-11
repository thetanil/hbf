/* SPDX-License-Identifier: MIT */
#ifndef HBF_MIGRATE_H
#define HBF_MIGRATE_H

#include <sqlite3.h>
#include <stdint.h>
#include <stddef.h>

/*
 * Asset migration for HBF
 *
 * Loads compressed asset bundles into SQLite at startup.
 * Idempotent via SHA256-based bundle tracking.
 */

typedef enum {
	MIGRATE_OK = 0,
	MIGRATE_ERR_DECOMPRESS = -1,
	MIGRATE_ERR_DB = -2,
	MIGRATE_ERR_ALREADY_APPLIED = -3,  /* Not an error; indicates no-op */
	MIGRATE_ERR_CORRUPT = -4
} migrate_status_t;

/*
 * Migrate embedded asset bundle into SQLite DB
 *
 * Idempotent: skips if bundle_id (SHA256 hash) already applied.
 *
 * Algorithm:
 *   1. Compute bundle_id = SHA256(bundle_blob)
 *   2. Check migrations table; skip if already applied
 *   3. Decompress bundle using miniz
 *   4. Parse binary format: [num_entries:u32][name_len:u32][name:bytes][data_len:u32][data:bytes]...
 *   5. BEGIN TRANSACTION
 *   6. For each entry: INSERT OR REPLACE INTO overlay (path, content, mtime)
 *   7. INSERT INTO migrations (bundle_id, applied_at)
 *   8. COMMIT (or ROLLBACK on error)
 *
 * @param db Open SQLite connection
 * @param bundle_blob Embedded compressed asset bundle
 * @param bundle_len Size of bundle in bytes
 * @return migrate_status_t status code
 */
migrate_status_t overlay_fs_migrate_assets(
	sqlite3 *db,
	const uint8_t *bundle_blob,
	size_t bundle_len
);

#endif /* HBF_MIGRATE_H */
