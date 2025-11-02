# Multi-Pod Build System Plan

## Current State (Problems)

**BUILD.bazel**:
- Hardcoded single pod: `//pods/base:pod_db` → `//:hbf` binary
- Manual genrule for each pod would create maintenance nightmare
- No mechanism to discover pods automatically

**GitHub Actions**:
- `.github/workflows/pr.yml` - Single build only
- `.github/workflows/release.yml` - Single build only
- `.github/actions/build-full/action.yml` - Hardcoded paths for single binary

**Missing**:
- No pod discovery mechanism
- No matrix strategy for building multiple pods
- No alignment between Bazel targets and CI/CD matrices

## Goal

Create a unified multi-pod build system where:

1. **Bazel Targets**: Each pod generates two targets:
   - `//:hbf_<podname>` - Stripped release binary
   - `//:hbf_<podname>_unstripped` - Debug binary with symbols
2. **GitHub Actions Matrix**:
   - PR: Build specified pods from manually maintained list (e.g., `["base", "test"]`)
   - Release: Build specified pods from manually maintained list (e.g., `["base"]`)
3. **Consistent Naming**: Same naming convention across Bazel and CI/CD

## Proposed Architecture

### 1. Pod Structure Standard

Each pod must follow this structure:
```
pods/
  base/                    # Default/primary pod
    BUILD.bazel           # Pod build definition
    hbf/                  # JavaScript source
      server.js
    static/               # Static assets
      index.html
      style.css
  test/                    # Test/example pod (NEW)
    BUILD.bazel
    hbf/
      server.js
    static/
      index.html
  myapp/                   # Future pods...
    BUILD.bazel
    hbf/
    static/
```

### 2. Bazel Build System Changes

#### 2.1. Root BUILD.bazel Refactoring

**Current** (hardcoded):
```python
genrule(
    name = "base_pod_embedded_gen",
    srcs = ["//pods/base:pod_db"],
    ...
)
```

**Proposed** (macro-based):
```python
load("//tools:pod_binary.bzl", "pod_binary")

# Generate binaries for each pod
pod_binary(
    name = "hbf",           # Default target for base pod
    pod = "//pods/base",
    visibility = ["//visibility:public"],
)

pod_binary(
    name = "hbf_test",
    pod = "//pods/test",
    visibility = ["//visibility:public"],
)

# Add more pods as needed...
```

#### 2.2. Create tools/pod_binary.bzl Macro

New Starlark macro that generates:
1. `<name>_embedded_gen` - Convert pod DB to C source
2. `<name>_embedded` - cc_library with embedded pod
3. `<name>_core` - cc_binary with embedded pod (unstripped)
4. `<name>` - genrule that strips the binary
5. `<name>_unstripped` - alias to unstripped binary

**Key Features**:
- Automatically wires up dependencies: pod → C source → cc_library → cc_binary
- Handles stripping/unstripped variants
- Consistent naming: `//:hbf_<podname>` and `//:hbf_<podname>_unstripped`
- Output paths: `bazel-bin/bin/hbf_<podname>` and `bazel-bin/hbf/shell/hbf_<podname>`

#### 2.3. Manual Pod Management

Pods are explicitly listed in two places:
1. **Root BUILD.bazel**: Add `pod_binary()` call for each pod
2. **GitHub Actions workflows**: Add pod name to matrix list

This explicit approach provides:
- Clear visibility of what's being built
- No surprises from auto-discovery
- Deliberate control over PR and Release pod sets

### 3. GitHub Actions Matrix Strategy

#### 3.1. Update build-full Action

**Current**: `.github/actions/build-full/action.yml` - Single binary only

**Proposed**: `.github/actions/build-pod/action.yml` - Per-pod build

