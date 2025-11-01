# Phase 2: Dev Overlay Filesystem and Live Edit API

**Status**: Planning
**Prerequisites**: Phase 0 & 1 complete (static assets packaged, HTMX & Monaco served)

## Architecture: Layered Filesystem in Main DB

### Core Principle: Single Database, Multiple Layers

The main DB (`hbf.db`) contains:
1. **Layer 0 (base)**: `sqlar` table - immutable runtime baseline from embedded `fs.db`
2. **Layer 1 (dev overlay)**: `fs_layer` table - append-only change log for dev edits
3. **Materialized view**: `overlay_sqlar` table - current state (maintained by triggers)
4. **Read abstraction**: `latest_fs` view - union of overlay + base

**Key decisions**:
- ✅ No separate dev database
- ✅ Overlay lives in same DB as base `sqlar`
- ✅ Base `sqlar` never modified at runtime (recovery fallback)
- ✅ Dev mode toggle controls which view C code/JS queries

## Database Schema

### Layer Tables

```sql
-- Base layer (hydrated from fs.db at startup, never written to)
-- Already exists from SQLAR extension
CREATE TABLE IF NOT EXISTS sqlar (
    name TEXT PRIMARY KEY,
    mode INT,
    mtime INT,
    sz INT,
    data BLOB
);

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

-- Materialized current overlay state (fast O(1) lookups)
CREATE TABLE IF NOT EXISTS overlay_sqlar (
    name TEXT PRIMARY KEY,
    mtime INT NOT NULL,
    sz INT NOT NULL,
    data BLOB,
    deleted INT NOT NULL DEFAULT 0  -- 1 = tombstone (hides base file)
);

-- Read-through view (overlay overrides base)
CREATE VIEW IF NOT EXISTS latest_fs AS
SELECT name, mode, mtime, sz, data
FROM overlay_sqlar
WHERE deleted = 0
UNION ALL
SELECT name, mode, mtime, sz, data
FROM sqlar
WHERE name NOT IN (SELECT name FROM overlay_sqlar);
```

### Triggers to Maintain overlay_sqlar

```sql
-- Auto-update overlay_sqlar when fs_layer changes are inserted
CREATE TRIGGER IF NOT EXISTS fs_layer_apply
AFTER INSERT ON fs_layer
BEGIN
    -- Handle delete operation
    CASE NEW.op
        WHEN 'delete' THEN
            INSERT INTO overlay_sqlar (name, mtime, sz, data, deleted)
            VALUES (NEW.name, NEW.mtime, 0, NULL, 1)
            ON CONFLICT(name) DO UPDATE SET
                mtime = excluded.mtime,
                sz = 0,
                data = NULL,
                deleted = 1;
        -- Handle add/modify operations
        ELSE
            INSERT INTO overlay_sqlar (name, mtime, sz, data, deleted)
            VALUES (NEW.name, NEW.mtime, NEW.sz, NEW.data, 0)
            ON CONFLICT(name) DO UPDATE SET
                mtime = excluded.mtime,
                sz = excluded.sz,
                data = excluded.data,
                deleted = 0;
    END;
END;
```

### Schema Initialization

Add to `internal/db/schema.sql` (or create `internal/db/overlay_schema.sql`):
- Include above `CREATE TABLE` and `CREATE TRIGGER` statements
- Initialize on first run via `hbf_db_init()` after main DB opens

## C API Changes

### Read Path: `hbf_db_read_file()`

Update `internal/db/db.c` to add dev-aware read function:

```c
/* New function: Read file with overlay support */
int hbf_db_read_file(sqlite3 *db, const char *path, int use_overlay,
                     unsigned char **data, size_t *size)
{
    sqlite3_stmt *stmt;
    const char *sql;
    int rc;
    const void *blob_data;
    int blob_size;

    if (!db || !path || !data || !size) {
        hbf_log_error("NULL parameter in hbf_db_read_file");
        return -1;
    }

    *data = NULL;
    *size = 0;

    /* Choose table based on dev mode */
    if (use_overlay) {
        /* Query latest_fs view (overlay + base) */
        sql = "SELECT sqlar_uncompress(data, sz) FROM latest_fs WHERE name = ?";
    } else {
        /* Query base sqlar only (production mode) */
        sql = "SELECT sqlar_uncompress(data, sz) FROM sqlar WHERE name = ?";
    }

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        hbf_log_error("Failed to prepare file read: %s", sqlite3_errmsg(db));
        return -1;
    }

    rc = sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        hbf_log_error("Failed to bind path: %s", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return -1;
    }

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        /* File not found */
        sqlite3_finalize(stmt);
        return -1;
    }

    blob_data = sqlite3_column_blob(stmt, 0);
    blob_size = sqlite3_column_bytes(stmt, 0);

    if (blob_size > 0 && blob_data) {
        *data = malloc((size_t)blob_size);
        if (*data) {
            memcpy(*data, blob_data, (size_t)blob_size);
            *size = (size_t)blob_size;
        }
    }

    sqlite3_finalize(stmt);

    if (!*data && blob_size > 0) {
        hbf_log_error("Failed to allocate memory for file");
        return -1;
    }

    return 0;
}
```

