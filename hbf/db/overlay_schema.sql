-- SPDX-License-Identifier: MIT
-- Overlay filesystem schema for dev mode live editing
-- Base sqlar table is never modified; all edits recorded in fs_layer

-- Append-only change log (all dev edits recorded here)
CREATE TABLE IF NOT EXISTS fs_layer (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    layer_id INT NOT NULL DEFAULT 1,  -- Future: support multiple dev layers
    name TEXT NOT NULL,
    op TEXT NOT NULL CHECK(op IN ('add', 'modify', 'delete')),
    mtime INT NOT NULL,               -- Unix timestamp of change
    sz INT NOT NULL,                  -- Uncompressed size
    data BLOB                         -- Uncompressed content (NULL for delete)
);

-- Index for fast lookups by name
CREATE INDEX IF NOT EXISTS idx_fs_layer_name ON fs_layer(name);

-- Materialized current overlay state (fast O(1) lookups)
CREATE TABLE IF NOT EXISTS overlay_sqlar (
    name TEXT PRIMARY KEY,
    mtime INT NOT NULL,
    sz INT NOT NULL,
    data BLOB,
    deleted INT NOT NULL DEFAULT 0  -- 1 = tombstone (hides base file)
);

-- Read-through view (overlay overrides base)
-- This view shows the current state: overlay files + base files not in overlay
CREATE VIEW IF NOT EXISTS latest_fs AS
SELECT name, 33188 AS mode, mtime, sz, data
FROM overlay_sqlar
WHERE deleted = 0
UNION ALL
SELECT name, mode, mtime, sz, data
FROM sqlar
WHERE name NOT IN (SELECT name FROM overlay_sqlar);

-- Trigger to auto-update overlay_sqlar when fs_layer changes
CREATE TRIGGER IF NOT EXISTS fs_layer_apply
AFTER INSERT ON fs_layer
BEGIN
    -- Handle delete operation (create tombstone)
    UPDATE overlay_sqlar SET deleted = 1, mtime = NEW.mtime, sz = 0, data = NULL
    WHERE NEW.op = 'delete' AND name = NEW.name;

    INSERT INTO overlay_sqlar (name, mtime, sz, data, deleted)
    SELECT NEW.name, NEW.mtime, 0, NULL, 1
    WHERE NEW.op = 'delete' AND (SELECT changes() = 0);

    -- Handle add/modify operations (upsert content)
    INSERT INTO overlay_sqlar (name, mtime, sz, data, deleted)
    SELECT NEW.name, NEW.mtime, NEW.sz, NEW.data, 0
    WHERE NEW.op IN ('add', 'modify')
    ON CONFLICT(name) DO UPDATE SET
        mtime = excluded.mtime,
        sz = excluded.sz,
        data = excluded.data,
        deleted = 0;
END;
