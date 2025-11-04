#!/bin/bash
# SPDX-License-Identifier: MIT
# Historical benchmark results viewer

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/lib/db.sh"

# Parse command line arguments
MODE="list"
RUN1=""
RUN2=""
ENDPOINT=""
LIMIT=10

while [[ $# -gt 0 ]]; do
	case $1 in
		--compare)
			MODE="compare"
			RUN1="$2"
			RUN2="$3"
			shift 3
			;;
		--trend)
			MODE="trend"
			ENDPOINT="$2"
			shift 2
			;;
		--limit)
			LIMIT="$2"
			shift 2
			;;
		--help)
			echo "Usage: $0 [options]"
			echo "Options:"
			echo "  --compare RUN1 RUN2  Compare two specific runs"
			echo "  --trend ENDPOINT     Show trend for specific endpoint"
			echo "  --limit N            Limit number of results (default: 10)"
			echo "  --help               Show this help message"
			exit 0
			;;
		*)
			echo "Unknown option: $1"
			exit 1
			;;
	esac
done

echo "========================================"
echo "HBF Benchmark Results"
echo "========================================"
echo ""

case "${MODE}" in
	list)
		echo "Recent benchmark runs (last ${LIMIT}):"
		echo ""
		list_recent_runs "${LIMIT}"
		echo ""
		echo "Use --compare to compare two runs"
		echo "Use --trend to see performance trend for an endpoint"
		;;

	compare)
		if [ -z "${RUN1}" ] || [ -z "${RUN2}" ]; then
			echo "ERROR: Both run IDs required for comparison"
			exit 1
		fi

		echo "Comparing Run ${RUN1} vs Run ${RUN2}"
		echo ""

		echo "Run ${RUN1} Info:"
		get_run_info "${RUN1}"
		echo ""

		echo "Run ${RUN2} Info:"
		get_run_info "${RUN2}"
		echo ""

		echo "Performance Comparison:"
		echo ""

		sqlite3 -header -column "${DB_FILE}" <<-EOF
		SELECT
			r1.endpoint,
			ROUND(r1.requests_per_sec, 0) as run${RUN1}_rps,
			ROUND(r2.requests_per_sec, 0) as run${RUN2}_rps,
			ROUND((r2.requests_per_sec - r1.requests_per_sec) / r1.requests_per_sec * 100, 1) as change_pct
		FROM benchmark_results r1
		JOIN benchmark_results r2 ON r1.endpoint = r2.endpoint
		WHERE r1.run_id = ${RUN1}
			AND r2.run_id = ${RUN2}
		ORDER BY r1.category, r1.endpoint;
		EOF
		;;

	trend)
		if [ -z "${ENDPOINT}" ]; then
			echo "ERROR: Endpoint required for trend analysis"
			exit 1
		fi

		echo "Performance trend for: ${ENDPOINT}"
		echo ""

		sqlite3 -header -column "${DB_FILE}" <<-EOF
		SELECT
			br.run_id,
			br.timestamp,
			br.git_commit,
			ROUND(r.requests_per_sec, 0) as rps,
			ROUND(r.time_per_request, 2) as time_per_req_ms
		FROM benchmark_results r
		JOIN benchmark_runs br ON r.run_id = br.run_id
		WHERE r.endpoint = '${ENDPOINT}'
		ORDER BY br.run_id DESC
		LIMIT ${LIMIT};
		EOF

		echo ""
		echo "Statistics:"
		sqlite3 -header -column "${DB_FILE}" <<-EOF
		SELECT
			COUNT(*) as num_runs,
			ROUND(MIN(requests_per_sec), 0) as min_rps,
			ROUND(MAX(requests_per_sec), 0) as max_rps,
			ROUND(AVG(requests_per_sec), 0) as avg_rps
		FROM benchmark_results
		WHERE endpoint = '${ENDPOINT}';
		EOF
		;;
esac

echo ""
