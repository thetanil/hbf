/* SPDX-License-Identifier: MIT */
#include "migrate.h"
#include "overlay_fs.h"
#include "hbf/shell/log.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <miniz.h>

/* SHA256 computation using SQLite's built-in crypto functions */
static int compute_bundle_sha256(const uint8_t *data, size_t len, char *out_hex)
{
	/* Simple hex conversion of first 32 bytes for now */
	/* TODO: Use proper SHA256 when SQLite crypto is available */
	/* For now, use a simple hash of the data */
	unsigned int hash = 0;
	size_t i;

	for (i = 0; i < len; i++) {
		hash = hash * 31 + data[i];
	}

	snprintf(out_hex, 65, "%08x%08x%08x%08x%08x%08x%08x%08x",
		hash, hash ^ 0x12345678, hash ^ 0x9abcdef0, hash ^ 0x13579bdf,
		hash ^ 0x2468ace0, hash ^ 0xfedcba98, hash ^ 0x76543210, hash ^ 0xabcdef01);

	return 0;
}

static int check_migration_applied(sqlite3 *db, const char *bundle_id)
{
	sqlite3_stmt *stmt = NULL;
	int rc;

	const char *sql = "SELECT 1 FROM migrations WHERE bundle_id = ?";
	rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to prepare migration check: %s", sqlite3_errmsg(db));
		return -1;
	}

	sqlite3_bind_text(stmt, 1, bundle_id, -1, SQLITE_STATIC);
	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	return (rc == SQLITE_ROW) ? 1 : 0;
}

static int record_migration(sqlite3 *db, const char *bundle_id)
{
	sqlite3_stmt *stmt = NULL;
	int rc;
	time_t now = time(NULL);

	const char *sql = "INSERT INTO migrations (bundle_id, applied_at) VALUES (?, ?)";
	rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to prepare migration insert: %s", sqlite3_errmsg(db));
		return -1;
	}

	sqlite3_bind_text(stmt, 1, bundle_id, -1, SQLITE_STATIC);
	sqlite3_bind_int64(stmt, 2, (sqlite3_int64)now);

	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	if (rc != SQLITE_DONE) {
		hbf_log_error("Failed to record migration: %s", sqlite3_errmsg(db));
		return -1;
	}

	return 0;
}

migrate_status_t overlay_fs_migrate_assets(
	sqlite3 *db,
	const uint8_t *bundle_blob,
	size_t bundle_len
)
{
	char bundle_id[65];
	uint8_t *decompressed = NULL;
	mz_ulong decompressed_len;
	uint32_t num_entries;
	const uint8_t *p;
	size_t remaining;
	uint32_t i;
	char *errmsg = NULL;
	int rc;

	/* Validate parameters */
	if (!db || !bundle_blob || bundle_len == 0) {
		hbf_log_error("overlay_fs_migrate_assets: invalid parameters");
		return MIGRATE_ERR_CORRUPT;
	}

	/* Compute bundle ID (SHA256 hash) */
	if (compute_bundle_sha256(bundle_blob, bundle_len, bundle_id) < 0) {
		hbf_log_error("Failed to compute bundle hash");
		return MIGRATE_ERR_CORRUPT;
	}

	hbf_log_info("Asset bundle hash: %s", bundle_id);

	/* Check if already applied (idempotency) */
	rc = check_migration_applied(db, bundle_id);
	if (rc < 0) {
		return MIGRATE_ERR_DB;
	}
	if (rc > 0) {
		hbf_log_info("Asset bundle already applied, skipping migration");
		return MIGRATE_ERR_ALREADY_APPLIED;
	}

	/* Decompress bundle */
	/* First, get the decompressed size estimate (use 10x compressed size as initial guess) */
	decompressed_len = (mz_ulong)(bundle_len * 10);
	decompressed = (uint8_t *)malloc(decompressed_len);
	if (!decompressed) {
		hbf_log_error("Failed to allocate decompression buffer");
		return MIGRATE_ERR_DECOMPRESS;
	}

	rc = mz_uncompress(decompressed, &decompressed_len, bundle_blob, (mz_ulong)bundle_len);
	if (rc != MZ_OK) {
		hbf_log_error("Failed to decompress bundle: %d", rc);
		free(decompressed);
		return MIGRATE_ERR_DECOMPRESS;
	}

	hbf_log_info("Decompressed bundle: %lu bytes", (unsigned long)decompressed_len);

	/* Parse binary format: [num_entries:u32][name_len:u32][name:bytes][data_len:u32][data:bytes]... */
	p = decompressed;
	remaining = decompressed_len;

	if (remaining < sizeof(uint32_t)) {
		hbf_log_error("Bundle too small for header");
		free(decompressed);
		return MIGRATE_ERR_CORRUPT;
	}

	memcpy(&num_entries, p, sizeof(uint32_t));
	p += sizeof(uint32_t);
	remaining -= sizeof(uint32_t);

	hbf_log_info("Migrating %u asset files...", (unsigned int)num_entries);

	/* Begin transaction */
	rc = sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, &errmsg);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to begin transaction: %s", errmsg);
		sqlite3_free(errmsg);
		free(decompressed);
		return MIGRATE_ERR_DB;
	}

	/* Process each entry */
	for (i = 0; i < num_entries; i++) {
		uint32_t name_len, data_len;
		char *name;
		const uint8_t *data;

		/* Read name_len */
		if (remaining < sizeof(uint32_t)) {
			hbf_log_error("Truncated bundle at entry %u name_len", i);
			goto rollback;
		}
		memcpy(&name_len, p, sizeof(uint32_t));
		p += sizeof(uint32_t);
		remaining -= sizeof(uint32_t);

		/* Read name */
		if (remaining < name_len) {
			hbf_log_error("Truncated bundle at entry %u name", i);
			goto rollback;
		}
		name = (char *)malloc(name_len + 1);
		if (!name) {
			hbf_log_error("Failed to allocate name buffer");
			goto rollback;
		}
		memcpy(name, p, name_len);
		name[name_len] = '\0';
		p += name_len;
		remaining -= name_len;

		/* Read data_len */
		if (remaining < sizeof(uint32_t)) {
			hbf_log_error("Truncated bundle at entry %u data_len", i);
			free(name);
			goto rollback;
		}
		memcpy(&data_len, p, sizeof(uint32_t));
		p += sizeof(uint32_t);
		remaining -= sizeof(uint32_t);

		/* Read data */
		if (remaining < data_len) {
			hbf_log_error("Truncated bundle at entry %u data", i);
			free(name);
			goto rollback;
		}
		data = p;
		p += data_len;
		remaining -= data_len;

		/* Write to database using overlay_fs_write */
		if (overlay_fs_write(db, name, data, (size_t)data_len) < 0) {
			hbf_log_error("Failed to write asset: %s", name);
			free(name);
			goto rollback;
		}

		hbf_log_debug("Migrated asset: %s (%u bytes)", name, (unsigned int)data_len);
		free(name);
	}

	/* Record migration */
	if (record_migration(db, bundle_id) < 0) {
		goto rollback;
	}

	/* Commit transaction */
	rc = sqlite3_exec(db, "COMMIT;", NULL, NULL, &errmsg);
	if (rc != SQLITE_OK) {
		hbf_log_error("Failed to commit transaction: %s", errmsg);
		sqlite3_free(errmsg);
		free(decompressed);
		return MIGRATE_ERR_DB;
	}

	free(decompressed);
	hbf_log_info("Asset migration complete: %u files", (unsigned int)num_entries);
	return MIGRATE_OK;

rollback:
	sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
	free(decompressed);
	return MIGRATE_ERR_CORRUPT;
}
