# HBF Benchmark Skill - Usage Examples

## Quick Start

### Run a basic benchmark

```bash
cd /workspaces/hbf
.claude/skills/benchmark/benchmark.sh
```

This runs 10,000 requests at concurrency 10 across all benchmark categories.

### View recent results

```bash
.claude/skills/benchmark/show_results.sh
```

Output:
```
========================================
HBF Benchmark Results
========================================

Recent benchmark runs (last 10):

run_id  timestamp            git_commit  build_mode  notes
------  -------------------  ----------  ----------  ----------------
4       2025-11-04 08:07:42  5f8e104     opt         Baseline performance test
3       2025-11-04 08:07:14  5f8e104     opt         Initial test run
```

## Common Workflows

### 1. Before/After Optimization

```bash
# Baseline before changes
.claude/skills/benchmark/benchmark.sh --notes "Before optimization"

# Make your code changes
# ... edit files ...

# Rebuild and retest
.claude/skills/benchmark/benchmark.sh --notes "After optimization"

# Compare results
.claude/skills/benchmark/show_results.sh --compare $(sqlite3 .benchmark/results.db "SELECT run_id FROM benchmark_runs ORDER BY run_id DESC LIMIT 1 OFFSET 1") $(sqlite3 .benchmark/results.db "SELECT run_id FROM benchmark_runs ORDER BY run_id DESC LIMIT 1")
```

### 2. Track Specific Endpoint Performance

```bash
# Run several benchmarks over time
.claude/skills/benchmark/benchmark.sh --notes "v1.0"
# ... make changes ...
.claude/skills/benchmark/benchmark.sh --notes "v1.1"
# ... make changes ...
.claude/skills/benchmark/benchmark.sh --notes "v1.2"

# View trend for a specific endpoint
.claude/skills/benchmark/show_results.sh --trend "/hello"
```

Output:
```
Performance trend for: /hello

run_id  timestamp            git_commit  rps    time_per_req_ms
------  -------------------  ----------  -----  ---------------
4       2025-11-04 08:07:42  5f8e104     255.0  39.3
3       2025-11-04 08:07:14  5f8e104     273.0  18.3

Statistics:
num_runs  min_rps  max_rps  avg_rps
--------  -------  -------  -------
2         255.0    273.0    264.0
```

### 3. High-Load Stress Test

```bash
# Test with high concurrency
.claude/skills/benchmark/benchmark.sh \
  --requests 100000 \
  --concurrency 100 \
  --notes "Stress test - 100k requests"
```

### 4. Debug Build Performance

```bash
# Test debug build
.claude/skills/benchmark/benchmark.sh \
  --build-mode dbg \
  --requests 1000 \
  --notes "Debug build test"
```

### 5. Quick Sanity Check

```bash
# Fast test with fewer requests
.claude/skills/benchmark/benchmark.sh \
  --requests 1000 \
  --concurrency 5 \
  --notes "Quick sanity check"
```

## Advanced Usage

### Compare Two Specific Runs

```bash
# Compare run 3 vs run 5
.claude/skills/benchmark/show_results.sh --compare 3 5
```

Output shows percentage change for each endpoint:
```
Comparing Run 3 vs Run 5

Performance Comparison:

endpoint                run3_rps  run5_rps  change_pct
----------------------  --------  --------  ----------
/hello                  273       255       -6.6%
/static/style.css       1886      1988      +5.4%
```

### Custom Trend Analysis

```bash
# Show last 20 runs for specific endpoint
.claude/skills/benchmark/show_results.sh --trend "/static/style.css" --limit 20
```

### Query Database Directly

```bash
# Custom SQL queries
sqlite3 /workspaces/hbf/.benchmark/results.db "
SELECT
  br.timestamp,
  r.endpoint,
  r.requests_per_sec,
  r.failed_requests
FROM benchmark_results r
JOIN benchmark_runs br ON r.run_id = br.run_id
WHERE r.category = 'runtime'
  AND br.build_mode = 'opt'
ORDER BY br.timestamp DESC
LIMIT 10;"
```

