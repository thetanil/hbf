# HBF Coding Standards

This document defines the coding standards and quality requirements for the HBF project.

## Language and Compiler Requirements

### C99 Strict Compliance
- **Language**: Strict C99, no extensions, no C++
- **Standard flag**: `-std=c99`
- **No compiler extensions**: Code must compile with both GCC and Clang

### Compiler Warnings (Always Enabled, Treated as Errors)
All code must compile cleanly with the following flags:

```
-std=c99
-Wall
-Wextra
-Werror
-Wpedantic
-Wconversion
-Wdouble-promotion
-Wshadow
-Wformat=2
-Wstrict-prototypes
-Wold-style-definition
-Wmissing-prototypes
-Wmissing-declarations
-Wcast-qual
-Wwrite-strings
-Wcast-align
-Wundef
-Wswitch-enum
-Wswitch-default
-Wbad-function-cast
```

## Code Style

### Linux Kernel Style (Approximated)
HBF follows a variant of the Linux kernel coding style, enforced via `.clang-format`:

- **Indentation**: Tabs (8-space width)
- **Continuation indentation**: 8 spaces (1 tab)
- **Column limit**: 100 characters
- **Braces**: K&R style (opening brace on same line for functions and control structures)
- **Spacing**: Space after keywords (`if`, `for`, `while`, etc.), no space after function names

### Example
```c
int hbf_process_request(struct hbf_request *req, struct hbf_response *res)
{
	int ret;

	if (!req || !res) {
		return -1;
	}

	ret = validate_request(req);
	if (ret < 0) {
		return ret;
	}

	return process_valid_request(req, res);
}
```

### Naming Conventions
- **Functions**: `hbf_module_action()` (snake_case with `hbf_` prefix)
- **Structs**: `struct hbf_name` or `hbf_name_t` for typedef
- **Macros/Constants**: `HBF_CONSTANT_NAME` (SCREAMING_SNAKE_CASE)
- **Global variables**: Avoid when possible; prefix with `hbf_` if necessary
- **Static variables**: `s_variable_name` or module-specific prefix

### File Organization
- **Header guards**: `HBF_MODULE_HEADER_H` format
- **SPDX license**: First line of every file: `/* SPDX-License-Identifier: MIT */`
- **Includes order**:
  1. System headers (`<stdio.h>`, `<stdlib.h>`)
  2. Third-party headers (`<sqlite3.h>`)
  3. Project headers (`"internal/core/hash.h"`)

## Standards Compliance

### CERT C Coding Standard
Follow applicable CERT C rules for C99:
- **MEM**: Memory management (no leaks, proper cleanup)
- **STR**: String handling (null termination, bounds checking)
- **INT**: Integer safety (no overflow, proper types)
- **ARR**: Array safety (bounds checking)
- **FIO**: File I/O (error checking, proper closure)
- **ERR**: Error handling (check return values, proper propagation)

### musl libc Style Principles
- **Minimalism**: Keep functions small and focused
- **Defined behavior**: No undefined behavior, ever
- **Simple macros**: Prefer functions over complex macros
- **Clear ownership**: Explicit memory management, clear caller/callee contracts

## Quality Gates

### Static Analysis
All code must pass:
1. **clang-tidy**: CERT checks, bug checks, portability checks
2. **clang-format**: Automatic formatting verification
3. **Compiler warnings**: Zero warnings with `-Werror`

### Testing Requirements
- **Unit tests**: minunit-style macros with assert.h
- **Integration tests**: Black-box HTTP endpoint testing
- **Build verification**: All tests pass before merge

### CI Pipeline
1. Lint (`bazel build //:lint`)
2. Build with `-Werror`
3. Run all tests (`bazel test //...`)

## Security Requirements

### Memory Safety
- **No buffer overflows**: Always check bounds
- **No use-after-free**: Clear ownership, proper cleanup
- **No memory leaks**: Free all allocations, use RAII patterns where possible
- **Validate all inputs**: Especially user-provided data

### Cryptographic Best Practices
- **Constant-time comparisons**: For secrets (passwords, tokens, MACs)
- **Secure random**: Use `/dev/urandom` or equivalent
- **No homebrew crypto**: Use vetted implementations (Argon2, SHA-256)

### SQL Safety
- **Always use prepared statements**: No string interpolation
- **Validate input**: Before passing to SQLite
- **Sanitize errors**: Don't leak schema details to users

## Documentation

### Function Documentation
Document public API functions with:
```c
/*
 * Brief description of function purpose.
 *
 * @param input: Description of input parameter
 * @param output: Description of output parameter (must be >= N bytes)
 * @return 0 on success, -1 on error
 */
```

### Code Comments
- **Why, not what**: Explain intent and reasoning, not obvious operations
- **Edge cases**: Document non-obvious behavior and boundary conditions
- **TODOs**: Mark with `/* TODO(context): description */` format

## Build System

### Bazel Configuration
- **bzlmod only**: Use MODULE.bazel, no WORKSPACE
- **musl toolchain**: 100% static linking, no glibc
- **Compiler opts**: Enforced via `.bazelrc` and BUILD files
- **Link flags**: `-static -Wl,--gc-sections` for size optimization

### Third-Party Dependencies
- **No GPL**: MIT, BSD, Apache-2.0, or Public Domain only
- **Vendored sources**: In `third_party/`, no git submodules
- **Minimal dependencies**: Prefer self-contained implementations

## References

- [CERT C Coding Standard](https://wiki.sei.cmu.edu/confluence/display/c/SEI+CERT+C+Coding+Standard)
- [Linux Kernel Coding Style](https://www.kernel.org/doc/html/latest/process/coding-style.html)
- [musl libc](https://musl.libc.org/)