### Update Existing Callers

**Static handler** (`internal/http/server.c`):
```c
/* Line ~79: Replace hbf_db_read_file_from_main() call */
ret = hbf_db_read_file(server->db, path, server->dev, &data, &size);
```

**QuickJS handler** (`internal/http/handler.c`):
```c
/* Line ~59: Load server.js with overlay support */
ret = hbf_db_read_file(server->db, "hbf/server.js", server->dev, &js_data, &js_size);
```

**Why this works**:
- Prod mode (`dev=0`): Queries base `sqlar` only (fast, predictable)
- Dev mode (`dev=1`): Queries `latest_fs` view (overlay overrides base)
- Single boolean controls entire read path behavior

## Dev API Endpoints (JavaScript in server.js)

### GET /__dev/api/file?name=<path>

Read current file content (from overlay if present, else base):

```javascript
if (path === '/__dev/api/file' && method === 'GET') {
    // Validate dev mode (check req.headers['x-hbf-dev'] === '1')
    if (!req.headers['x-hbf-dev']) {
        res.status(403).send('Dev mode not enabled');
        return;
    }

    const fileName = req.query.name;
    if (!fileName || !fileName.startsWith('static/') && !fileName.startsWith('hbf/')) {
        res.status(400).send('Invalid file name');
        return;
    }

    // Query latest_fs view (overlay-aware)
    const result = db.query(
        'SELECT sqlar_uncompress(data, sz) AS content FROM latest_fs WHERE name = ?',
        [fileName]
    );

    if (result.length === 0) {
        res.status(404).send('File not found');
        return;
    }

    // Detect content type from extension
    const ext = fileName.split('.').pop();
    const contentType = {
        'html': 'text/html',
        'css': 'text/css',
        'js': 'application/javascript',
        'json': 'application/json',
        'md': 'text/markdown'
    }[ext] || 'text/plain';

    res.set('Content-Type', contentType);
    res.send(result[0].content);
    return;
}
```

### PUT /__dev/api/file?name=<path>

Write file to overlay (appends to `fs_layer`, trigger updates `overlay_sqlar`):

```javascript
if (path === '/__dev/api/file' && method === 'PUT') {
    if (!req.headers['x-hbf-dev']) {
        res.status(403).send('Dev mode not enabled');
        return;
    }

    const fileName = req.query.name;
    if (!fileName || !fileName.startsWith('static/') && !fileName.startsWith('hbf/')) {
        res.status(400).send('Invalid file name');
        return;
    }

    const content = req.body;  // Assume raw body as string/buffer
    const sz = content.length;
    const mtime = Math.floor(Date.now() / 1000);  // Unix timestamp

    // Insert into fs_layer (trigger auto-updates overlay_sqlar)
    db.execute(
        `INSERT INTO fs_layer (layer_id, name, op, mtime, sz, data)
         VALUES (1, ?, 'modify', ?, ?, ?)`,
        [fileName, mtime, sz, content]
    );

    res.status(200).send('OK');
    return;
}
```

### DELETE /__dev/api/file?name=<path>

Tombstone file in overlay:

```javascript
if (path === '/__dev/api/file' && method === 'DELETE') {
    if (!req.headers['x-hbf-dev']) {
        res.status(403).send('Dev mode not enabled');
        return;
    }

    const fileName = req.query.name;
    const mtime = Math.floor(Date.now() / 1000);

    db.execute(
        `INSERT INTO fs_layer (layer_id, name, op, mtime, sz, data)
         VALUES (1, ?, 'delete', ?, 0, NULL)`,
        [fileName, mtime]
    );

    res.status(200).send('OK');
    return;
}
```