### Export Results

```bash
# Export to CSV
sqlite3 -header -csv /workspaces/hbf/.benchmark/results.db "
SELECT
  br.run_id,
  br.timestamp,
  br.git_commit,
  br.build_mode,
  r.category,
  r.endpoint,
  r.requests_per_sec,
  r.time_per_request
FROM benchmark_results r
JOIN benchmark_runs br ON r.run_id = br.run_id
ORDER BY br.timestamp DESC;" > benchmark_export.csv
```

## Integration with Git Workflow

### Pre-Commit Benchmark

```bash
#!/bin/bash
# .git/hooks/pre-push

echo "Running performance benchmark..."
.claude/skills/benchmark/benchmark.sh \
  --requests 5000 \
  --notes "Pre-push check" \
  --build-mode opt

# Check for significant regressions
# (implement your own logic here)
```

### CI/CD Integration

```yaml
# .github/workflows/benchmark.yml
name: Performance Benchmark

on:
  push:
    branches: [main]

jobs:
  benchmark:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Run Benchmark
        run: |
          .claude/skills/benchmark/benchmark.sh \
            --requests 10000 \
            --notes "CI benchmark on ${GITHUB_SHA}"
      - name: Upload Results
        uses: actions/upload-artifact@v2
        with:
          name: benchmark-results
          path: .benchmark/results.db
```

## Asking Claude to Run Benchmarks

You can ask Claude Code to run benchmarks for you:

```
"Run the benchmark skill to test current performance"
```

```
"Benchmark HBF with 50k requests and show me the results"
```

```
"Show me the performance trend for the /hello endpoint"
```

```
"Compare the last two benchmark runs"
```

## Tips and Best Practices

### 1. Run Multiple Times

For more reliable results, run the benchmark multiple times and average:

```bash
for i in {1..3}; do
  .claude/skills/benchmark/benchmark.sh --notes "Run $i of 3"
  sleep 5
done
```

### 2. Document Your Changes

Always use `--notes` to document what changed:

```bash
.claude/skills/benchmark/benchmark.sh \
  --notes "Added caching to QuickJS module loader"
```

### 3. Compare Similar Builds

Only compare runs with the same build mode:
- opt vs opt (production performance)
- dbg vs dbg (debug overhead comparison)

### 4. Check System Load

Ensure system is idle before benchmarking:

```bash
# Check load
uptime

# Check CPU usage
top -bn1 | grep "Cpu(s)"

# Then run benchmark
.claude/skills/benchmark/benchmark.sh
```

### 5. Watch for Failed Requests

Always check the `failed_requests` column - it should be 0:

```bash
sqlite3 /workspaces/hbf/.benchmark/results.db "
SELECT run_id, endpoint, failed_requests
FROM benchmark_results
WHERE failed_requests > 0;"
```

## Troubleshooting

### Server Won't Start

If server fails to start:
```bash
# Check if port is in use
lsof -i :5309

# Kill any stray processes
pkill -f hbf

# Try again
.claude/skills/benchmark/benchmark.sh
```

### Benchmark Takes Too Long

Reduce request count:
```bash
.claude/skills/benchmark/benchmark.sh --requests 1000
```

### Results Look Wrong

1. Check build mode (dbg is much slower than opt)
2. Check system load
3. Run multiple times to verify consistency
4. Compare with previous baseline

## Performance Expectations

### Typical Performance (opt build, idle system)

| Endpoint | Expected Req/sec | Notes |
|----------|------------------|-------|
| /static/favicon.ico | 20,000+ | Empty file, fastest |
| /static/style.css | 2,000+ | Small file |
| /hello | 250+ | Simple JSON |
| /user/:id | 230+ | JSON with param |
| PUT file | 200-300 | SQLite write |

If you see significantly lower numbers:
- Check build mode (should be `opt`)
- Check system load
- Check for background processes
- Verify disk isn't swapping

## More Information

- Full documentation: `SKILL.md`
- Quick reference: `README.md`
- Implementation summary: `SUMMARY.md`
