// Express-style router for HBF
// Pure JavaScript implementation compatible with QuickJS

function Router() {
    this.routes = {
        GET: [],
        POST: [],
        PUT: [],
        DELETE: []
    };
    this.middleware = [];
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
    this.routes.GET.push({
        path: compilePath(path),
        handler: handler
    });
    return this;
};

// Register POST route
Router.prototype.post = function (path, handler) {
    this.routes.POST.push({
        path: compilePath(path),
        handler: handler
    });
    return this;
};

// Register PUT route
Router.prototype.put = function (path, handler) {
    this.routes.PUT.push({
        path: compilePath(path),
        handler: handler
    });
    return this;
};

// Register DELETE route
Router.prototype.delete = function (path, handler) {
    this.routes.DELETE.push({
        path: compilePath(path),
        handler: handler
    });
    return this;
};

// Register middleware (fallback handler)
Router.prototype.use = function (handler) {
    this.middleware.push(handler);
    return this;
};

// Handle incoming request
Router.prototype.handle = function (req, res) {
    var method = req.method || 'GET';
    var path = req.path || '/';
    var routes = this.routes[method] || [];
    var i, route, match, j;

    // Prevent accidental recursion like app.use(app.handle) or nested app.handle()
    if (this._inHandle) {
        throw new Error('Re-entrant app.handle() detected');
    }
    this._inHandle = true;

    // Try to match routes
    for (i = 0; i < routes.length; i++) {
        route = routes[i];
        match = path.match(route.path.regex);

        if (match) {
            // Extract parameters
            req.params = req.params || {};
            for (j = 0; j < route.path.keys.length; j++) {
                req.params[route.path.keys[j]] = match[j + 1];
            }

            // Call handler
            try {
                route.handler(req, res);
                return true;
            } finally {
                this._inHandle = false;
            }
        }
    }

    // Try middleware (fallback handlers)
    for (i = 0; i < this.middleware.length; i++) {
        try {
            this.middleware[i](req, res);
            return true;
        } finally {
            this._inHandle = false;
        }
    }

    // No handler found
    this._inHandle = false;
    return false;
};

// Dummy listen method (for Express.js compatibility)
Router.prototype.listen = function (port) {
    // No-op: HBF handles the actual server listening
    return this;
};

// Export router instance
var app = new Router();

// Make app available globally
if (typeof globalThis !== 'undefined') {
    globalThis.app = app;
} else if (typeof global !== 'undefined') {
    global.app = app;
}

// Also support module.exports for consistency
if (typeof module !== 'undefined' && module.exports) {
    module.exports = app;
}