```yaml
name: build-pod
description: Build stripped and unstripped binaries for a single pod

inputs:
  pod-name:
    description: Name of the pod (e.g., "base", "test")
    required: true
  tag:
    description: Version tag for naming assets
    required: true
  artifact-name:
    description: Name for uploaded artifacts
    required: true

runs:
  using: composite
  steps:
    - name: Set up Bazelisk
      uses: bazelbuild/setup-bazelisk@v3

    - name: Cache Bazel directories
      uses: actions/cache@v4
      with:
        path: |
          ~/.cache/bazel
          ~/.cache/bazelisk
        key: ${{ runner.os }}-bazel-${{ inputs.pod-name }}-${{ hashFiles('**/*.bzl', '**/*.bazel') }}
        restore-keys: |
          ${{ runner.os }}-bazel-${{ inputs.pod-name }}-
          ${{ runner.os }}-bazel-

    - name: Build pod binaries
      shell: bash
      run: |
        set -euo pipefail
        POD="${{ inputs.pod-name }}"

        # Special case: "base" pod uses //:hbf target
        if [[ "$POD" == "base" ]]; then
          TARGET_NAME="hbf"
        else
          TARGET_NAME="hbf_${POD}"
        fi

        bazel build //:${TARGET_NAME}
        bazel build //:${TARGET_NAME}_unstripped

    - name: Prepare assets
      shell: bash
      run: |
        set -euo pipefail
        POD="${{ inputs.pod-name }}"
        TAG="${{ inputs.tag }}"

        # Determine target name
        if [[ "$POD" == "base" ]]; then
          TARGET_NAME="hbf"
          ASSET_BASE="hbf"
        else
          TARGET_NAME="hbf_${POD}"
          ASSET_BASE="hbf-${POD}"
        fi

        # Asset names
        NAME_STRIPPED="${ASSET_BASE}-${TAG}-linux-x86_64"
        NAME_UNSTRIPPED="${ASSET_BASE}-${TAG}-linux-x86_64-unstripped"

        # Copy binaries
        cp "bazel-bin/bin/${TARGET_NAME}" "$NAME_STRIPPED"
        cp "bazel-bin/hbf/shell/${TARGET_NAME}" "$NAME_UNSTRIPPED"

        # Generate checksums
        sha256sum "$NAME_STRIPPED" > "$NAME_STRIPPED.sha256"
        sha256sum "$NAME_UNSTRIPPED" > "$NAME_UNSTRIPPED.sha256"

    - name: Upload build assets
      uses: actions/upload-artifact@v4
      with:
        name: ${{ inputs.artifact-name }}
        path: |
          hbf*-${{ inputs.tag }}-linux-x86_64*
        if-no-files-found: error
```

#### 3.2. Update PR Workflow (Build Specified Pods)

**New**: `.github/workflows/pr.yml`

```yaml
name: PR CI

on:
  pull_request:
  push:
    branches: [ main ]

concurrency:
  group: ${{ github.workflow }}-${{ github.ref }}
  cancel-in-progress: true

jobs:
  # Build pods in parallel matrix
  build-pods:
    name: Build Pod - ${{ matrix.pod }}
    runs-on: ubuntu-latest
    timeout-minutes: 30
    strategy:
      fail-fast: false
      matrix:
        # MANUALLY MAINTAINED: Add/remove pods as needed
        pod: ["base", "test"]
    permissions:
      contents: read
    steps:
      - name: Checkout
        uses: actions/checkout@v4
        with:
          fetch-depth: 0

      - name: Build pod
        uses: ./.github/actions/build-pod
        with:
          pod-name: ${{ matrix.pod }}
          tag: ${{ github.sha }}
          artifact-name: build-${{ matrix.pod }}

  # Lint and Test (runs once, not per-pod)
  lint-test:
    name: Lint and Test
    runs-on: ubuntu-latest
    timeout-minutes: 30
    steps:
      - name: Checkout
        uses: actions/checkout@v4

      - name: Install lint dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y clang-tidy-18

      - name: Set up Bazelisk
        uses: bazelbuild/setup-bazelisk@v3

      - name: Lint
        run: bazel run //:lint

      - name: Test
        run: bazel test //...
```

#### 3.3. Update Release Workflow (Build Specified Pods)

**New**: `.github/workflows/release.yml`

