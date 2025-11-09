#!/bin/bash
# SPDX-License-Identifier: MIT
# Server lifecycle management for HBF benchmarking

SERVER_PORT="${SERVER_PORT:-5309}"
SERVER_PID_FILE="/tmp/hbf_bench_server.pid"
BINARY_PATH="${BINARY_PATH:-bazel-bin/bin/hbf}"

# Start HBF server in background
start_server() {
	local port="$1"
	local pid_file="$2"

	if [ -f "${pid_file}" ]; then
		local old_pid
		old_pid=$(cat "${pid_file}")
		if kill -0 "${old_pid}" 2>/dev/null; then
			echo "Server already running with PID ${old_pid}"
			return 0
		fi
		rm -f "${pid_file}"
	fi

	local cmd="${BINARY_PATH} --port ${port} --log-level error --inmem"

	echo "Starting server: ${cmd}"
	${cmd} &
	local pid=$!
	echo "${pid}" > "${pid_file}"

	# Wait for server to be ready
	local max_wait=30
	local waited=0
	while [ ${waited} -lt ${max_wait} ]; do
		if curl -s "http://localhost:${port}/hello" >/dev/null 2>&1; then
			echo "Server ready on port ${port} (PID: ${pid})"
			return 0
		fi
		sleep 1
		waited=$((waited + 1))
	done

	echo "ERROR: Server failed to start within ${max_wait} seconds"
	stop_server "${pid_file}"
	return 1
}

# Stop HBF server
stop_server() {
	local pid_file="$1"

	if [ ! -f "${pid_file}" ]; then
		return 0
	fi

	local pid
	pid=$(cat "${pid_file}")
	if kill -0 "${pid}" 2>/dev/null; then
		echo "Stopping server (PID: ${pid})"
		kill "${pid}"

		# Wait for graceful shutdown
		local max_wait=10
		local waited=0
		while kill -0 "${pid}" 2>/dev/null && [ ${waited} -lt ${max_wait} ]; do
			sleep 1
			waited=$((waited + 1))
		done

		# Force kill if still running
		if kill -0 "${pid}" 2>/dev/null; then
			echo "Force killing server (PID: ${pid})"
			kill -9 "${pid}"
		fi
	fi

	rm -f "${pid_file}"
}

# Stop all benchmark servers
stop_all_servers() {
	stop_server "${SERVER_PID_FILE}"
}

# Health check
health_check() {
	local port="$1"
	curl -s "http://localhost:${port}/hello" >/dev/null 2>&1
	return $?
}
