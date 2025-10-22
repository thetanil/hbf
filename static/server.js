// Minimal server.js for Phase 3.1
// This will be loaded from database and evaluated in QuickJS

console.log('HBF server.js loaded - Phase 3.1');

// Simple test function
function greet(name) {
	return 'Hello, ' + name + '!';
}

// Export for testing
if (typeof module !== 'undefined') {
	module.exports = { greet: greet };
}
