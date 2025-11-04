# HBF Benchmark Skill - Quick Reference

This skill provides comprehensive performance benchmarking for HBF using Apache Bench.

## Quick Start

```bash
# Run basic benchmark (10k requests, concurrency 10)
cd /workspaces/hbf
.claude/skills/benchmark/benchmark.sh

# Run with custom parameters
.claude/skills/benchmark/benchmark.sh --requests 50000 --concurrency 50 --notes "After optimization"

# View historical results
.claude/skills/benchmark/show_results.sh

# Compare two runs
.claude/skills/benchmark/show_results.sh --compare 1 2

# Show trend for specific endpoint
.claude/skills/benchmark/show_results.sh --trend "/hello"
```

## What Gets Benchmarked

### 1. Static Assets (from latest_fs)
- `/static/style.css` - CSS file served from SQLite
- `/static/favicon.ico` - Empty file

### 2. QuickJS Runtime Routes
- `/hello` - Simple JSON response
- `/user/42` - Parameterized route
- `/echo` - Echo request details

### 3. Versioned Filesystem Operations (dev mode)
- `GET /__dev/api/files` - List all files
- `PUT /__dev/api/file?name=static/bench.txt` - Write new version

## Results Database

All results are stored in `/workspaces/hbf/.benchmark/results.db` for historical tracking.

Schema:
- `benchmark_runs` - Run metadata (timestamp, commit, notes)
- `benchmark_results` - Individual endpoint results

## Example Output

```
========================================
HBF Benchmark Runner
========================================
Requests per endpoint: 10000
Concurrency: 10
Build mode: opt

...

Category 1: Static Assets
========================================
Benchmarking: /static/style.css
  Requests/sec: 45,230
  Time/request: 0.22 ms
  Failed: 0

Category 2: QuickJS Runtime Routes
========================================
Benchmarking: /hello
  Requests/sec: 38,500
  Time/request: 0.26 ms
  Failed: 0
```

## Files

- `benchmark.sh` - Main benchmark runner
- `show_results.sh` - Results viewer and comparison tool
- `lib/db.sh` - Database operations
- `lib/server.sh` - Server lifecycle management
- `lib/parser.sh` - Apache Bench output parser
- `SKILL.md` - Complete documentation

## Tips

1. **Reproducibility**: Run on idle system with consistent parameters
2. **Warmup**: First run may be slower (cache warming)
3. **Build Mode**: Use `--build-mode opt` for release performance testing
4. **History**: Use `--notes` to document what changed between runs
5. **Comparison**: Compare similar build modes (opt vs opt, dbg vs dbg)

## Integration with Claude

Ask Claude to run benchmarks:

```
"Run the benchmark skill to measure current performance"
"Benchmark HBF and compare with previous run"
"Show me the performance trend for /hello endpoint"
```

## Prerequisites

- Apache Bench (`ab`) - Included in devcontainer
- SQLite3 CLI - Included in devcontainer
- Bazel build system - Available in workspace
- Git - For commit metadata
