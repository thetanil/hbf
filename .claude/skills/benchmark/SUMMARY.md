# HBF Benchmark Skill - Summary

## Installation Complete

The HBF benchmark skill has been successfully created and tested. It provides comprehensive performance testing using Apache Bench with historical data tracking.

## What Was Created

### Core Files

```
.claude/skills/benchmark/
├── SKILL.md              - Complete skill documentation
├── README.md             - Quick reference guide
├── SUMMARY.md            - This file
├── benchmark.sh          - Main benchmark runner (executable)
├── show_results.sh       - Results viewer (executable)
└── lib/
    ├── db.sh             - Database operations
    ├── server.sh         - Server lifecycle management
    └── parser.sh         - Apache Bench output parser
```

### Data Storage

```
/workspaces/hbf/.benchmark/
└── results.db            - SQLite database with historical results
```

## Features

### Benchmark Categories

1. **Static Assets** - Tests serving from `latest_files` view
   - `/static/style.css` (~2,000 req/sec)
   - `/static/favicon.ico` (~21,000 req/sec)

2. **QuickJS Runtime** - Tests JavaScript execution
   - `/hello` (~250 req/sec)
   - `/user/42` (~235 req/sec)
   - `/echo` (~235 req/sec)

3. **Versioned Filesystem** - Tests writes to file_versions
   - `PUT /__dev/api/file` (~240 req/sec)
   - `GET /__dev/api/files` (needs investigation - crashes under load)

### Historical Tracking

- All results stored in SQLite database
- Compare runs over time
- Track performance trends per endpoint
- Includes git metadata (commit, branch)
- Support for notes on each run

## Usage Examples

```bash
# Basic benchmark (10k requests, concurrency 10)
.claude/skills/benchmark/benchmark.sh

# Custom parameters
.claude/skills/benchmark/benchmark.sh \
  --requests 50000 \
  --concurrency 50 \
  --notes "After optimization"

# View results
.claude/skills/benchmark/show_results.sh

# Compare runs
.claude/skills/benchmark/show_results.sh --compare 3 4

# Show trend
.claude/skills/benchmark/show_results.sh --trend "/hello"
```

## Integration with Claude Code

The skill is registered and can be invoked by asking Claude:

```
"Run the benchmark skill"
"Benchmark HBF performance"
"Show me performance trends"
```

## Baseline Performance (Optimized Build)

Based on initial testing with 5,000 requests at concurrency 10:

| Category | Endpoint | Req/sec | Time/req |
|----------|----------|---------|----------|
| Static | /static/favicon.ico | 21,793 | 0.46 ms |
| Static | /static/style.css | 1,988 | 2.5 ms |
| Runtime | /hello | 255 | 39 ms |
| Runtime | /user/42 | 235 | 43 ms |
| Runtime | /echo | 235 | 43 ms |
| FS Write | PUT file | 241 | 21 ms |

## Known Issues

### 1. Segfault on GET /__dev/api/files

**Status:** Discovered during benchmark testing
**Description:** Server crashes with segmentation fault when handling `GET /__dev/api/files` under load (5000 requests, concurrency 10)
**Reproduction:** Run benchmark with default parameters
**Impact:** High - dev mode endpoint crashes under concurrent load
**Location:** Likely in `hbf/db/overlay_fs.c` or query result handling

**Error output:**
```
apr_socket_recv: Connection reset by peer (104)
Total of 1691 requests completed
Segmentation fault (core dumped)
```

**Next steps:**
- Run with debug build and gdb to get stack trace
- Check memory management in file listing code
- Verify SQLite query result handling for concurrent access

## Performance Notes

1. **Static assets** perform very well (20k+ req/sec for empty file)
2. **QuickJS routes** are slower (~250 req/sec) due to JavaScript execution overhead
3. **FS writes** are limited by SQLite WAL journal (~240 req/sec)
4. **In-memory mode** used for benchmarking to avoid database conflicts

## Design Decisions

### Why --inmem Flag?

The benchmark uses `--inmem` flag to ensure:
- No database conflicts between multiple server instances
- Clean state for each benchmark run
- Consistent performance measurements
- No disk I/O interference

### Why Two Server Instances?

- Port 5309: Normal mode for static and runtime tests
- Port 5310: Dev mode for filesystem write tests
- Separates concerns and avoids permission issues

## Reproducibility

For consistent results:
1. Run on idle system
2. Use same build mode (opt)
3. Use same request/concurrency parameters
4. Close other applications
5. Use `--notes` to document changes

## Future Enhancements

Potential improvements:
- [ ] Add percentile latency (p50, p95, p99)
- [ ] Test different concurrency levels automatically
- [ ] Add memory profiling
- [ ] Support custom pod benchmarking
- [ ] Add warmup phase
- [ ] Export to CSV/JSON
- [ ] Fix segfault in GET /__dev/api/files

## Testing Status

✅ Script structure created
✅ Database operations working
✅ Server management working
✅ Apache Bench integration working
✅ Results parsing working
✅ Historical storage working
✅ Results viewer working
✅ Trend analysis working
✅ Comparison working
⚠️  Found bug in HBF (segfault under load)

## Conclusion

The HBF benchmark skill is **ready for use** and provides:
- Reproducible performance testing
- Historical data tracking
- Multiple benchmark categories
- Easy-to-use CLI interface
- Integration with Claude Code

It has already proven valuable by uncovering a crash bug in the dev mode file listing endpoint.
