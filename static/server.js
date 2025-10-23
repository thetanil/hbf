// Minimal server.js - single endpoint test
// No router, no framework, just a simple request handler

// Global app object that C code will call
var app = {
    handle: function(req, res) {
        // Simple hello endpoint
        if (req.path === '/hellojs') {
            res.status(200);
            res.send('Hello from JavaScript!');
        } else {
            res.status(404);
            res.send('Not Found');
        }
    }
};
