---
name: add-pod
description: Create a new HBF pod with proper BUILD.bazel structure, CI/CD registration, and GitHub Actions matrix configuration. Use when adding a new pod to the multi-pod build system.
---

# Add Pod Skill

This skill guides you through creating a new HBF pod with all required configuration.

## Pod Architecture Overview

HBF uses a macro-based multi-pod build system. Each pod is:
- Self-contained (SQLite DB + JavaScript + static files)
- Manually registered in root BUILD.bazel
- Built via `pod_binary()` macro in `tools/pod_binary.bzl`
- Embedded as a C array via `db_to_c` tool

## Required Steps

When creating a new pod, you MUST complete these steps in order:

### 1. Create Pod Directory Structure

Create the following directory layout:

```
pods/<podname>/
  BUILD.bazel          # Must contain pod_db target
  hbf/                 # JavaScript source files
    server.js          # Entry point
  static/              # Static assets (HTML, CSS, etc.)
```

**BUILD.bazel Requirements:**
- Must define a `pod_db` target using `sqlar_archive` rule
- See `templates/pod_build_template.bazel` for reference

### 2. Register Pod Binary in Root BUILD.bazel

Add the pod_binary() call to `/BUILD.bazel`:

```python
pod_binary(
    name = "hbf_<podname>",
    pod = "//pods/<podname>",
    tags = ["hbf_pod"],
)
```

**Important:** The `tags = ["hbf_pod"]` is required for pod discovery.

### 3. Add Pod to CI Build Matrix

Edit `.github/workflows/pr.yml`:

Add `"<podname>"` to the `pod` matrix list in the `build-and-test` job:

```yaml
strategy:
  matrix:
    pod: ["base", "test", "<podname>"]
```

### 4. (Optional) Add Pod to Release Workflow

If this pod should be released, edit `.github/workflows/release.yml`:

Add `"<podname>"` to the `pod` matrix list in the `release-binaries` job.

## Verification Steps

After completing all steps, verify:

1. **Build succeeds:**
   ```bash
   bazel build //:hbf_<podname>
   ```

2. **Binary appears in output directory:**
   ```bash
   ls -lh bazel-bin/bin/hbf_<podname>
   ```

3. **Pod is discoverable:**
   ```bash
   bazel query 'attr(tags, "hbf_pod", kind("cc_binary", //...))'
   ```

4. **Tests pass:**
   ```bash
   bazel test //pods/<podname>:all
   ```

## Pod Naming Conventions

- Use lowercase letters and hyphens only
- Keep names short and descriptive
- Binary will be named `hbf_<podname>`
- Example: `chat` → `hbf_chat`

## Common Issues

**Symbol resolution errors:** Ensure `alwayslink = True` is set in the embedded library (handled by `pod_binary()` macro automatically).

**CI build failures:** Double-check that pod name matches exactly in all three locations (BUILD.bazel, pr.yml, release.yml).

**Missing static files:** Verify all static assets are in the `static/` directory and included in the `pod_db` target's srcs.

## Template Files

Reference templates are available in this skill's `templates/` directory:
- `pod_build_template.bazel` - Example BUILD.bazel for a new pod
- `server_template.js` - Minimal server.js entry point

## Manual Pod Registration Philosophy

Pods are manually registered (not auto-discovered) because:
- **Explicit control:** No surprises about what's being built
- **Deliberate releases:** Only explicitly listed pods are released
- **Simple and predictable:** Easy to understand what will be built in CI/CD
