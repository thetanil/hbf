
// Minimal server.js template for new HBF pod
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

    // Root route
    if (path === "/" && method === "GET") {
        res.set("Content-Type", "text/html");
        res.send(`<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>My HBF Pod</title>
</head>
<body>
    <h1>My HBF Pod</h1>
    <p>Single binary web compute environment</p>
</body>
</html>`);
        return;
    }

    // Health check
    if (path === "/health" && method === "GET") {
        res.set("Content-Type", "application/json");
        res.send(JSON.stringify({ status: "ok" }));
        return;
    }

    // Default: 404
    res.status(404);
    res.set("Content-Type", "text/plain");
    res.send("Not Found");
};
