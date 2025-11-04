#!/bin/bash
# SPDX-License-Identifier: MIT
# Database operations for HBF benchmarking

DB_DIR="/workspaces/hbf/.benchmark"
DB_FILE="${DB_DIR}/results.db"

# Initialize database schema
init_db() {
	mkdir -p "${DB_DIR}"

	sqlite3 "${DB_FILE}" <<-EOF
	CREATE TABLE IF NOT EXISTS benchmark_runs (
		run_id INTEGER PRIMARY KEY AUTOINCREMENT,
		timestamp TEXT NOT NULL,
		git_commit TEXT,
		git_branch TEXT,
		build_mode TEXT,
		notes TEXT
	);

	CREATE TABLE IF NOT EXISTS benchmark_results (
		result_id INTEGER PRIMARY KEY AUTOINCREMENT,
		run_id INTEGER NOT NULL,
		category TEXT NOT NULL,
		endpoint TEXT NOT NULL,
		requests INTEGER NOT NULL,
		concurrency INTEGER NOT NULL,
		time_taken REAL NOT NULL,
		requests_per_sec REAL NOT NULL,
		time_per_request REAL NOT NULL,
		transfer_rate REAL NOT NULL,
		failed_requests INTEGER NOT NULL,
		FOREIGN KEY (run_id) REFERENCES benchmark_runs(run_id)
	);

	CREATE INDEX IF NOT EXISTS idx_run_timestamp ON benchmark_runs(timestamp);
	CREATE INDEX IF NOT EXISTS idx_result_run ON benchmark_results(run_id);
	CREATE INDEX IF NOT EXISTS idx_result_endpoint ON benchmark_results(endpoint);
	EOF
}

# Create new benchmark run
# Args: git_commit git_branch build_mode notes
create_run() {
	local commit="$1"
	local branch="$2"
	local build_mode="$3"
	local notes="$4"
	local timestamp
	timestamp=$(date '+%Y-%m-%d %H:%M:%S')

	sqlite3 "${DB_FILE}" <<-EOF
	INSERT INTO benchmark_runs (timestamp, git_commit, git_branch, build_mode, notes)
	VALUES ('${timestamp}', '${commit}', '${branch}', '${build_mode}', '${notes}');
	SELECT last_insert_rowid();
	EOF
}

# Store benchmark result
# Args: run_id category endpoint requests concurrency time_taken rps time_per_req transfer_rate failed
store_result() {
	local run_id="$1"
	local category="$2"
	local endpoint="$3"
	local requests="$4"
	local concurrency="$5"
	local time_taken="$6"
	local rps="$7"
	local time_per_req="$8"
	local transfer_rate="$9"
	local failed="${10}"

	sqlite3 "${DB_FILE}" <<-EOF
	INSERT INTO benchmark_results
	(run_id, category, endpoint, requests, concurrency, time_taken, requests_per_sec, time_per_request, transfer_rate, failed_requests)
	VALUES
	(${run_id}, '${category}', '${endpoint}', ${requests}, ${concurrency}, ${time_taken}, ${rps}, ${time_per_req}, ${transfer_rate}, ${failed});
	EOF
}

# Get previous run ID
get_previous_run() {
	sqlite3 "${DB_FILE}" "SELECT run_id FROM benchmark_runs ORDER BY run_id DESC LIMIT 1 OFFSET 1;" 2>/dev/null || echo ""
}

# Get run info
# Args: run_id
get_run_info() {
	local run_id="$1"
	sqlite3 -header -column "${DB_FILE}" "SELECT * FROM benchmark_runs WHERE run_id = ${run_id};"
}

# Get results for run
# Args: run_id
get_run_results() {
	local run_id="$1"
	sqlite3 -header -column "${DB_FILE}" "SELECT category, endpoint, requests_per_sec, time_per_request, failed_requests FROM benchmark_results WHERE run_id = ${run_id} ORDER BY category, endpoint;"
}

# Compare two runs for specific endpoint
# Args: endpoint run1_id run2_id
compare_endpoint() {
	local endpoint="$1"
	local run1="$2"
	local run2="$3"

	sqlite3 -header -column "${DB_FILE}" <<-EOF
	SELECT
		'${endpoint}' as endpoint,
		r1.requests_per_sec as previous_rps,
		r2.requests_per_sec as current_rps,
		ROUND((r2.requests_per_sec - r1.requests_per_sec) / r1.requests_per_sec * 100, 1) as change_pct
	FROM benchmark_results r1
	JOIN benchmark_results r2 ON r1.endpoint = r2.endpoint
	WHERE r1.run_id = ${run1}
		AND r2.run_id = ${run2}
		AND r1.endpoint = '${endpoint}';
	EOF
}

# List recent runs
list_recent_runs() {
	local limit="${1:-10}"
	sqlite3 -header -column "${DB_FILE}" "SELECT run_id, timestamp, git_commit, build_mode, notes FROM benchmark_runs ORDER BY run_id DESC LIMIT ${limit};"
}
