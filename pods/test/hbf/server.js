
// Minimal server.js for HBF integration test

import Router from "./lib/find-my-way.js";
import { hello as staticHello, value as staticValue } from "./lib/esm_test.js";

// In ES modules, variables are module-scoped, not global Use globalThis to
// expose app to the C handler The line globalThis.app = {}
globalThis.app = {};

// Initialize the router
const router = Router({
    ignoreTrailingSlash: true,
    caseSensitive: true
});

// Route: Home page
router.on('GET', '/', (req, res) => {
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
        <li><a href="/static/monaco/vs/loader.js">/static/monaco/vs/loader.js</a> - Monaco editor loader</li>
        <li><a href="/static/monaco/vs/editor/editor.main.js">/static/monaco/vs/editor/editor.main.js</a> - Monaco editor main</li>
    </ul>
</body>
</html>`);
});

// Route: Hello world
router.on('GET', '/hello', (req, res) => {
    res.set("Content-Type", "application/json");
    res.send(JSON.stringify({ message: "Hello, World!" }));
});

// Route: User by ID (parameter route)
router.on('GET', '/user/:id', (req, res, params) => {
    res.set("Content-Type", "application/json");
    res.send(JSON.stringify({ userId: params.id }));
});

// Route: Echo
router.on('GET', '/echo', (req, res) => {
    res.set("Content-Type", "application/json");
    res.send(JSON.stringify({ method: req.method, url: req.url }));
});

// Route: ESM import test (dynamic import)
router.on('GET', '/esm-test', (req, res) => {
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
});

// Route: ESM import test (static import)
router.on('GET', '/esm-test-static', (req, res) => {
    res.set("Content-Type", "application/json");
    res.send(JSON.stringify({
        message: staticHello("Static ESM"),
        value: staticValue
    }));
});

// Request adapter: transform HBF req to router-compatible format
function adaptReq(originalReq) {
    return {
        method: originalReq.method,
        url: originalReq.path + (originalReq.query ? ('?' + originalReq.query) : '')
    };
}

// Main request handler
app.handle = function (req, res) {
    // Middleware: add custom header
    res.set("X-Powered-By", "HBF");

    // Adapt request to router format
    const adaptedReq = adaptReq(req);

    // Extract path without query string for routing
    const path = adaptedReq.url.split('?')[0];

    // Find matching route
    const match = router.find(adaptedReq.method, path);

    if (!match) {
        res.status(404);
        res.set("Content-Type", "text/plain");
        res.send("Not Found");
        return;
    }

    // Execute route handler
    try {
        match.handler(adaptedReq, res, match.params);
    } catch (e) {
        res.status(500);
        res.set("Content-Type", "application/json");
        res.send(JSON.stringify({ error: "Router handler error", details: String(e) }));
    }
};
