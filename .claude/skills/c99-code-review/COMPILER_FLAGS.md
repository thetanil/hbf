# Compiler Flags Reference

## Required Base Flags

```
-std=c99           # Strict C99 standard (no GNU extensions)
-Wall              # Enable all common warnings
-Wextra            # Enable extra warnings
-Werror            # Treat warnings as errors
-Wpedantic         # Strict ISO C compliance
```

## Type Conversion & Promotion

```
-Wconversion           # Warn on implicit conversions that may change value
-Wdouble-promotion     # Warn when float promoted to double implicitly
-Wcast-qual            # Warn on casts that drop qualifiers (const, volatile)
-Wcast-align           # Warn on casts that increase alignment requirements
```

**Examples:**

```c
// -Wconversion: Implicit narrowing
int x = 300;
uint8_t y = x;  // WARNING: may lose data

// -Wdouble-promotion: Float to double
float f = 1.0f;
double d = sin(f);  // WARNING: f promoted to double

// -Wcast-qual: Dropping const
const char *str = "hello";
char *mut = (char *)str;  // WARNING: casting away const

// -Wcast-align: Alignment increase
char buf[100];
int *ptr = (int *)buf;  // WARNING: may not be aligned
```

## Function Declarations

```
-Wstrict-prototypes    # Require function prototypes with types
-Wold-style-definition # Reject K&R style function definitions
-Wmissing-prototypes   # Warn if function defined without prototype
-Wmissing-declarations # Warn if global function has no declaration
-Wbad-function-cast    # Warn on suspicious function casts
```

**Examples:**

```c
// -Wstrict-prototypes: Missing parameter types
void foo();  // WARNING: should be foo(void)

// -Wold-style-definition: K&R style
int bar(x, y)
    int x, y;  // WARNING: old-style definition
{ return x + y; }

// -Wmissing-prototypes: Missing prototype
static void helper() { }  // WARNING: no prototype before definition

// GOOD: Proper prototype
static void helper(void);
static void helper(void) { }
```

## Code Quality

```
-Wshadow           # Warn when variable shadows another
-Wformat=2         # Extra format string checks (printf, etc.)
-Wwrite-strings    # Warn when string literals assigned to non-const
-Wundef            # Warn on undefined identifiers in #if
-Wswitch-enum      # Warn when switch doesn't handle all enum cases
-Wswitch-default   # Warn when switch has no default case
```

**Examples:**

```c
// -Wshadow: Variable shadowing
int count = 0;
void foo(void) {
    int count = 1;  // WARNING: shadows outer count
}

// -Wformat=2: Format string mismatch
int x = 42;
printf("%s", x);  // WARNING: %s expects string, got int

// -Wwrite-strings: String literal to non-const
char *s = "hello";  // WARNING: should be const char *

// -Wundef: Undefined macro in #if
#if SOME_UNDEFINED_MACRO  // WARNING: SOME_UNDEFINED_MACRO not defined
#endif

// -Wswitch-enum: Missing enum case
enum color { RED, GREEN, BLUE };
switch (c) {
    case RED: break;
    case GREEN: break;
    // WARNING: BLUE not handled
}

// -Wswitch-default: No default case
switch (x) {
    case 1: break;
    // WARNING: no default case
}
```

## Complete Flag List

Full compilation command for HBF C99 code:

```bash
gcc -std=c99 \
    -Wall \
    -Wextra \
    -Werror \
    -Wpedantic \
    -Wconversion \
    -Wdouble-promotion \
    -Wshadow \
    -Wformat=2 \
    -Wstrict-prototypes \
    -Wold-style-definition \
    -Wmissing-prototypes \
    -Wmissing-declarations \
    -Wcast-qual \
    -Wwrite-strings \
    -Wcast-align \
    -Wundef \
    -Wswitch-enum \
    -Wswitch-default \
    -Wbad-function-cast \
    -c file.c -o file.o
```

## Suppressing Warnings (Use Sparingly)

In rare cases where a warning is unavoidable and the code is verified safe:

```c
// Pragma approach (GCC/Clang)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
uint8_t x = validated_value;  // Known to be in range
#pragma GCC diagnostic pop

// Explicit cast approach (preferred)
uint8_t x = (uint8_t)(validated_value & 0xFF);
```

**IMPORTANT:** Always prefer fixing the code over suppressing warnings. Only suppress when:
1. The code is provably correct
2. Fixing would significantly reduce readability
3. Documented with comment explaining why

## Bazel Integration

These flags are configured in `.bazelrc` and applied automatically:

```python
# In BUILD.bazel
cc_library(
    name = "mylib",
    srcs = ["mylib.c"],
    # Flags applied automatically via .bazelrc
)
```

Verify warnings are enabled:
```bash
bazel build //... --verbose_failures
```
