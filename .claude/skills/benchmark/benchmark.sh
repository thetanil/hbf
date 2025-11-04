#!/bin/bash
# SPDX-License-Identifier: MIT
# Main HBF benchmark runner

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/lib/db.sh"
source "${SCRIPT_DIR}/lib/server.sh"
source "${SCRIPT_DIR}/lib/parser.sh"

# Default parameters
REQUESTS=1000
CONCURRENCY=10
BUILD_MODE="opt"
NOTES=""
WORKSPACE_ROOT="/workspaces/hbf"

# Parse command line arguments
while [[ $# -gt 0 ]]; do
	case $1 in
		--requests)
			REQUESTS="$2"
			shift 2
			;;
		--concurrency)
			CONCURRENCY="$2"
			shift 2
			;;
		--build-mode)
			BUILD_MODE="$2"
			shift 2
			;;
		--notes)
			NOTES="$2"
			shift 2
			;;
		--help)
			echo "Usage: $0 [options]"
			echo "Options:"
			echo "  --requests N        Number of requests per endpoint (default: 1000)"
			echo "  --concurrency N     Number of concurrent connections (default: 10)"
			echo "  --build-mode MODE   Build mode: opt, dbg, fastbuild (default: opt)"
			echo "  --notes TEXT        Notes for this benchmark run"
			echo "  --help              Show this help message"
			exit 0
			;;
		*)
			echo "Unknown option: $1"
			exit 1
			;;
	esac
done

echo "========================================"
echo "HBF Benchmark Runner"
echo "========================================"
echo "Requests per endpoint: ${REQUESTS}"
echo "Concurrency: ${CONCURRENCY}"
echo "Build mode: ${BUILD_MODE}"
echo "Notes: ${NOTES:-none}"
echo ""

# Change to workspace root
cd "${WORKSPACE_ROOT}"

# Check prerequisites
echo "Checking prerequisites..."
command -v ab >/dev/null 2>&1 || { echo "ERROR: ab (Apache Bench) not found. Install apache2-utils."; exit 1; }
command -v sqlite3 >/dev/null 2>&1 || { echo "ERROR: sqlite3 not found"; exit 1; }
command -v bazel >/dev/null 2>&1 || { echo "ERROR: bazel not found"; exit 1; }
echo "✓ All prerequisites found"
echo ""

# Initialize database
echo "Initializing benchmark database..."
init_db
echo "✓ Database ready"
echo ""

# Get git metadata
GIT_COMMIT=$(git rev-parse --short HEAD 2>/dev/null || echo "unknown")
GIT_BRANCH=$(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo "unknown")
echo "Git commit: ${GIT_COMMIT}"
echo "Git branch: ${GIT_BRANCH}"
echo ""

# Build binary
echo "Building HBF binary (mode: ${BUILD_MODE})..."
bazel build //:hbf --compilation_mode="${BUILD_MODE}"
if [ $? -ne 0 ]; then
	echo "ERROR: Build failed"
	exit 1
fi
echo "✓ Build successful"
echo ""

# Create benchmark run
echo "Creating benchmark run record..."
RUN_ID=$(create_run "${GIT_COMMIT}" "${GIT_BRANCH}" "${BUILD_MODE}" "${NOTES}")
echo "✓ Run ID: ${RUN_ID}"
echo ""

# Cleanup function
cleanup() {
	echo ""
	echo "Cleaning up..."
	stop_all_servers
	rm -f /tmp/ab_output_*.txt
	rm -f /tmp/bench_body.txt
	echo "✓ Cleanup complete"
}
trap cleanup EXIT

# Create temp directory for results
TEMP_DIR=$(mktemp -d)
echo "Temp directory: ${TEMP_DIR}"
echo ""

# Start normal server (port 5309)
echo "Starting HBF server on port 5309..."
BINARY_PATH="${WORKSPACE_ROOT}/bazel-bin/bin/hbf"
start_server "false" "5309" "${SERVER_PID_FILE}"
if [ $? -ne 0 ]; then
	echo "ERROR: Failed to start server"
	exit 1
fi
echo ""

# Start dev server (port 5310) for write tests
echo "Starting HBF server on port 5310 (dev mode)..."
start_server "true" "5310" "${SERVER_DEV_PID_FILE}"
if [ $? -ne 0 ]; then
	echo "ERROR: Failed to start dev server"
	exit 1
fi
echo ""

# Prepare test data for PUT requests
echo "test data for benchmarking" > /tmp/bench_body.txt

# Category 1: Static Assets
echo "========================================"
echo "Category 1: Static Assets"
echo "========================================"

endpoints_static=(
	"/static/style.css"
	"/static/favicon.ico"
)

for endpoint in "${endpoints_static[@]}"; do
	echo "Benchmarking: ${endpoint}"
	output_file="${TEMP_DIR}/ab_static_${endpoint//\//_}.txt"
	metrics=$(run_ab_benchmark "http://localhost:5309${endpoint}" "${REQUESTS}" "${CONCURRENCY}" "${output_file}")

	if [ $? -eq 0 ]; then
		read -r time_taken rps time_per_req transfer_rate failed <<< "${metrics}"
		echo "  Requests/sec: $(format_rps ${rps})"
		echo "  Time/request: ${time_per_req} ms"
		echo "  Failed: ${failed}"

		store_result "${RUN_ID}" "static" "${endpoint}" "${REQUESTS}" "${CONCURRENCY}" \
			"${time_taken}" "${rps}" "${time_per_req}" "${transfer_rate}" "${failed}"
	else
		echo "  ERROR: Benchmark failed"
	fi
	echo ""
