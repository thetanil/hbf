#!/bin/bash
# hbf_simple_benchmark.sh: Benchmark HBF Simple server throughput (requests/sec)

NUM_CLIENTS=8
NUM_REQUESTS=1000
PORT=$((8000 + RANDOM % 1000))
SERVER_URL="http://127.0.0.1:$PORT/health"

if [ "$1" = "--help" ]; then
    echo "Usage: $0 [num_clients] [num_requests]"
    echo "Defaults: num_clients=$NUM_CLIENTS num_requests=$NUM_REQUESTS"
    exit 0
fi

if [ -n "$1" ]; then NUM_CLIENTS="$1"; fi
if [ -n "$2" ]; then NUM_REQUESTS="$2"; fi

echo "Building //:hbf (optimized, stripped) and starting server on port $PORT..."
bazel build //:hbf > /dev/null 2>&1

bazel-bin/bin/hbf --port $PORT &
SERVER_PID=$!

# Wait for server to be ready
for i in {1..30}; do
    curl -s "$SERVER_URL" > /dev/null && break
    sleep 0.2
done

if ! curl -s "$SERVER_URL" > /dev/null; then
    echo "Server did not start on $PORT. Exiting."
    kill $SERVER_PID 2>/dev/null
    exit 1
fi

echo "Benchmarking $SERVER_URL with $NUM_CLIENTS clients, $NUM_REQUESTS requests each..."

start_time=$(date +%s.%N)

run_client() {
    for ((i=0;i<$NUM_REQUESTS;i++)); do
        curl -sS -o /dev/null "$SERVER_URL" 2>/dev/null
    done
}

for ((c=0;c<$NUM_CLIENTS;c++)); do
    run_client &
    pids[$c]=$!
    sleep 0.01
done

for pid in ${pids[@]}; do
    wait $pid
done

end_time=$(date +%s.%N)

total_requests=$((NUM_CLIENTS * NUM_REQUESTS))

# Use awk for floating-point arithmetic instead of bc
total_time=$(awk "BEGIN {print $end_time - $start_time}")
rps=$(awk "BEGIN {print $total_requests / $total_time}")

printf "\nTotal requests: %d\n" "$total_requests"
printf "Total time: %.2f seconds\n" "$total_time"
printf "Requests per second: %.2f\n" "$rps"

echo "Stopping hbf server (PID $SERVER_PID)..."
kill $SERVER_PID 2>/dev/null
