-- HBF Database Schema v1
-- Phase 2a: Document-Graph + System Tables
--
-- This schema implements:
-- 1. Document-graph model (nodes, edges, tags) from DOCS/schema_doc_graph.md
-- 2. HBF system tables (_hbf_*) for users, sessions, permissions, etc.
-- 3. FTS5 full-text search on node content
-- 4. Default configuration values

-- =============================================================================
-- DOCUMENT-GRAPH CORE TABLES
-- =============================================================================

-- Nodes: atomic or composite entities (functions, modules, scenes, documents, etc.)
CREATE TABLE IF NOT EXISTS nodes (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    type TEXT NOT NULL,                    -- e.g., "function", "module", "scene", "document"
    body TEXT NOT NULL,                    -- JSON content and metadata
    name TEXT GENERATED ALWAYS AS (json_extract(body, '$.name')) VIRTUAL,
    version TEXT GENERATED ALWAYS AS (json_extract(body, '$.version')) VIRTUAL,
    created_at INTEGER NOT NULL DEFAULT (unixepoch()),
    modified_at INTEGER NOT NULL DEFAULT (unixepoch())
);

CREATE INDEX IF NOT EXISTS idx_nodes_type ON nodes(type);
CREATE INDEX IF NOT EXISTS idx_nodes_name ON nodes(name);
CREATE INDEX IF NOT EXISTS idx_nodes_created ON nodes(created_at);

-- Edges: directed relationships between nodes
CREATE TABLE IF NOT EXISTS edges (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    src INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,
    dst INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,
    rel TEXT NOT NULL,                     -- relationship type: "includes", "calls", "depends-on", "follows", "branch-to", etc.
    props TEXT,                            -- JSON properties: ordering, weights, conditions, etc.
    created_at INTEGER NOT NULL DEFAULT (unixepoch())
);

CREATE INDEX IF NOT EXISTS idx_edges_src ON edges(src);
CREATE INDEX IF NOT EXISTS idx_edges_dst ON edges(dst);
CREATE INDEX IF NOT EXISTS idx_edges_rel ON edges(rel);
CREATE INDEX IF NOT EXISTS idx_edges_src_rel ON edges(src, rel);

-- Tags: hierarchical labeling for composable documents
CREATE TABLE IF NOT EXISTS tags (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tag TEXT NOT NULL,                     -- tag identifier
    node_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,
    parent_tag_id INTEGER REFERENCES tags(id) ON DELETE CASCADE,
    order_num INTEGER NOT NULL DEFAULT 0,  -- ordering among siblings
    metadata TEXT,                         -- JSON metadata
    created_at INTEGER NOT NULL DEFAULT (unixepoch())
);

CREATE INDEX IF NOT EXISTS idx_tags_tag ON tags(tag);
CREATE INDEX IF NOT EXISTS idx_tags_node ON tags(node_id);
CREATE INDEX IF NOT EXISTS idx_tags_parent ON tags(parent_tag_id);
CREATE INDEX IF NOT EXISTS idx_tags_parent_order ON tags(parent_tag_id, order_num);

-- =============================================================================
-- HBF SYSTEM TABLES
-- =============================================================================

-- User accounts with Argon2id password hashes
CREATE TABLE IF NOT EXISTS _hbf_users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT NOT NULL UNIQUE,
    password_hash TEXT NOT NULL,           -- Argon2id PHC string format
    email TEXT,
    role TEXT NOT NULL DEFAULT 'user',     -- 'owner', 'admin', 'user'
    enabled INTEGER NOT NULL DEFAULT 1,
    created_at INTEGER NOT NULL DEFAULT (unixepoch()),
    modified_at INTEGER NOT NULL DEFAULT (unixepoch())
);

CREATE INDEX IF NOT EXISTS idx_hbf_users_username ON _hbf_users(username);
CREATE INDEX IF NOT EXISTS idx_hbf_users_role ON _hbf_users(role);

-- JWT session tracking with token hashes
CREATE TABLE IF NOT EXISTS _hbf_sessions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER NOT NULL REFERENCES _hbf_users(id) ON DELETE CASCADE,
    token_hash TEXT NOT NULL UNIQUE,       -- SHA-256 hash of JWT
    expires_at INTEGER NOT NULL,
    last_used INTEGER NOT NULL DEFAULT (unixepoch()),
    created_at INTEGER NOT NULL DEFAULT (unixepoch())
);

CREATE INDEX IF NOT EXISTS idx_hbf_sessions_user ON _hbf_sessions(user_id);
CREATE INDEX IF NOT EXISTS idx_hbf_sessions_token ON _hbf_sessions(token_hash);
CREATE INDEX IF NOT EXISTS idx_hbf_sessions_expires ON _hbf_sessions(expires_at);

