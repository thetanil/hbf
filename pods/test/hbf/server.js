
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
    <link rel="stylesheet" href="/static/style.css">
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
    <ul>
        <li><a href="/htmx">/htmx</a> - HTMX dynamic content loading example</li>
    </ul>
</body>
</html>`);
});

// Route: load a page with htmx which loads content dynamically
router.on('GET', '/htmx', (req, res) => {
    res.set("Content-Type", "text/html");
    res.send(`<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>HBF HTMX Cards Demo</title>
    <script src="/static/vendor/htmx.min.js"></script>
</head>
<body>
    <h1>HTMX Hypermedia Card Demo</h1>
    <p>This page loads cards dynamically from hypermedia endpoints using HTMX.</p>
    <div id="cards">
        <!-- Cards will be loaded here on page load -->
        <div hx-get="/card/1" hx-trigger="load" hx-target="#cards" hx-swap="beforeend"></div>
        <div hx-get="/card/2" hx-trigger="load" hx-target="#cards" hx-swap="beforeend"></div>
        <div hx-get="/card/3" hx-trigger="load" hx-target="#cards" hx-swap="beforeend"></div>
    </div>
    <hr>
    <button hx-get="/card/4" hx-target="#cards" hx-swap="beforeend">Load Another Card</button>
</body>
</html>`);
});

// Route: Hello world
// Card routes (hypermedia endpoints)
router.on('GET', '/card/:id', (req, res, params) => {
    const id = params.id;
    let title = `Card Title ${id}`;
    let content = `This is the content for card #${id}. Loaded dynamically via HTMX.`;
    // Example: customize card 2
    if (id === '2') {
        title = 'Special Card';
        content = 'This card has custom content.';
    }
    res.set("Content-Type", "text/html");
    res.send(renderCard(id, title, content));
});

// Card template function (inline styled)
function renderCard(id, title, content) {
    return `<div style="border:1px solid #ccc;border-radius:8px;padding:16px;margin:12px 0;background:#f9f9f9;box-shadow:0 2px 6px #eee;max-width:400px;">
            <span style="width:32px;height:32px;display:inline-block;vertical-align:middle;overflow:hidden;">
                <div hx-get="/icons/rocket/svg" hx-trigger="load" hx-target="this" hx-swap="outerHTML" style="width:32px;height:32px;display:inline-block;vertical-align:middle;"></div>
            </span>
        <h3 style="margin-top:0;display:inline-block;margin-left:8px;vertical-align:middle;">${title}</h3>
        <p>${content}</p>
        <small style="color:#888;">Card #${id}</small>
    </div>`;
}

router.on('GET', '/icons/rocket/svg', (req, res) => {
    res.set("Content-Type", "image/svg+xml");
    res.send(`<svg version="1.1" id="_x32_" xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" viewBox="0 0 512 512" xml:space="preserve" fill="#000000" style="width:32px;height:32px;display:inline-block;"><g id="SVGRepo_bgCarrier" stroke-width="0"></g><g id="SVGRepo_tracerCarrier" stroke-linecap="round" stroke-linejoin="round"></g><g id="SVGRepo_iconCarrier"> <style type="text/css">  .st0{fill:#000000;}  </style> <g> <path class="st0" d="M127.083,247.824l50.031-76.906c0,0-74.734-29.688-109.547-3.078C32.755,194.465,0.005,268.184,0.005,268.184 l37.109,21.516C37.114,289.699,84.083,198.684,127.083,247.824z"></path> <path class="st0" d="M264.177,384.918l76.906-50.031c0,0,29.688,74.734,3.078,109.547 c-26.625,34.797-100.344,67.563-100.344,67.563l-21.5-37.109C222.317,474.887,313.333,427.918,264.177,384.918z"></path> <path class="st0" d="M206.692,362.887l-13.203-13.188c-24,62.375-80.375,49.188-80.375,49.188s-13.188-56.375,49.188-80.375 l-13.188-13.188c-34.797-6-79.188,35.984-86.391,76.766c-7.188,40.781-8.391,75.563-8.391,75.563s34.781-1.188,75.578-8.391 S212.692,397.684,206.692,362.887z"></path> <path class="st0" d="M505.224,6.777C450.786-18.738,312.927,28.98,236.255,130.668c-58.422,77.453-89.688,129.641-89.688,129.641 l46.406,46.406l12.313,12.313l46.391,46.391c0,0,52.219-31.25,129.672-89.656C483.005,199.074,530.739,61.215,505.224,6.777z M274.63,237.371c-12.813-12.813-12.813-33.594,0-46.406s33.578-12.813,46.406,0.016c12.813,12.813,12.813,33.578,0,46.391 C308.208,250.184,287.442,250.184,274.63,237.371z M351.552,160.465c-16.563-16.578-16.563-43.422,0-59.984 c16.547-16.563,43.406-16.563,59.969,0s16.563,43.406,0,59.984C394.958,177.012,368.099,177.012,351.552,160.465z"></path> </g> </g></svg>`);
});

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
