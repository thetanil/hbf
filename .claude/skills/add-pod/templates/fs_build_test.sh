#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Test that the pod database was created correctly

set -euo pipefail

DB_FILE="$1"

# Check that the database file exists
if [[ ! -f "$DB_FILE" ]]; then
    echo "FAIL: Database file does not exist: $DB_FILE"
    exit 1
fi

# Check that the database is a valid SQLite file
if ! sqlite3 "$DB_FILE" "PRAGMA integrity_check;" > /dev/null; then
    echo "FAIL: Database integrity check failed"
    exit 1
fi

# Check that the sqlar table exists
if ! sqlite3 "$DB_FILE" "SELECT name FROM sqlite_master WHERE type='table' AND name='sqlar';" | grep -q sqlar; then
    echo "FAIL: sqlar table does not exist"
    exit 1
fi

# Check that server.js exists in the archive
if ! sqlite3 "$DB_FILE" "SELECT name FROM sqlar WHERE name='hbf/server.js';" | grep -q "hbf/server.js"; then
    echo "FAIL: hbf/server.js not found in sqlar archive"
    exit 1
fi

# Check that overlay schema was applied (fs_layer table exists)
if ! sqlite3 "$DB_FILE" "SELECT name FROM sqlite_master WHERE type='table' AND name='fs_layer';" | grep -q fs_layer; then
    echo "FAIL: fs_layer table does not exist (overlay schema not applied)"
    exit 1
fi

echo "PASS: Pod database built correctly"
exit 0
