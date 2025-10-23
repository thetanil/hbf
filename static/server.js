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
    // Diagnostic logging
    if (typeof print === 'function') {
        print('handleRequest: incoming req = ' + JSON.stringify(req));
    }
    var status = 200;
    var body = '';
    var headers = {};
    if (req.uri === '/' || req.path === '/') {
        status = 200;
        body = 'Hello from JavaScript!';
    } else if (req.uri === '/qjshealth' || req.path === '/qjshealth') {
        status = 200;
        body = '{"status":"ok"}';
        headers['Content-Type'] = 'application/json';
    } else if (req.uri === '/echo' || req.path === '/echo') {
        status = 200;
        body = '{"received":' + JSON.stringify(req.body || '') + '}';
        headers['Content-Type'] = 'application/json';
    } else {
        status = 404;
        body = 'Not Found';
    }
    var resObj = {
        status: status,
        body: body,
        headers: headers
    };
    if (typeof print === 'function') {
        print('handleRequest: outgoing res = ' + JSON.stringify(resObj));
    }
    return resObj;
}



function handleRequest(req) {
    var status = 200;
    var body = '';
    var headers = {};
    if (req.uri === '/' || req.path === '/') {
        status = 200;
        body = 'Hello from JavaScript!';
    } else if (req.uri === '/qjshealth' || req.path === '/qjshealth') {
        status = 200;
        body = '{"status":"ok"}';
        headers['Content-Type'] = 'application/json';
    } else if (req.uri === '/echo' || req.path === '/echo') {
        status = 200;
        body = '{"received":' + JSON.stringify(req.body || '') + '}';
        headers['Content-Type'] = 'application/json';
    } else {
        status = 404;
        body = 'Not Found';
    }
    var resObj = {
        status: status,
        body: body,
        headers: headers
    };
    return resObj;
}

globalThis.handleRequest = handleRequest;