```yaml
name: Release

on:
  push:
    tags:
      - 'v*.*.*'

concurrency:
  group: ${{ github.workflow }}-${{ github.ref }}
  cancel-in-progress: true

permissions:
  contents: write

jobs:
  build-release-pods:
    name: Build Pod - ${{ matrix.pod }}
    runs-on: ubuntu-latest
    timeout-minutes: 30
    strategy:
      fail-fast: true  # Fail fast on release
      matrix:
        # MANUALLY MAINTAINED: Only release-ready pods
        pod: ["base"]   # Add more as needed: ["base", "myapp"]
    steps:
      - name: Validate tag is semver vMAJ.MIN.PATCH
        run: |
          TAG="${GITHUB_REF_NAME}"
          if [[ ! "$TAG" =~ ^v[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
            echo "Tag $TAG does not match vMAJ.MIN.PATCH" >&2
            exit 1
          fi

      - name: Checkout
        uses: actions/checkout@v4
        with:
          fetch-depth: 0

      - name: Build pod
        uses: ./.github/actions/build-pod
        with:
          pod-name: ${{ matrix.pod }}
          tag: ${{ github.ref_name }}
          artifact-name: build-${{ matrix.pod }}

  publish-release:
    name: Publish GitHub Release
    needs: build-release-pods
    runs-on: ubuntu-latest
    steps:
      - name: Download all artifacts
        uses: actions/download-artifact@v4
        with:
          path: artifacts/

      - name: Flatten artifacts
        run: |
          find artifacts/ -type f -exec mv {} . \;

      - name: Publish GitHub Release
        uses: softprops/action-gh-release@v2
        with:
          tag_name: ${{ github.ref_name }}
          files: |
            hbf*-${{ github.ref_name }}-linux-x86_64*
          draft: false
          prerelease: false
        env:
          GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
```

### 4. Implementation Plan

#### Phase 1: Foundation (Create Infrastructure)

1. **Create `tools/pod_binary.bzl` macro**
   - Define `pod_binary()` macro
   - Wire up: pod_db → C source → cc_library → cc_binary → strip
   - Handle naming: `hbf_<podname>` and `hbf_<podname>_unstripped`

2. **Refactor root `BUILD.bazel`**
   - Replace hardcoded genrules with `pod_binary()` calls
   - Start with just base pod: `pod_binary(name = "hbf", pod = "//pods/base")`
   - Test: `bazel build //:hbf` and `bazel build //:hbf_unstripped` should work

#### Phase 2: Add Test Pod (Validate System)

3. **Duplicate `pods/base` → `pods/test`**
   ```bash
   cp -r pods/base pods/test
   ```

4. **Customize pods/test**
   - Update `pods/test/hbf/server.js` (change title/content to distinguish from base)
   - Update `pods/test/static/index.html` (add "TEST POD" banner)
   - Keep same BUILD.bazel structure

5. **Add test pod to root BUILD.bazel**
   ```python
   pod_binary(
       name = "hbf_test",
       pod = "//pods/test",
   )
   ```

6. **Test local builds**
   ```bash
   bazel build //:hbf         # Base pod (default)
   bazel build //:hbf_test    # Test pod
   bazel run //:hbf_test      # Should serve on port 5309
   ```

#### Phase 3: GitHub Actions Integration

7. **Create `.github/actions/build-pod/action.yml`**
   - Implement per-pod build logic
   - Handle naming conventions
   - Generate artifacts with checksums

8. **Update `.github/workflows/pr.yml`**
   - Add `build-pods` matrix job with `pod: ["base", "test"]`
   - Keep `lint-test` job separate

9. **Update `.github/workflows/release.yml`**
   - Add `build-release-pods` matrix job with `pod: ["base"]`
   - Add `publish-release` job to aggregate artifacts

10. **Remove old `.github/actions/build-full/action.yml`**
    - Deprecated by new build-pod action

#### Phase 4: Validation

11. **Test PR workflow**
    - Create PR with test changes
    - Verify all pods build in matrix
    - Check artifacts are uploaded correctly

