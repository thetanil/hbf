// HBF server.js - Phase 3.2
// Express-style routing with QuickJS and next() middleware

// Note: In QuickJS, we need to load router.js first
// For now, we'll define routes assuming router.js is pre-loaded

// Example middleware: Add custom header to all responses
app.use(function(req, res, next) {
	res.set('X-Powered-By', 'HBF');
	next(); // Continue to next handler
});

// Homepage - list of available routes
app.get('/', function(req, res) {
	var html = '<!DOCTYPE html>\n' +
		'<html>\n' +
		'<head>\n' +
		'  <meta charset="utf-8">\n' +
		'  <title>HBF - Available Routes</title>\n' +
		'  <style>\n' +
		'    body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif; margin: 40px; background: #f5f5f5; }\n' +
		'    .container { max-width: 800px; margin: 0 auto; background: white; padding: 30px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }\n' +
		'    h1 { color: #333; margin-top: 0; }\n' +
		'    .subtitle { color: #666; margin-bottom: 30px; }\n' +
		'    .route { margin: 15px 0; padding: 15px; background: #f8f9fa; border-left: 3px solid #007bff; border-radius: 4px; }\n' +
		'    .route-method { display: inline-block; padding: 2px 8px; background: #007bff; color: white; border-radius: 3px; font-size: 12px; font-weight: bold; margin-right: 10px; }\n' +
		'    .route-path { font-family: monospace; font-weight: bold; color: #333; }\n' +
		'    .route-desc { color: #666; margin-top: 5px; font-size: 14px; }\n' +
		'    a { color: #007bff; text-decoration: none; }\n' +
		'    a:hover { text-decoration: underline; }\n' +
		'  </style>\n' +
		'</head>\n' +
		'<body>\n' +
		'  <div class="container">\n' +
		'    <h1>HBF Web Compute Environment</h1>\n' +
		'    <p class="subtitle">Available Routes</p>\n' +
		'    \n' +
		'    <div class="route">\n' +
		'      <span class="route-method">GET</span>\n' +
		'      <span class="route-path"><a href="/hello">/hello</a></span>\n' +
		'      <div class="route-desc">Simple JSON greeting with timestamp</div>\n' +
		'    </div>\n' +
		'    \n' +
		'    <div class="route">\n' +
		'      <span class="route-method">GET</span>\n' +
		'      <span class="route-path"><a href="/user/42">/user/:id</a></span>\n' +
		'      <div class="route-desc">Route parameter example - try <a href="/user/42">/user/42</a> or <a href="/user/alice">/user/alice</a></div>\n' +
		'    </div>\n' +
		'    \n' +
		'    <div class="route">\n' +
		'      <span class="route-method">GET</span>\n' +
		'      <span class="route-path"><a href="/echo">/echo</a></span>\n' +
		'      <div class="route-desc">Echo request information (method, path, query, headers)</div>\n' +
		'    </div>\n' +
		'    \n' +
		'    <div class="route">\n' +
		'      <span class="route-method">GET</span>\n' +
		'      <span class="route-path"><a href="/health">/health</a></span>\n' +
		'      <div class="route-desc">Health check endpoint (built-in)</div>\n' +
		'    </div>\n' +
		'    \n' +
		'    <div class="route">\n' +
		'      <span class="route-method">GET</span>\n' +
		'      <span class="route-path"><a href="/api/nodes">/api/nodes</a></span>\n' +
		'      <div class="route-desc">List nodes from database (DB API example)</div>\n' +
		'    </div>\n' +
		'    \n' +
		'    <div class="route">\n' +
		'      <span class="route-method">GET</span>\n' +
		'      <span class="route-path"><a href="/api/nodes/1">/api/nodes/:id</a></span>\n' +
		'      <div class="route-desc">Get node by ID (DB API example with parameter)</div>\n' +
		'    </div>\n' +
		'    \n' +
		'    <div class="route">\n' +
		'      <span class="route-method">POST</span>\n' +
		'      <span class="route-path">/api/nodes</span>\n' +
		'      <div class="route-desc">Create a new node (DB API write example)</div>\n' +
		'    </div>\n' +
		'  </div>\n' +
		'</body>\n' +
		'</html>';

	res.set('Content-Type', 'text/html');
	res.send(html);
});

// Simple hello route
app.get('/hello', function(req, res) {
	res.json({ message: 'Hello, World!', timestamp: Date.now() });
});

// Route with parameter
app.get('/user/:id', function(req, res) {
	res.json({
		userId: req.params.id,
		name: 'User ' + req.params.id
	});
});

// Echo endpoint (shows request info)
app.get('/echo', function(req, res) {
	res.json({
		method: req.method,
		path: req.path,
		query: req.query,
		headers: req.headers
	});
});

// Database example: List all nodes
app.get('/api/nodes', function(req, res) {
	try {
		var rows = db.query('SELECT id, type, created_at FROM nodes ORDER BY created_at DESC LIMIT 10');
		res.json({
			count: rows.length,
			nodes: rows
		});
	} catch (e) {
		res.status(500).json({ error: e.toString() });
	}
});

// Database example: Get node by ID
app.get('/api/nodes/:id', function(req, res) {
	try {
		var rows = db.query('SELECT * FROM nodes WHERE id = ?', [req.params.id]);
		if (rows.length === 0) {
			res.status(404).json({ error: 'Node not found' });
		} else {
			res.json(rows[0]);
		}
	} catch (e) {
		res.status(500).json({ error: e.toString() });
	}
});

// Database example: Create a new node (POST)
app.post('/api/nodes', function(req, res) {
	try {
		// In a real app, req.body would contain parsed JSON
		// For now, create a simple test node
		var result = db.execute(
			'INSERT INTO nodes (type, body) VALUES (?, ?)',
			['test', '{"message": "Created via API"}']
		);
		res.status(201).json({
			message: 'Node created',
			affected: result
		});
	} catch (e) {
		res.status(500).json({ error: e.toString() });
	}
});

// 404 handler (middleware fallback)
// This only runs if no route matched
app.use(function(req, res) {
	res.status(404).json({
		error: 'Not Found',
		path: req.path
	});
	// No next() call - this is the final handler
});

// For compatibility
app.listen(5309);
