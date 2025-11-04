
// Minimal server.js for HBF integration test
app = {};

// Helper: Parse query string into object
function parseQuery(queryString) {
    const params = {};
    if (!queryString) return params;
    queryString.split('&').forEach(function(pair) {
        const parts = pair.split('=');
        if (parts[0]) {
            params[decodeURIComponent(parts[0])] = parts[1] ? decodeURIComponent(parts[1]) : '';
        }
    });
    return params;
}

app.handle = function (req, res) {
    // Middleware: add custom header
    res.set("X-Powered-By", "HBF");

    const { method, path } = req;
    const query = parseQuery(req.query);
    if (path === "/" && method === "GET") {
        // Serve index.html
        res.set("Content-Type", "text/html");
        res.send(`<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>HBF</title>
    <link rel="stylesheet" href="/style.css">
</head>
<body>
    <h1>HBF</h1>
    <p>Single binary web compute environment</p>
    <h2>Available Routes</h2>
    <ul>
        <li><a href="/health">/health</a> - Health check</li>
        <li><a href="/hello">/hello</a> - Hello world</li>
        <li><a href="/user/42">/user/42</a> - User endpoint</li>
        <li><a href="/echo">/echo</a> - Echo request</li>
        <li><a href="/__dev/">/__dev/</a> - Dev editor (dev mode only)</li>
    </ul>
    <h2>Static Assets</h2>
    <ul>
        <li><a href="/static/style.css">/static/style.css</a> - Local CSS file</li>
        <li><a href="/static/vendor/htmx.min.js">/static/vendor/htmx.min.js</a> - HTMX library</li>
        <li><a href="/static/monaco/vs/loader.js">/static/monaco/vs/loader.js</a> - Monaco editor loader</li>
        <li><a href="/static/monaco/vs/editor/editor.main.js">/static/monaco/vs/editor/editor.main.js</a> - Monaco editor main</li>
    </ul>
</body>
</html>`);
        return;
    }
    if (path === "/hello" && method === "GET") {
        res.set("Content-Type", "application/json");
        res.send(JSON.stringify({ message: "Hello, World!" }));
        return;
    }
    if (path.startsWith("/user/") && method === "GET") {
        const userId = path.split("/")[2];
        res.set("Content-Type", "application/json");
        res.send(JSON.stringify({ userId }));
        return;
    }
    if (path === "/echo" && method === "GET") {
        res.set("Content-Type", "application/json");
        res.send(JSON.stringify({ method, url: path }));
        return;
    }

    // Dev UI: Monaco editor interface
    if (path === "/__dev/" && method === "GET") {
        if (!req.dev) {
            res.status(403);
            res.set("Content-Type", "text/plain");
            res.send("Dev mode not enabled");
            return;
        }

        // Serve dev editor UI
        const result = db.query(
            "SELECT data AS content FROM latest_files WHERE path = ?",
            ["static/dev/index.html"]
        );

        if (result.length === 0) {
            res.status(404);
            res.set("Content-Type", "text/plain");
            res.send("Dev UI not found");
            return;
        }

        res.set("Content-Type", "text/html");
        res.send(result[0].content);
        return;
    }

    // Dev API: Read file from overlay
    if (path === "/__dev/api/file" && method === "GET") {
        // Check dev mode
        if (!req.dev) {
            res.status(403);
            res.set("Content-Type", "text/plain");
            res.send("Dev mode not enabled");
            return;
        }

        const fileName = query.name;
        if (!fileName || (!fileName.startsWith("static/") && !fileName.startsWith("hbf/"))) {
            res.status(400);
            res.set("Content-Type", "text/plain");
            res.send("Invalid file name (must start with static/ or hbf/)");
            return;
        }

        // Query latest_files view (versioned filesystem)
        const result = db.query(
            "SELECT data AS content FROM latest_files WHERE path = ?",
            [fileName]
        );

        if (result.length === 0) {
            res.status(404);
            res.set("Content-Type", "text/plain");
            res.send("File not found");
            return;
        }

        // Detect content type from extension
        const ext = fileName.split(".").pop();
        const contentTypes = {
            "html": "text/html",
            "css": "text/css",
            "js": "application/javascript",
            "json": "application/json",
            "md": "text/markdown",
            "txt": "text/plain"
        };
        const contentType = contentTypes[ext] || "text/plain";

        res.set("Content-Type", contentType);
        res.send(result[0].content);
        return;
    }

    // Dev API: Write file to overlay
    if (path === "/__dev/api/file" && method === "PUT") {
        if (!req.dev) {
            res.status(403);
            res.set("Content-Type", "text/plain");
            res.send("Dev mode not enabled");
            return;
        }

        const fileName = query.name;
        if (!fileName || (!fileName.startsWith("static/") && !fileName.startsWith("hbf/"))) {
            res.status(400);
            res.set("Content-Type", "text/plain");
            res.send("Invalid file name (must start with static/ or hbf/)");
            return;
        }

        const content = req.body;
        const mtime = Math.floor(Date.now() / 1000);

        // Write to versioned filesystem
        // 1. Get or create file_id
        db.execute(
            "INSERT OR IGNORE INTO file_ids (path) VALUES (?)",
            [fileName]
        );

        const fileIdResult = db.query(
            "SELECT file_id FROM file_ids WHERE path = ?",
            [fileName]
        );

        if (fileIdResult.length === 0) {
            res.status(500);
            res.send("Failed to get file_id");
            return;
        }

        const fileId = fileIdResult[0].file_id;

        // 2. Get next version number
        const versionResult = db.query(
            "SELECT COALESCE(MAX(version_number), 0) + 1 AS next_version FROM file_versions WHERE file_id = ?",
            [fileId]
        );

        const nextVersion = versionResult[0].next_version;

        // 3. Insert new version
        db.execute(
            "INSERT INTO file_versions (file_id, path, version_number, mtime, data) VALUES (?, ?, ?, ?, ?)",
            [fileId, fileName, nextVersion, mtime, content]
        );

        res.status(200);
        res.set("Content-Type", "text/plain");
        res.send("OK");
        return;
    }

    // Dev API: Delete file from overlay (tombstone)
    if (path === "/__dev/api/file" && method === "DELETE") {
        if (!req.dev) {
            res.status(403);
            res.set("Content-Type", "text/plain");
            res.send("Dev mode not enabled");
            return;
        }

        const fileName = query.name;
        if (!fileName) {
            res.status(400);
            res.set("Content-Type", "text/plain");
            res.send("Missing file name");
            return;
        }

        // Delete from versioned filesystem
        // 1. Get file_id
        const fileIdResult = db.query(
            "SELECT file_id FROM file_ids WHERE path = ?",
            [fileName]
        );

        if (fileIdResult.length === 0) {
            res.status(404);
            res.send("File not found");
            return;
        }

        const fileId = fileIdResult[0].file_id;

        // 2. Delete all versions
        db.execute(
            "DELETE FROM file_versions WHERE file_id = ?",
            [fileId]
        );

        // 3. Delete file_id entry
        db.execute(
            "DELETE FROM file_ids WHERE file_id = ?",
            [fileId]
        );

        res.status(200);
        res.set("Content-Type", "text/plain");
        res.send("OK");
        return;
    }

    // Dev API: List all files
    if (path === "/__dev/api/files" && method === "GET") {
        if (!req.dev) {
            res.status(403);
            res.set("Content-Type", "text/plain");
            res.send("Dev mode not enabled");
            return;
        }

        const files = db.query(
            "SELECT path AS name, mtime, size AS sz FROM latest_files_metadata ORDER BY path"
        );

        res.set("Content-Type", "application/json");
        res.send(JSON.stringify(files));
        return;
    }

    // Default: 404
    res.status(404);
    res.set("Content-Type", "text/plain");
    res.send("Not Found");
};
