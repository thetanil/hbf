
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

        // Query latest_fs view (overlay-aware)
        const result = db.query(
            "SELECT sqlar_uncompress(data, sz) AS content FROM latest_fs WHERE name = ?",
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
        const sz = content.length;
        const mtime = Math.floor(Date.now() / 1000);

        // Insert into fs_layer (trigger auto-updates overlay_sqlar)
        db.execute(
            "INSERT INTO fs_layer (layer_id, name, op, mtime, sz, data) VALUES (1, ?, 'modify', ?, ?, ?)",
            [fileName, mtime, sz, content]
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

        const mtime = Math.floor(Date.now() / 1000);

        db.execute(
            "INSERT INTO fs_layer (layer_id, name, op, mtime, sz, data) VALUES (1, ?, 'delete', ?, 0, NULL)",
            [fileName, mtime]
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
            "SELECT name, mtime, sz FROM latest_fs ORDER BY name"
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
