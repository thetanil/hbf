
// Minimal server.js for HBF integration test
app = {};

app.handle = function (req, res) {
    // Middleware: add custom header
    res.set("X-Powered-By", "HBF");

    const { method, path } = req;
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
    // Default: 404
    res.status(404);
    res.set("Content-Type", "text/plain");
    res.send("Not Found");
};
