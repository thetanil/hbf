// Stub for node:assert (QuickJS compatibility)
// QuickJS doesn't provide Node.js built-in modules
function assert(condition, message) {
    if (!condition) {
        throw new Error(message || 'Assertion failed');
    }
}
assert.ok = assert;
assert.strictEqual = function(a, b, message) {
    if (a !== b) {
        throw new Error(message || `Expected ${a} to equal ${b}`);
    }
};
module.exports = assert;