### GET /__dev/api/files

List all files (base + overlay, excluding deleted):

```javascript
if (path === '/__dev/api/files' && method === 'GET') {
    if (!req.headers['x-hbf-dev']) {
        res.status(403).send('Dev mode not enabled');
        return;
    }

    const files = db.query(`
        SELECT name, mtime, sz
        FROM latest_fs
        ORDER BY name
    `);

    res.set('Content-Type', 'application/json');
    res.send(JSON.stringify(files));
    return;
}
```

## Dev Header Injection (C Code)

Update `internal/http/server.c` static handler to inject `X-HBF-Dev` header:

```c
/* After line 90 (before sending response headers) */
const char *dev_header = server->dev ? "X-HBF-Dev: 1\r\n" : "";

mg_printf(conn,
          "HTTP/1.1 200 OK\r\n"
          "Content-Type: %s\r\n"
          "Content-Length: %zu\r\n"
          "%s"        /* Cache-Control */
          "%s"        /* X-HBF-Dev (if dev mode) */
          "Connection: close\r\n"
          "\r\n",
          mime_type, size, cache_header, dev_header);
```

**Why**: JavaScript in `server.js` can check `req.headers['x-hbf-dev']` to gate dev endpoints.

## Minimal Dev UI (Phase 3 Preview)

Create `fs/static/dev/index.html`:

```html
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>HBF Dev Editor</title>
    <style>
        body { margin: 0; display: flex; flex-direction: column; height: 100vh; }
        #toolbar { padding: 10px; background: #2d2d2d; color: #fff; }
        #container { display: flex; flex: 1; }
        #editor { width: 50%; height: 100%; }
        #preview { width: 50%; height: 100%; border-left: 1px solid #ccc; }
    </style>
    <script src="/static/monaco/vs/loader.js"></script>
</head>
<body>
    <div id="toolbar">
        <input type="text" id="fileName" value="static/index.html" />
        <button onclick="loadFile()">Load</button>
        <button onclick="saveFile()">Save</button>
        <span id="status"></span>
    </div>
    <div id="container">
        <div id="editor"></div>
        <iframe id="preview" src="/"></iframe>
    </div>
    <script>
        let editor;
        require.config({ paths: { 'vs': '/static/monaco/vs' } });
        require(['vs/editor/editor.main'], function () {
            editor = monaco.editor.create(document.getElementById('editor'), {
                value: '',
                language: 'html',
                theme: 'vs-dark'
            });
        });

        async function loadFile() {
            const name = document.getElementById('fileName').value;
            const res = await fetch(`/__dev/api/file?name=${encodeURIComponent(name)}`);
            if (res.ok) {
                const content = await res.text();
                editor.setValue(content);
                document.getElementById('status').textContent = 'Loaded: ' + name;
            } else {
                alert('Failed to load file: ' + res.statusText);
            }
        }

        async function saveFile() {
            const name = document.getElementById('fileName').value;
            const content = editor.getValue();
            const res = await fetch(`/__dev/api/file?name=${encodeURIComponent(name)}`, {
                method: 'PUT',
                body: content
            });
            if (res.ok) {
                document.getElementById('status').textContent = 'Saved: ' + name;
                document.getElementById('preview').contentWindow.location.reload();
            } else {
                alert('Failed to save file: ' + res.statusText);
            }
        }

        // Auto-load on startup
        window.addEventListener('load', () => setTimeout(loadFile, 500));
    </script>
</body>
</html>
```

## Implementation Tasks

### Phase 2.1: Schema and Triggers (C)
- [ ] Create `internal/db/overlay_schema.sql` with tables and triggers
- [ ] Update `hbf_db_init()` to apply overlay schema after main DB opens
- [ ] Add `hbf_db_read_file()` function with `use_overlay` parameter
- [ ] Update static handler to use new read function with `server->dev`
- [ ] Update QuickJS handler to use new read function with `server->dev`
- [ ] Inject `X-HBF-Dev` header in static handler when `server->dev == 1`

### Phase 2.2: Dev API (JavaScript)
- [ ] Add `GET /__dev/api/file` endpoint to `fs/hbf/server.js`
- [ ] Add `PUT /__dev/api/file` endpoint
- [ ] Add `DELETE /__dev/api/file` endpoint
- [ ] Add `GET /__dev/api/files` list endpoint
- [ ] Validate all dev endpoints check `req.headers['x-hbf-dev']`

