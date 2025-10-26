# HBF Spec — Next Steps (Migration Plan)

This document translates the deltas in `DOCS/hbf_spec.md` into concrete
implementation steps with scoped changes, acceptance criteria, and testing
guidance. The primary goal is to align the code with the fs_db-as-template
design: fs_db remains private to the DB module and the runtime uses only the
main database.

## Goals

- Keep fs_db strictly private to the DB module.
- Hydrate (create/recreate) the main DB from the embedded fs_db template when
  needed.
- Ensure all runtime asset reads (e.g., `hbf/server.js`, `static/*`) go through
  the main DB’s SQLAR.
- Remove `fs_db` from public APIs and server structures.
- Preserve existing behavior and integration tests while migrating.

## Targeted API Changes

1) DB module (internal/db/db.h, db.c)
- Keep existing `hbf_db_init(int inmem, sqlite3 **db, sqlite3 **fs_db)`
  signature only until migration is complete, then collapse to `hbf_db_init(int
  inmem, sqlite3 **db)` (returning only main db).
- New helpers targeting main DB SQLAR:
  - `int hbf_db_read_file_from_main(sqlite3 *db, const char *path, unsigned char **data, size_t *size);`
  - `int hbf_db_file_exists_in_main(sqlite3 *db, const char *path);`
- Hydration integrity check:
  - `int hbf_db_hydrate_if_needed(sqlite3 *db);` — Verify presence of required SQLAR content; if missing, rehydrate from embedded template.
- Implementation notes:
  - fs_db remains static/private in db.c; do not expose its pointer.
  - On init: open/create main DB → call `hbf_db_hydrate_if_needed(db)` → return `db` only.

2) HTTP server API (internal/http/server.h/.c)
- Change `hbf_server_create(int port, sqlite3 *db /*, sqlite3 *fs_db*/)` → remove fs_db parameter.
- Update `hbf_server_t` to store only `sqlite3 *db`.

3) Handlers
- Static handler: replace any `hbf_db_read_file(server->fs_db, ...)` with `hbf_db_read_file_from_main(server->db, ...)`.
- JS handler: replace `hbf_db_read_file(server->fs_db, "hbf/server.js", ...)` with main-DB read helper and keep per-request QuickJS lifecycle untouched.

## Step-by-Step Migration

Phase 1 — Add new DB helpers (non-breaking)
- Implement `hbf_db_read_file_from_main`, `hbf_db_file_exists_in_main`, and `hbf_db_hydrate_if_needed` in db.c.
- Wire hydration into `hbf_db_init` (behind the scenes) so main DB is always ready for runtime asset reads.
- Acceptance criteria:
  - Build succeeds; existing tests pass.

Phase 2 — Refactor server and handlers to main DB only
- Change `hbf_server_t` to hold only `db`.
- Update `hbf_server_create` signature and all call sites.
- Switch static and JS handlers to use main DB read helpers.
- Acceptance criteria:
  - `tools/integration_test.sh` passes fully.
  - No references to `fs_db` outside the DB module.

Phase 3 — Contract cleanup
- Remove `fs_db` from public DB headers and public targets.
- Consider collapsing `hbf_db_init` to return only `db`.
- Update BUILD files and includes as needed.
- Acceptance criteria:
  - Grep for `fs_db` shows usage only within `internal/db/` implementation.

## Testing and Validation

- Unit tests:
  - DB initialization with and without existing `hbf.db`.
  - Hydration when SQLAR table is missing.
  - Read helpers: positive/negative lookups for `hbf/server.js`, `static/index.html`.
- Integration tests:
  - Run `./tools/integration_test.sh` (should remain green).
- Manual checks:
  - Start server and verify startup URL and routes function.

## Backward Compatibility

- External behavior remains unchanged: endpoints, JSON shapes, headers, and homepage.
- CLI options unchanged.
- Internal-only API changes; no user-facing API.

## Risks and Mitigations

- Risk: Missing asset(s) after hydration refactor.
  - Mitigation: Add hydration integrity checks for required paths (e.g., `hbf/server.js`, `static/index.html`).
- Risk: Divergence between embedded template and main DB contents.
  - Mitigation: Only hydrate when main DB is missing baseline content; preserve user content otherwise.
- Risk: Wide refactor touching HTTP, DB, and handlers.
  - Mitigation: Stage in phases, run integration tests at each step.

## Work Items Checklist

- DB module
  - [ ] Implement `hbf_db_read_file_from_main` and `hbf_db_file_exists_in_main`
  - [ ] Implement `hbf_db_hydrate_if_needed` and call during init
  - [ ] Keep fs_db private in db.c
- HTTP
  - [ ] Remove fs_db from `hbf_server_t` and constructors
  - [ ] Update static handler to main DB helpers
  - [ ] Update JS handler to main DB helpers
- Cleanup
  - [ ] Remove fs_db from public headers
  - [ ] Optional: simplify `hbf_db_init` signature

## Completion Criteria

- No references to `fs_db` outside `internal/db/` implementation files.
- All endpoint and integration tests pass.
- Spec and code match the fs_db-as-template model documented in `DOCS/hbf_spec.md`.
