# HBF Integration Test

## Overview

`integration_test.sh` is a bash script that performs end-to-end testing of the HBF server by:

1. Building the HBF binary with Bazel
2. Starting the server on a random available port (10000-20000)
3. Testing multiple HTTP endpoints with `curl`
4. Verifying responses and status codes
5. Gracefully shutting down the server

## Usage

```bash
# Run the integration test
./tools/integration_test.sh
```

The script will automatically:
- Find an available port
- Build the server
- Start it in the background
- Run all tests
- Clean up (stop server and remove temp files)

## Tests Performed

The script tests the following endpoints:

| Test | Endpoint | Method | Expected Status | Validates |
|------|----------|--------|-----------------|-----------|
| Health Check | `/health` | GET | 200 | Server is running, returns `"status":"ok"` |
| Homepage | `/` | GET | 200 | HTML page contains "Available Routes" |
| Hello | `/hello` | GET | 200 | JSON contains "Hello, World!" |
| User Param (numeric) | `/user/42` | GET | 200 | JSON contains `"userId":"42"` |
| User Param (string) | `/user/alice` | GET | 200 | JSON contains `"userId":"alice"` |
| Echo | `/echo` | GET | 200 | JSON contains `"method":"GET"` |
| 404 Handler | `/does-not-exist` | GET | 404 | JSON contains "Not Found" |
| Middleware Header | `/hello` | GET | 200 | Response header contains `X-Powered-By: HBF` |

## Exit Codes

- `0` - All tests passed
- `1` - One or more tests failed

## Requirements

- Bazel (for building)
- `curl` (for HTTP requests)
- `nc` or `netcat` (for port checking)

## Known Issues

**Current Status**: Tests 2-8 fail with 500 errors due to a JavaScript context initialization bug.

The issue is that `router.js`'s `app` global variable is not accessible when called from the C handler (`internal/http/handler.c`). The QuickJS context pool loads the scripts during initialization, but the global scope appears to be lost when handling requests.

**Test 1 (Health Check)** passes because it's handled directly by C code without JavaScript.

### Debug Information

When tests fail, the script displays the last 20 lines of server stderr logs to help diagnose issues.

## Output Example

```
==================================
HBF Integration Test
==================================

Using port: 12630

Building HBF...
Build successful

Starting server on port 12630...
Server started (PID: 824738)

Waiting for server to start on port 12630...
Server is ready!

Running endpoint tests...
==================================
Testing GET /health - Health check... PASS
Testing GET / - Homepage HTML... PASS
Testing GET /hello - Hello JSON... PASS
...

==================================
Test Summary
==================================
Passed: 8
Failed: 0

ALL TESTS PASSED!
```

## Adding New Tests

To add a new test, add a call to `test_endpoint` in the `main()` function:

```bash
if test_endpoint "GET" "/your-path" "200" "Your description" "grep -q 'expected-content'"; then
    passed=$((passed + 1))
else
    failed=$((failed + 1))
fi
```

Parameters:
- `$1` - HTTP method (GET, POST, etc.)
- `$2` - URL path
- `$3` - Expected HTTP status code
- `$4` - Test description
- `$5` - Additional validation command (optional)

The validation command receives the response body via stdin and should return 0 for success.