done

# Category 2: QuickJS Runtime Routes
echo "========================================"
echo "Category 2: QuickJS Runtime Routes"
echo "========================================"

endpoints_runtime=(
	"/hello"
	"/user/42"
	"/echo"
)

for endpoint in "${endpoints_runtime[@]}"; do
	echo "Benchmarking: ${endpoint}"
	output_file="${TEMP_DIR}/ab_runtime_${endpoint//\//_}.txt"
	metrics=$(run_ab_benchmark "http://localhost:5309${endpoint}" "${REQUESTS}" "${CONCURRENCY}" "${output_file}")

	if [ $? -eq 0 ]; then
		read -r time_taken rps time_per_req transfer_rate failed <<< "${metrics}"
		echo "  Requests/sec: $(format_rps ${rps})"
		echo "  Time/request: ${time_per_req} ms"
		echo "  Failed: ${failed}"

		store_result "${RUN_ID}" "runtime" "${endpoint}" "${REQUESTS}" "${CONCURRENCY}" \
			"${time_taken}" "${rps}" "${time_per_req}" "${transfer_rate}" "${failed}"
	else
		echo "  ERROR: Benchmark failed"
	fi
	echo ""
done

# Category 3: Versioned Filesystem Operations (dev mode, port 5310)
echo "========================================"
echo "Category 3: Versioned Filesystem Writes"
echo "========================================"

# Test file read from versioned filesystem
endpoint="/__dev/api/files"
echo "Benchmarking: GET ${endpoint}"
output_file="${TEMP_DIR}/ab_fs_read.txt"
metrics=$(run_ab_benchmark "http://localhost:5310${endpoint}" "${REQUESTS}" "${CONCURRENCY}" "${output_file}")

if [ $? -eq 0 ]; then
	read -r time_taken rps time_per_req transfer_rate failed <<< "${metrics}"
	echo "  Requests/sec: $(format_rps ${rps})"
	echo "  Time/request: ${time_per_req} ms"
	echo "  Failed: ${failed}"

	store_result "${RUN_ID}" "fs_read" "GET ${endpoint}" "${REQUESTS}" "${CONCURRENCY}" \
		"${time_taken}" "${rps}" "${time_per_req}" "${transfer_rate}" "${failed}"
else
	echo "  ERROR: Benchmark failed"
fi
echo ""

# Test file write to versioned filesystem
endpoint="/__dev/api/file?name=static/bench.txt"
echo "Benchmarking: PUT ${endpoint}"
output_file="${TEMP_DIR}/ab_fs_write.txt"
metrics=$(run_ab_put_benchmark "http://localhost:5310${endpoint}" "${REQUESTS}" "${CONCURRENCY}" "/tmp/bench_body.txt" "${output_file}")

if [ $? -eq 0 ]; then
	read -r time_taken rps time_per_req transfer_rate failed <<< "${metrics}"
	echo "  Requests/sec: $(format_rps ${rps})"
	echo "  Time/request: ${time_per_req} ms"
	echo "  Failed: ${failed}"

	store_result "${RUN_ID}" "fs_write" "PUT ${endpoint}" "${REQUESTS}" "${CONCURRENCY}" \
		"${time_taken}" "${rps}" "${time_per_req}" "${transfer_rate}" "${failed}"
else
	echo "  ERROR: Benchmark failed"
fi
echo ""

# Generate summary report
echo "========================================"
echo "Benchmark Summary"
echo "========================================"
echo ""
echo "Run ID: ${RUN_ID}"
echo "Timestamp: $(date '+%Y-%m-%d %H:%M:%S')"
echo "Commit: ${GIT_COMMIT}"
echo "Branch: ${GIT_BRANCH}"
echo "Build Mode: ${BUILD_MODE}"
echo "Notes: ${NOTES:-none}"
echo ""

echo "Results:"
get_run_results "${RUN_ID}"
echo ""

# Compare with previous run
PREV_RUN_ID=$(get_previous_run)
if [ -n "${PREV_RUN_ID}" ]; then
	echo "Comparison with previous run (ID: ${PREV_RUN_ID}):"
	echo ""

	for endpoint in "${endpoints_static[@]}" "${endpoints_runtime[@]}" "GET /__dev/api/files" "PUT /__dev/api/file?name=static/bench.txt"; do
		result=$(compare_endpoint "${endpoint}" "${PREV_RUN_ID}" "${RUN_ID}")
		if [ -n "${result}" ]; then
			echo "${result}" | sqlite3 -column "${DB_FILE}"
		fi
	done
	echo ""
fi

echo "✓ Benchmark complete!"
echo ""
echo "View results with: ${SCRIPT_DIR}/show_results.sh"
