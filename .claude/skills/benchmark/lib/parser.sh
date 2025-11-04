#!/bin/bash
# SPDX-License-Identifier: MIT
# Apache Bench output parser for HBF benchmarking

# Parse ab output and extract metrics
# Args: ab_output_file
parse_ab_output() {
	local output_file="$1"

	if [ ! -f "${output_file}" ]; then
		echo "ERROR: Output file not found: ${output_file}" >&2
		return 1
	fi

	# Extract key metrics using awk
	local time_taken
	local requests_per_sec
	local time_per_request
	local transfer_rate
	local failed_requests

	time_taken=$(grep "Time taken for tests:" "${output_file}" | awk '{print $5}')
	requests_per_sec=$(grep "Requests per second:" "${output_file}" | awk '{print $4}')
	time_per_request=$(grep "Time per request:" "${output_file}" | head -n1 | awk '{print $4}')
	transfer_rate=$(grep "Transfer rate:" "${output_file}" | awk '{print $3}')
	failed_requests=$(grep "Failed requests:" "${output_file}" | awk '{print $3}')

	# Return as space-separated values
	echo "${time_taken} ${requests_per_sec} ${time_per_request} ${transfer_rate} ${failed_requests}"
}

# Run ab benchmark and return parsed metrics
# Args: url requests concurrency output_file
run_ab_benchmark() {
	local url="$1"
	local requests="$2"
	local concurrency="$3"
	local output_file="$4"

	# Run ab with quiet mode, save output
	ab -n "${requests}" -c "${concurrency}" -q "${url}" > "${output_file}" 2>&1

	if [ $? -ne 0 ]; then
		echo "ERROR: ab failed for ${url}" >&2
		cat "${output_file}" >&2
		return 1
	fi

	# Parse and return metrics
	parse_ab_output "${output_file}"
}

# Run ab benchmark for PUT request with body
# Args: url requests concurrency body_file output_file
run_ab_put_benchmark() {
	local url="$1"
	local requests="$2"
	local concurrency="$3"
	local body_file="$4"
	local output_file="$5"

	# Run ab with PUT method
	ab -n "${requests}" -c "${concurrency}" -p "${body_file}" -T "text/plain" -q "${url}" > "${output_file}" 2>&1

	if [ $? -ne 0 ]; then
		echo "ERROR: ab failed for PUT ${url}" >&2
		cat "${output_file}" >&2
		return 1
	fi

	# Parse and return metrics
	parse_ab_output "${output_file}"
}

# Format requests per second with comma separator
format_rps() {
	local rps="$1"
	printf "%'.0f" "${rps}"
}

# Calculate percentage change
calc_pct_change() {
	local old_val="$1"
	local new_val="$2"

	if [ -z "${old_val}" ] || [ "${old_val}" = "0" ]; then
		echo "N/A"
		return
	fi

	local change
	change=$(echo "scale=1; (${new_val} - ${old_val}) / ${old_val} * 100" | bc)

	if (( $(echo "${change} > 0" | bc -l) )); then
		echo "+${change}%"
	else
		echo "${change}%"
	fi
}
