// Express-style router for HBF
// Pure JavaScript implementation compatible with QuickJS

function Router() {
    this.stack = []; // Unified middleware/route stack (like Express 5.x)
    this._inHandle = false; // reentrancy guard
}

// Compile path pattern to regex (supports :param syntax)
function compilePath(path) {
    var keys = [];
    var pattern = path;

    // Replace :param with regex capture group
    pattern = pattern.replace(/:([^\/]+)/g, function (match, key) {
        keys.push(key);
        return '([^/]+)';
    });

    // Escape forward slashes
    pattern = pattern.replace(/\//g, '\\/');

    return {
        regex: new RegExp('^' + pattern + '$'),
        keys: keys
    };
}

// Register GET route
Router.prototype.get = function (path, handler) {
    this.stack.push({
        method: 'GET',
        path: compilePath(path),
        handler: handler
    });
    return this;
};

// Register POST route
Router.prototype.post = function (path, handler) {
    this.stack.push({
        method: 'POST',
        path: compilePath(path),
        handler: handler
    });
    return this;
};

// Register PUT route
Router.prototype.put = function (path, handler) {
    this.stack.push({
        method: 'PUT',
        path: compilePath(path),
        handler: handler
    });
    return this;
};

// Register DELETE route
Router.prototype.delete = function (path, handler) {
    this.stack.push({
        method: 'DELETE',
        path: compilePath(path),
        handler: handler
    });
    return this;
};

// Register middleware (runs on all requests, all methods)
Router.prototype.use = function (handler) {
    this.stack.push({
        method: null, // null = matches all methods
        path: null,   // null = matches all paths
        handler: handler
    });
    return this;
};

// Handle incoming request (internal - called from C handler only)
Router.prototype.handle = function (req, res) {
    var self = this;
    var method = req.method || 'GET';
    var path = req.path || '/';
    var idx = 0;
    var nextCalled = false;
    var callNext = true;

    // Prevent accidental recursion like app.use(app.handle) or nested app.handle()
    if (this._inHandle) {
        throw new Error('Re-entrant app.handle() detected (path=' + path + ')');
    }
    this._inHandle = true;

    // Initialize params
    req.params = req.params || {};

    // next() function - allows middleware to continue the chain
    function next(err) {
        if (err) {
            // Error handling
            throw err;
        }
        nextCalled = true;
    }

    // Iterative middleware/routing chain (avoids stack overflow)
    try {
        while (callNext && idx < self.stack.length) {
            nextCalled = false;
            callNext = false;

            // Get next layer from stack
            var layer = self.stack[idx++];

            // Check if this layer matches the request
            var methodMatch = layer.method === null || layer.method === method;
            var pathMatch = false;
            var match = null;

            if (layer.path === null) {
                // Middleware - matches all paths
                pathMatch = true;
            } else {
                // Route - check if path matches
                match = path.match(layer.path.regex);
                pathMatch = match !== null;

                if (pathMatch && match) {
                    // Extract route parameters
                    for (var j = 0; j < layer.path.keys.length; j++) {
                        req.params[layer.path.keys[j]] = match[j + 1];
                    }
                }
            }

            // If layer doesn't match, continue to next
            if (!methodMatch || !pathMatch) {
                callNext = true;
                continue;
            }

            // Call the handler
            // Handler may call next() to continue, or not (to end the chain)
            layer.handler(req, res, next);

            // If handler called next(), continue the chain
            if (nextCalled) {
                callNext = true;
            }
        }
    } finally {
        // Always clear the reentrancy flag
        this._inHandle = false;
    }
};

// Dummy listen method (for Express.js compatibility)
Router.prototype.listen = function (port) {
    // No-op: HBF handles the actual server listening
    return this;
};

// Export router instance and make it global
// Top-level var with JS_EVAL_TYPE_GLOBAL should be automatically global in QuickJS
var app = new Router();

// Debug: verify app is accessible
if (typeof app === 'undefined') {
    throw new Error('ROUTER.JS BUG: app is undefined after Router creation!');
}
if (typeof app.handle !== 'function') {
    throw new Error('ROUTER.JS BUG: app.handle is not a function!');
}