### Phase 2.3: Dev UI (HTML + Monaco)
- [ ] Create `fs/static/dev/index.html` with Monaco editor
- [ ] Implement load/save buttons with fetch API
- [ ] Add preview iframe with auto-reload on save
- [ ] Test edit → save → preview workflow

### Phase 2.4: Testing
- [ ] Unit test: Overlay trigger updates `overlay_sqlar` correctly
- [ ] Integration test: PUT file, verify GET returns new content
- [ ] Integration test: DELETE file, verify GET returns 404
- [ ] Integration test: Prod mode reads base `sqlar`, ignores overlay
- [ ] Integration test: Dev mode reads `latest_fs` with overlay applied

## Acceptance Criteria

```bash
# Build and start in dev mode
bazel build //:hbf
rm -f hbf.db && bazel-bin/hbf --dev &

# Verify overlay schema created
sqlite3 hbf.db "SELECT name FROM sqlite_master WHERE type='table' AND name LIKE 'fs_%' OR name = 'overlay_sqlar'"
# Should output: fs_layer, overlay_sqlar

# Test dev API: Save file
curl -X PUT 'http://localhost:5309/__dev/api/file?name=static/test.html' \
  -d '<h1>Test</h1>'
# Should return: OK

# Test dev API: Read file
curl 'http://localhost:5309/__dev/api/file?name=static/test.html'
# Should return: <h1>Test</h1>

# Test overlay serving
curl http://localhost:5309/static/test.html
# Should return: <h1>Test</h1>

# Verify base sqlar unchanged
sqlite3 hbf.db "SELECT name FROM sqlar WHERE name = 'static/test.html'"
# Should return: (empty - base not modified)

# Verify overlay recorded change
sqlite3 hbf.db "SELECT name, op FROM fs_layer WHERE name = 'static/test.html'"
# Should output: static/test.html|modify

# Test prod mode fallback
pkill hbf
bazel-bin/hbf &  # Start without --dev
curl http://localhost:5309/static/test.html
# Should return: 404 (overlay ignored, base has no test.html)

pkill hbf
```

## Out of Scope (Future Enhancements)

- SSE live reload (Phase 3)
- Multi-layer support (layer_id > 1)
- Overlay compaction/promotion to base
- Conflict resolution UI
- Undo/redo via fs_layer history
- Monaco Vim mode
- Syntax validation
- File tree navigation UI

## Risk Mitigation

**Risk**: Overlay gets corrupted, dev environment unusable
- **Mitigation**: Add `/__dev/api/reset` endpoint to clear overlay tables
- **Recovery**: Restart with `--inmem` or delete `hbf.db` to rebuild from embedded `fs.db`

**Risk**: Base `sqlar` accidentally written to in dev mode
- **Mitigation**: All writes go through `fs_layer` INSERT (triggers handle `overlay_sqlar`)
- **Verification**: Add assertion in C code to detect direct `sqlar` writes

**Risk**: Large files in overlay bloat DB size
- **Mitigation**: Phase 3 adds size limits and warnings in dev UI
- **Future**: Implement overlay compaction to merge changes back into base

## Notes

### Why Append-Only `fs_layer`?
- Preserves edit history (useful for undo/debugging)
- Audit trail of all dev changes
- Future: Time-travel debugging ("show me file state at 3pm yesterday")
- Window functions can query historical states if needed

### Why Materialized `overlay_sqlar`?
- O(1) lookup performance (indexed PRIMARY KEY)
- Avoids expensive window function queries on every file read
- Triggers keep it synchronized automatically
- Trade-off: Extra storage (acceptable - dev DB is transient)

### Why `latest_fs` View?
- Clean abstraction for "current state" logic
- C code can query one view instead of complex UNION logic
- Easier to extend later (e.g., add Layer 2 for staging)

### Compression Handling
- `sqlar` stores compressed data (zlib)
- `fs_layer` stores **uncompressed** data (simplicity)
- `sqlar_uncompress(data, sz)` handles both:
  - If `sz == length(data)`: return `data` as-is (uncompressed)
  - If `sz > length(data)`: decompress `data` and return
- Overlay reads are slightly slower (no compression) but dev UX prioritizes simplicity
