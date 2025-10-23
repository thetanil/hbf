// Minimal server.js - single endpoint test
// No router, no framework, just a simple request handler

// Global app object that C code will call
var app = {
    handle: function (req, res) {
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

function handleRequest(req) {
    // Minimal response object
    var res = {
        status: function (code) { this.status = code; return this; },
        send: function (body) { this.body = body; return this; }
    };
    // Simple hello endpoint
    if (req.uri === '/' || req.path === '/') {
        res.status(200);
        res.send('Hello from JavaScript!');
    } else if (req.uri === '/qjshealth' || req.path === '/qjshealth') {
        res.status(200);
        res.send('{"status":"ok"}');
    } else if (req.uri === '/echo' || req.path === '/echo') {
        res.status(200);
        res.send('{"received":' + JSON.stringify(req.body || '') + '}');
    } else {
        res.status(404);
        res.send('Not Found');
    }
    return res;
}

globalThis.handleRequest = handleRequest;