-- Table-level access control
CREATE TABLE IF NOT EXISTS _hbf_table_permissions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER NOT NULL REFERENCES _hbf_users(id) ON DELETE CASCADE,
    table_name TEXT NOT NULL,
    can_select INTEGER NOT NULL DEFAULT 0,
    can_insert INTEGER NOT NULL DEFAULT 0,
    can_update INTEGER NOT NULL DEFAULT 0,
    can_delete INTEGER NOT NULL DEFAULT 0,
    created_at INTEGER NOT NULL DEFAULT (unixepoch())
);

CREATE UNIQUE INDEX IF NOT EXISTS idx_hbf_perms_user_table ON _hbf_table_permissions(user_id, table_name);

-- Row-level security policies (SQL conditions)
CREATE TABLE IF NOT EXISTS _hbf_row_policies (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    table_name TEXT NOT NULL,
    policy_name TEXT NOT NULL,
    role TEXT NOT NULL,                    -- 'owner', 'admin', 'user'
    operation TEXT NOT NULL,               -- 'SELECT', 'INSERT', 'UPDATE', 'DELETE'
    condition TEXT NOT NULL,               -- SQL WHERE clause (e.g., "user_id = $user_id")
    enabled INTEGER NOT NULL DEFAULT 1,
    created_at INTEGER NOT NULL DEFAULT (unixepoch())
);

CREATE INDEX IF NOT EXISTS idx_hbf_policies_table ON _hbf_row_policies(table_name);
CREATE UNIQUE INDEX IF NOT EXISTS idx_hbf_policies_name ON _hbf_row_policies(table_name, policy_name);

-- Per-henv configuration key-value store
CREATE TABLE IF NOT EXISTS _hbf_config (
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL,                   -- JSON value
    description TEXT,
    modified_at INTEGER NOT NULL DEFAULT (unixepoch())
);

-- Audit trail for admin actions
CREATE TABLE IF NOT EXISTS _hbf_audit_log (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER REFERENCES _hbf_users(id) ON DELETE SET NULL,
    action TEXT NOT NULL,                  -- 'CREATE_USER', 'GRANT_PERMISSION', etc.
    table_name TEXT,
    record_id INTEGER,
    details TEXT,                          -- JSON details
    created_at INTEGER NOT NULL DEFAULT (unixepoch())
);

CREATE INDEX IF NOT EXISTS idx_hbf_audit_user ON _hbf_audit_log(user_id);
CREATE INDEX IF NOT EXISTS idx_hbf_audit_created ON _hbf_audit_log(created_at);
CREATE INDEX IF NOT EXISTS idx_hbf_audit_action ON _hbf_audit_log(action);

-- Schema version tracking
CREATE TABLE IF NOT EXISTS _hbf_schema_version (
    version INTEGER PRIMARY KEY,
    applied_at INTEGER NOT NULL DEFAULT (unixepoch()),
    description TEXT
);

-- =============================================================================
-- FULL-TEXT SEARCH
-- =============================================================================

-- FTS5 virtual table for searching node content
CREATE VIRTUAL TABLE IF NOT EXISTS nodes_fts USING fts5(
    type UNINDEXED,
    content,
    content=nodes,
    content_rowid=id,
    tokenize='porter unicode61'
);

-- Triggers to keep FTS index synchronized with nodes table
CREATE TRIGGER IF NOT EXISTS nodes_fts_insert AFTER INSERT ON nodes BEGIN
    INSERT INTO nodes_fts(rowid, type, content)
    VALUES (new.id, new.type, json_extract(new.body, '$.content'));
END;

CREATE TRIGGER IF NOT EXISTS nodes_fts_update AFTER UPDATE ON nodes BEGIN
    INSERT INTO nodes_fts(nodes_fts, rowid, type, content)
    VALUES ('delete', old.id, old.type, json_extract(old.body, '$.content'));
    INSERT INTO nodes_fts(rowid, type, content)
    VALUES (new.id, new.type, json_extract(new.body, '$.content'));
END;

CREATE TRIGGER IF NOT EXISTS nodes_fts_delete AFTER DELETE ON nodes BEGIN
    INSERT INTO nodes_fts(nodes_fts, rowid, type, content)
    VALUES ('delete', old.id, old.type, json_extract(old.body, '$.content'));
END;

-- =============================================================================
-- DEFAULT CONFIGURATION VALUES
-- =============================================================================

-- Insert default config values
INSERT OR IGNORE INTO _hbf_config (key, value, description) VALUES
    ('qjs_mem_mb', '64', 'QuickJS memory limit in MB'),
    ('qjs_timeout_ms', '5000', 'QuickJS execution timeout in milliseconds'),
    ('session_lifetime_hours', '168', 'Session lifetime in hours (default: 7 days)'),
    ('max_upload_size_mb', '10', 'Maximum upload size in MB'),
    ('enable_audit_log', 'true', 'Enable audit logging for admin actions');

-- Insert schema version
INSERT OR IGNORE INTO _hbf_schema_version (version, description) VALUES
    (1, 'Initial schema: document-graph + HBF system tables');
