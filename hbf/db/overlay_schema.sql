-- SPDX-License-Identifier: MIT
-- Versioned file system schema for overlay_fs
-- Each file write creates a new version; reads get the latest version

-- File versions table - stores all versions of all files
CREATE TABLE IF NOT EXISTS file_versions (
    file_id         INTEGER NOT NULL,    -- Unique ID for each file path
    path            TEXT NOT NULL,       -- Full file path
    version_number  INTEGER NOT NULL,    -- Version counter for this file_id
    mtime           INTEGER NOT NULL,    -- Unix timestamp
    data            BLOB NOT NULL,       -- Uncompressed content
    PRIMARY KEY (file_id, version_number)
) WITHOUT ROWID;

-- Index for fast path lookups (path -> file_id)
CREATE INDEX IF NOT EXISTS idx_file_versions_path ON file_versions(path);

-- Index for getting latest version efficiently
CREATE INDEX IF NOT EXISTS idx_file_versions_file_id_version
    ON file_versions(file_id, version_number DESC);

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
    fv.data
FROM file_versions fv
INNER JOIN latest_versions lv
    ON fv.file_id = lv.file_id
    AND fv.version_number = lv.max_version;