12. **Test Release workflow** (on test tag)
    - Create test tag: `git tag v0.99.0-test && git push --tags`
    - Verify only "base" pod is built
    - Verify release assets are correct

13. **Update documentation**
    - Update `CLAUDE.md` with new build commands
    - Document how to add new pods
    - Document release pod selection process

### 5. Build Target Naming Convention

| Pod Name | Stripped Target | Unstripped Target | Binary Path (stripped) | Binary Path (unstripped) |
|----------|----------------|-------------------|------------------------|--------------------------|
| base     | `//:hbf`       | `//:hbf_unstripped` | `bazel-bin/bin/hbf` | `bazel-bin/hbf/shell/hbf` |
| test     | `//:hbf_test`  | `//:hbf_test_unstripped` | `bazel-bin/bin/hbf_test` | `bazel-bin/hbf/shell/hbf_test` |
| myapp    | `//:hbf_myapp` | `//:hbf_myapp_unstripped` | `bazel-bin/bin/hbf_myapp` | `bazel-bin/hbf/shell/hbf_myapp` |

### 6. Release Asset Naming Convention

| Pod Name | Tag | Stripped Asset | Unstripped Asset |
|----------|-----|----------------|------------------|
| base     | v1.0.0 | `hbf-v1.0.0-linux-x86_64` | `hbf-v1.0.0-linux-x86_64-unstripped` |
| test     | v1.0.0 | `hbf-test-v1.0.0-linux-x86_64` | `hbf-test-v1.0.0-linux-x86_64-unstripped` |
| myapp    | v1.0.0 | `hbf-myapp-v1.0.0-linux-x86_64` | `hbf-myapp-v1.0.0-linux-x86_64-unstripped` |

Each asset has accompanying `.sha256` checksum file.

### 7. Key Design Decisions

**Why manual pod lists instead of auto-discovery?**
- **Explicit is better than implicit**: No surprises about what's being built
- **Bazel alignment**: Same manual approach in BUILD.bazel and GitHub Actions
- **Deliberate control**: Adding a pod requires updating two files (BUILD.bazel + workflow)
- **Release safety**: Only pods you explicitly list get released

**Why separate stripped/unstripped targets?**
- Stripped: Production deployment (5.3 MB)
- Unstripped: Debugging, coredump analysis (5.4 MB)
- Developers can choose which to use

**Why base pod gets special name `//:hbf`?**
- Backward compatibility
- Default target: `bazel build //:hbf` "just works"
- Aligns with project convention

### 8. Success Criteria

✅ `bazel build //:hbf` builds base pod (backward compatible)
✅ `bazel build //:hbf_test` builds test pod
✅ PR workflow discovers and builds all pods automatically
✅ Release workflow builds only explicit list of pods
✅ Each pod generates 4 assets (stripped + unstripped + 2 checksums)
✅ Asset names distinguish between pods
✅ Local development: `bazel run //:hbf_test` launches test pod
✅ No code changes needed in hbf/ core (C code) - only build system

---

## Simplifications from Manual Pod Lists

By using manual pod lists instead of auto-discovery, we:

✅ **Removed** `tools/list_pods.sh` script
✅ **Removed** `discover-pods` job from PR workflow
✅ **Simplified** GitHub Actions - just static matrices
✅ **Reduced** implementation steps (13 instead of 14)
✅ **Improved** clarity - what's built is obvious in the workflow file

Adding a new pod now requires:
1. Create `pods/<name>/` directory with BUILD.bazel
2. Add `pod_binary(name = "hbf_<name>", pod = "//pods/<name>")` to root BUILD.bazel
3. Add `"<name>"` to matrix pod list in `.github/workflows/pr.yml`
4. (Optional) Add `"<name>"` to matrix pod list in `.github/workflows/release.yml`

---

## Next Steps

1. Review this plan
2. Start with Phase 1: Create `tools/pod_binary.bzl` macro
3. Incrementally validate each phase before moving to next
4. Test thoroughly before merging to main
