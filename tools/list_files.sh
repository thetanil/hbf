#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# List latest files from an HBF overlay filesystem SQLite DB
# Usage:
#   tools/list_files.sh <path-to-db.sqlite> [--long]
#
# By default, prints tab-separated: path\tsize_bytes
# With --long, prints tab-separated: path\tsize\tversion\tmtime_iso8601

set -euo pipefail

print_usage() {
    echo "Usage: $0 <path-to-db.sqlite> [--long]" >&2
}

if ! command -v sqlite3 >/dev/null 2>&1; then
    echo "Error: sqlite3 CLI not found. Please install sqlite3." >&2
    exit 1
fi

if [ $# -lt 1 ] || [ $# -gt 2 ]; then
    print_usage
    exit 1
fi

DB_PATH="$1"
LONG_FLAG="0"
if [ "${2-}" = "--long" ]; then
    LONG_FLAG="1"
fi

if [ ! -f "$DB_PATH" ]; then
    echo "Error: Database file not found: $DB_PATH" >&2
    exit 1
fi

# Helper to check if a table or view exists
# $1: type ('table' or 'view')
# $2: name
exists_in_db() {
    local type="$1"; shift
    local name="$1"
    sqlite3 -readonly -noheader -batch "$DB_PATH" \
        "SELECT 1 FROM sqlite_master WHERE type='${type}' AND name='${name}' LIMIT 1;" \
        | grep -q 1
}

# Decide the best source for listing latest files, with sensible fallbacks
# Preference: materialized table -> view (metadata) -> view (with data) -> compute from file_versions -> sqlar
SQL_QUERY=""
if exists_in_db table latest_files_meta; then
    if [ "$LONG_FLAG" = "1" ]; then
        SQL_QUERY="SELECT path || char(9) || size || char(9) || version_number || char(9) || strftime('%Y-%m-%dT%H:%M:%SZ', mtime, 'unixepoch') FROM latest_files_meta ORDER BY path;"
    else
        SQL_QUERY="SELECT path || char(9) || size FROM latest_files_meta ORDER BY path;"
    fi
elif exists_in_db view latest_files_metadata; then
    if [ "$LONG_FLAG" = "1" ]; then
        SQL_QUERY="SELECT path || char(9) || size || char(9) || version_number || char(9) || strftime('%Y-%m-%dT%H:%M:%SZ', mtime, 'unixepoch') FROM latest_files_metadata ORDER BY path;"
    else
        SQL_QUERY="SELECT path || char(9) || size FROM latest_files_metadata ORDER BY path;"
    fi
elif exists_in_db view latest_files; then
    if [ "$LONG_FLAG" = "1" ]; then
        SQL_QUERY="SELECT path || char(9) || size || char(9) || version_number || char(9) || strftime('%Y-%m-%dT%H:%M:%SZ', mtime, 'unixepoch') FROM latest_files ORDER BY path;"
    else
        SQL_QUERY="SELECT path || char(9) || size FROM latest_files ORDER BY path;"
    fi
elif exists_in_db table file_versions; then
    if [ "$LONG_FLAG" = "1" ]; then
        SQL_QUERY="WITH lv AS (SELECT file_id, MAX(version_number) AS maxv FROM file_versions GROUP BY file_id) SELECT fv.path || char(9) || fv.size || char(9) || fv.version_number || char(9) || strftime('%Y-%m-%dT%H:%M:%SZ', fv.mtime, 'unixepoch') FROM file_versions fv JOIN lv ON fv.file_id = lv.file_id AND fv.version_number = lv.maxv ORDER BY fv.path;"
    else
        SQL_QUERY="WITH lv AS (SELECT file_id, MAX(version_number) AS maxv FROM file_versions GROUP BY file_id) SELECT fv.path || char(9) || fv.size FROM file_versions fv JOIN lv ON fv.file_id = lv.file_id AND fv.version_number = lv.maxv ORDER BY fv.path;"
    fi
elif exists_in_db table sqlar; then
    # Fallback for raw SQLAR archives (no overlay schema); mtime in SQLAR is seconds since epoch
    if [ "$LONG_FLAG" = "1" ]; then
        SQL_QUERY="SELECT name || char(9) || sz || char(9) || 1 || char(9) || strftime('%Y-%m-%dT%H:%M:%SZ', mtime, 'unixepoch') FROM sqlar ORDER BY name;"
    else
        SQL_QUERY="SELECT name || char(9) || sz FROM sqlar ORDER BY name;"
    fi
else
    echo "Error: Could not find any known overlay or SQLAR tables/views in: $DB_PATH" >&2
    exit 1
fi

# Execute the query
sqlite3 -readonly -noheader -batch "$DB_PATH" "$SQL_QUERY"
