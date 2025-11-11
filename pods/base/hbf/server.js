
// Minimal server.js for HBF integration test

import { hello as staticHello, value as staticValue } from "./lib/esm_test.js";

// In ES modules, variables are module-scoped, not global Use globalThis to
// expose app to the C handler The line globalThis.app = {}
globalThis.app = {};

// Helper: Parse query string into object
function parseQuery(queryString) {
    const params = {};
    if (!queryString) return params;
    queryString.split('&').forEach(function (pair) {
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

    // ESM import test route (dynamic import)
    if (path === "/esm-test" && method === "GET") {
        (async function () {
            try {
                const mod = await import("./lib/esm_test.js");
                res.set("Content-Type", "application/json");
                res.send(JSON.stringify({
                    message: mod.hello("ESM"),
                    value: mod.value
                }));
            } catch (e) {
                res.status(500);
                res.set("Content-Type", "application/json");
                res.send(JSON.stringify({ error: "Import failed", details: String(e) }));
            }
        })();
        return;
    }

    // ESM import test route (static import)
    if (path === "/esm-test-static" && method === "GET") {
        res.set("Content-Type", "application/json");
        res.send(JSON.stringify({
            message: staticHello("Static ESM"),
            value: staticValue
        }));
        return;
    }
    if (path === "/" && method === "GET") {
        // Serve index.html
        res.set("Content-Type", "text/html");
        res.send(`<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>HBF</title>
    <link rel="stylesheet" href="/static/style.css">
    <link rel="icon" type="image/x-icon" href="/static/favicon.ico">
</head>
<body>
    <h1>HBF</h1>
    <p>Single binary web compute environment</p>
    <ul>
        <li><a href="https://github.com/thetanil/hbf/actions">CI/CD Status</a></li>
    </ul>
    <h2>Available Routes</h2>
    <ul>
        <li><a href="/health">/health</a> - Health check</li>
        <li><a href="/hello">/hello</a> - Hello world</li>
        <li><a href="/user/42">/user/42</a> - User endpoint</li>
        <li><a href="/echo">/echo</a> - Echo request</li>
        <li><a href="/esm-test">/esm-test</a> - Dynamic ESM import test</li>
        <li><a href="/esm-test-static">/esm-test-static</a> - Static ESM import test</li>
    </ul>
    <h2>Static Assets</h2>
    <ul>
        <li><a href="/static/style.css">/static/style.css</a> - Local CSS file</li>
        <li><a href="/static/vendor/htmx.min.js">/static/vendor/htmx.min.js</a> - HTMX library</li>
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

    // Default: 404
    res.status(404);
    res.set("Content-Type", "text/plain");
    res.send("Not Found");
};
