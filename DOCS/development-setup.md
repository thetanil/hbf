# Development Setup

This document describes the development environment setup for HBF.

## System Requirements

- **Bazel 8.x** - Build system
- **Git** - Version control
- **GCC or Clang** - C compiler (musl toolchain provided by Bazel)

## Required System Packages

### LLVM Tools (for linting and coverage)

**If using the devcontainer**: These are already installed in `.devcontainer/Dockerfile`.

**If setting up manually**:
```bash
sudo apt-get update
sudo apt-get install -y clang-tidy-18 llvm-18
```

**Why these are needed:**
- `clang-tidy-18`: Static analysis and linting (matches our coding standards)
- `llvm-18`: Provides `llvm-cov-18` for code coverage reporting

**Note**: We use system LLVM tools instead of Bazel's `toolchains_llvm` due to library compatibility issues (libtinfo.so.5).

## Bazel Configuration

The project uses **bzlmod** (MODULE.bazel) instead of WORKSPACE.

### Key Dependencies

- **rules_cc**: C/C++ compilation rules
- **platforms**: Platform definitions
- **toolchains_musl**: Musl libc toolchain for 100% static linking
- **CivetWeb**: HTTP server (fetched from Git)

## Development Tools

### Building

```bash
# Build the binary
bazel build //:hbf

# Build unstripped binary (for debugging)
bazel build //:hbf_unstripped
```

### Testing

```bash
# Run all tests
bazel test //...
```

### Linting

```bash
# Run clang-tidy on all source files
bazel run //:lint
```

**Lint tool requirements:**
- Requires `clang-tidy-18` installed system-wide
- Uses `.clang-tidy` configuration at repository root
- Enforces CERT C checks, readability, performance, and portability

### Code Coverage

```bash
# Generate coverage report (TODO: not yet implemented)
# Will use llvm-cov-18
```

## Binary Characteristics

- **Size**: 170 KB (stripped, statically linked with musl)
- **Dependencies**: ZERO runtime dependencies
- **Toolchain**: musl libc 1.2.3 (not glibc)
- **Format**: ELF 64-bit, statically linked

## IDE Setup

The project includes:
- `.clang-format`: Code formatting rules (Linux kernel style)
- `.clang-tidy`: Static analysis configuration
- VS Code recommended extensions for Python, C/C++

## Troubleshooting

### clang-tidy not found

If `bazel run //:lint` fails with "clang-tidy-18 not found":
```bash
sudo apt-get install clang-tidy-18
```

### Build fails with "musl toolchain not found"

Clean and rebuild:
```bash
bazel clean --expunge
bazel build //:hbf
```

### Binary size larger than expected

Make sure you're building the stripped version:
```bash
bazel build //:hbf  # This is the stripped version
```

The unstripped version is larger (~205 KB).
