/* SPDX-License-Identifier: MIT */
#include "migrate.h"
#include "hbf/shell/log.h"
#include <string.h>

/*
 * Stub implementation of asset migration
 *
 * TODO: Implement full migration logic:
 *   - SHA256 hash computation
 *   - Idempotency check via migrations table
 *   - miniz decompression
 *   - Binary format parsing
 *   - SQLite transaction + insert
 */

migrate_status_t overlay_fs_migrate_assets(
	sqlite3 *db,
	const uint8_t *bundle_blob,
	size_t bundle_len
)
{
	/* Stub: validate parameters */
	if (!db || !bundle_blob || bundle_len == 0) {
		hbf_log_error("overlay_fs_migrate_assets: invalid parameters");
		return MIGRATE_ERR_CORRUPT;
	}

	/* Stub: no-op for now */
	hbf_log_info("overlay_fs_migrate_assets: stub called (no-op)");
	(void)db;
	(void)bundle_blob;
	(void)bundle_len;

	return MIGRATE_OK;
}
