// HBF server.js - Phase 3.2
// Express-style routing with QuickJS

// Note: In QuickJS, we need to load router.js first
// For now, we'll define routes assuming router.js is pre-loaded

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

// 404 handler (middleware)
app.use(function(req, res) {
	res.status(404).json({
		error: 'Not Found',
		path: req.path
	});
});

// For compatibility
app.listen(5309);
