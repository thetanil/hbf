// HBF server.js - Express.js-compatible routing
// Loaded from database on each request for complete isolation

// Global app object that C code will call
var app = {
    handle: function (req, res) {
        // Add custom header for all responses (middleware simulation)
        res.set('X-Powered-By', 'HBF');

        // Route: Homepage
        if (req.path === '/') {
            res.status(200);
            res.send('<html><body><h1>HBF Server</h1><p>Available Routes:</p><ul>' +
                '<li>GET /hello - Hello World JSON again</li>' +
                '<li>GET /user/:id - User info with parameter</li>' +
                '<li>GET /echo - Echo request info</li>' +
                '</ul></body></html>');
            return;
        }

        // Route: /hello - Hello World JSON
        if (req.path === '/hello') {
            res.status(200);
            res.json({ message: 'Hello World', status: 'ok' });
            return;
        }

        // Route: /user/:id - User with parameter extraction
        var userMatch = req.path.match(/^\/user\/([^\/]+)$/);
        if (userMatch) {
            var userId = userMatch[1];
            res.status(200);
            res.json({ userId: userId, message: 'User retrieved' });
            return;
        }

        // Route: /echo - Echo request info
        if (req.path === '/echo') {
            res.status(200);
            res.json({
                method: req.method,
                path: req.path,
                headers: req.headers || {}
            });
            return;
        }

        // Route: /hellojs - Legacy endpoint for backward compatibility
        if (req.path === '/hellojs') {
            res.status(200);
            res.send('Hello from JavaScript!');
            return;
        }

        // 404 - Not Found
        res.status(404);
        res.send('Not Found');
    }
};
