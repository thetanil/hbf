-- SPDX-License-Identifier: MIT
-- Versioned file system schema for overlay_fs
-- Each file write creates a new version; reads get the latest version

-- File versions table - stores all versions of all files
CREATE TABLE IF NOT EXISTS file_versions (
    file_id         INTEGER NOT NULL,    -- Unique ID for each file path
    path            TEXT NOT NULL,       -- Full file path
    version_number  INTEGER NOT NULL,    -- Version counter for this file_id
    mtime           INTEGER NOT NULL,    -- Unix timestamp
    size            INTEGER NOT NULL,    -- File size in bytes (pre-computed for performance)
    data            BLOB NOT NULL,       -- Uncompressed content
    PRIMARY KEY (file_id, version_number)
) WITHOUT ROWID;

-- Index for fast path lookups (path -> file_id)
CREATE INDEX IF NOT EXISTS idx_file_versions_path ON file_versions(path);

-- Index for getting latest version efficiently
CREATE INDEX IF NOT EXISTS idx_file_versions_file_id_version
    ON file_versions(file_id, version_number DESC);

-- Covering index to serve latest_files_metadata without touching BLOB pages
-- This allows SQLite to satisfy the SELECT (file_id, path, version_number, mtime, size)
-- from the index alone after the (file_id, version_number) join, avoiding table lookups
-- that would otherwise read the large 'data' payload.
CREATE INDEX IF NOT EXISTS idx_file_versions_latest_cover
    ON file_versions(file_id, version_number DESC, path, mtime, size);

-- File ID mapping table (path -> file_id)
CREATE TABLE IF NOT EXISTS file_ids (
    file_id  INTEGER PRIMARY KEY AUTOINCREMENT,
    path     TEXT NOT NULL UNIQUE
);

-- View to get latest version of each file
CREATE VIEW IF NOT EXISTS latest_files AS
WITH latest_versions AS (
    SELECT file_id, MAX(version_number) as max_version
    FROM file_versions
    GROUP BY file_id
)
SELECT
    fv.file_id,
    fv.path,
    fv.version_number,
    fv.mtime,
    fv.size,
    fv.data
FROM file_versions fv
INNER JOIN latest_versions lv
    ON fv.file_id = lv.file_id
    AND fv.version_number = lv.max_version;

-- View to get latest file metadata WITHOUT data blobs (fast for listings)
CREATE VIEW IF NOT EXISTS latest_files_metadata AS
WITH latest_versions AS (
    SELECT file_id, MAX(version_number) as max_version
    FROM file_versions
    GROUP BY file_id
)
SELECT
    fv.file_id,
    fv.path,
    fv.version_number,
    fv.mtime,
    fv.size
FROM file_versions fv
INNER JOIN latest_versions lv
    ON fv.file_id = lv.file_id
    AND fv.version_number = lv.max_version;

-- Materialized latest metadata for faster listings
CREATE TABLE IF NOT EXISTS latest_files_meta (
    file_id        INTEGER PRIMARY KEY,
    path           TEXT NOT NULL,
    version_number INTEGER NOT NULL,
    mtime          INTEGER NOT NULL,
    size           INTEGER NOT NULL
);

-- Index to support ORDER BY path quickly
CREATE INDEX IF NOT EXISTS idx_latest_files_meta_path ON latest_files_meta(path);

-- Bootstrap the materialized table from existing data
INSERT INTO latest_files_meta (file_id, path, version_number, mtime, size)
SELECT fv.file_id, fv.path, fv.version_number, fv.mtime, fv.size
FROM file_versions fv
JOIN (
    SELECT file_id, MAX(version_number) AS max_version
    FROM file_versions
    GROUP BY file_id
) lv ON fv.file_id = lv.file_id AND fv.version_number = lv.max_version
ON CONFLICT(file_id) DO UPDATE SET
    path = excluded.path,
    version_number = excluded.version_number,
    mtime = excluded.mtime,
    size = excluded.size
    WHERE excluded.version_number >= latest_files_meta.version_number;

-- Keep materialized latest in sync on insert
CREATE TRIGGER IF NOT EXISTS trg_file_versions_ai
AFTER INSERT ON file_versions
BEGIN
    INSERT INTO latest_files_meta (file_id, path, version_number, mtime, size)
    VALUES (NEW.file_id, NEW.path, NEW.version_number, NEW.mtime, NEW.size)
    ON CONFLICT(file_id) DO UPDATE SET
        path = excluded.path,
        version_number = excluded.version_number,
        mtime = excluded.mtime,
        size = excluded.size
        WHERE excluded.version_number >= latest_files_meta.version_number;
END;

-- Keep materialized latest in sync on delete
CREATE TRIGGER IF NOT EXISTS trg_file_versions_ad
AFTER DELETE ON file_versions
BEGIN
    -- If there is still a version, set to max; otherwise delete the meta row
    INSERT INTO latest_files_meta (file_id, path, version_number, mtime, size)
    SELECT fv.file_id, fv.path, fv.version_number, fv.mtime, fv.size
    FROM file_versions fv
    WHERE fv.file_id = OLD.file_id
    ORDER BY fv.version_number DESC
    LIMIT 1
    ON CONFLICT(file_id) DO UPDATE SET
        path = excluded.path,
        version_number = excluded.version_number,
        mtime = excluded.mtime,
        size = excluded.size;

    DELETE FROM latest_files_meta
    WHERE file_id = OLD.file_id
      AND NOT EXISTS (SELECT 1 FROM file_versions WHERE file_id = OLD.file_id);
END;

-- Asset bundle migrations tracking table
-- Tracks which asset bundles have been applied (idempotency)
CREATE TABLE IF NOT EXISTS migrations (
    bundle_id   TEXT PRIMARY KEY,   -- SHA256 hash of compressed bundle
    applied_at  INTEGER NOT NULL    -- Unix timestamp when migration was applied
);
