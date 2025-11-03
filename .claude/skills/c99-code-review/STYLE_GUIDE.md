# HBF C99 Style Guide

Based on Linux kernel coding style with HBF-specific adaptations.

## Indentation

**Use TABS, not spaces**

- Tab width: 8 characters
- Never mix tabs and spaces
- Continuation lines: align with opening delimiter or use 2 tabs

```c
// GOOD: Tab indentation
int foo(void)
{
	int x = 42;
	if (x > 0) {
		return x;
	}
	return 0;
}

// GOOD: Long function arguments - continuation alignment
int result = very_long_function_name(first_argument,
				     second_argument,
				     third_argument);

// GOOD: Alternative - 2 tabs for continuation
int result = very_long_function_name(
		first_argument,
		second_argument,
		third_argument);
```

## Line Length

- **Preferred:** 80 characters
- **Maximum:** 100 characters
- Break long lines at logical points

```c
// GOOD: Break at logical operator
if (some_condition && another_condition &&
    yet_another_condition) {
	do_something();
}

// GOOD: Break after assignment operator
struct my_struct *instance =
	create_instance(param1, param2, param3);
```

## Bracing

### If/Else/While/For

Opening brace on **same line** as statement:

```c
// GOOD
if (condition) {
	statement;
}

if (condition) {
	statement1;
} else {
	statement2;
}

while (condition) {
	statement;
}

for (int i = 0; i < count; i++) {
	statement;
}

// GOOD: Single statement can omit braces (but prefer braces)
if (simple_condition)
	single_statement();

// PREFERRED: Always use braces for clarity
if (simple_condition) {
	single_statement();
}
```

### Functions

Opening brace on **new line**:

```c
// GOOD
int calculate_sum(int a, int b)
{
	return a + b;
}

static void
helper_with_long_name(int param1, int param2, int param3)
{
	// Implementation
}
```

### Switch Statements

```c
// GOOD
switch (value) {
case OPTION_A:
	handle_a();
	break;
case OPTION_B:
	handle_b();
	break;
default:
	handle_default();
	break;
}

// GOOD: Fallthrough must be commented
switch (value) {
case OPTION_A:
	/* fallthrough */
case OPTION_B:
	handle_both();
	break;
default:
	break;
}
```

## Naming Conventions

### Functions and Variables

Use `snake_case`:

```c
// GOOD
int calculate_hash(const char *input);
size_t buffer_length;
struct http_request *req;

// BAD
int CalculateHash(const char *input);  // camelCase
int calculate_Hash(const char *input); // mixed
```

### Macros and Constants

Use `UPPER_SNAKE_CASE`:

```c
// GOOD
#define MAX_BUFFER_SIZE 4096
#define LOG_LEVEL_DEBUG 0

// GOOD: Macro functions
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
```

### Types

Use `snake_case_t` suffix:

```c
// GOOD
typedef struct http_request http_request_t;
typedef int (*handler_fn_t)(void *ctx);

// Struct definitions
struct http_request {
	char *method;
	char *path;
};
```

### Static Functions

Prefix with module name or use descriptive names:

```c
// GOOD: Module prefix
static int db_open_connection(const char *path);
static void http_handle_request(struct request *req);

// GOOD: Descriptive name
static void cleanup_resources(void);
static bool validate_input(const char *str);
```

## Spacing

### Operators

Space around binary operators, no space around unary:

```c
// GOOD
int sum = a + b;
int product = x * y;
bool equal = (a == b);
ptr = &value;
deref = *ptr;
count++;

// BAD
int sum=a+b;           // No spaces
int product = x*y;     // Inconsistent
ptr = & value;         // Space after unary
```

### Function Calls

No space between function name and opening parenthesis:

```c
// GOOD
result = function(arg1, arg2);
printf("Hello, world\n");

// BAD
result = function (arg1, arg2);
```

### Control Statements

Space after keyword, before opening parenthesis:

```c
// GOOD
if (condition) { }
while (condition) { }
for (i = 0; i < n; i++) { }
switch (value) { }

// BAD
if(condition) { }
while(condition) { }
```

### Pointer Declarations

Asterisk attached to variable name:

```c
// GOOD
char *str;
int *ptr, *ptr2;
const char *const *argv;

// ACCEPTABLE in function parameters
int function(char *str, int *ptr);

// BAD
char* str;
char * str;
```

## Comments

### Style

Prefer C-style `/* */` comments:

```c
// GOOD: Multi-line
/*
 * This is a multi-line comment
 * with detailed explanation
 */

// GOOD: Single-line
/* Quick note */

// ACCEPTABLE: C++ style for brief notes
int x = 42; // Temporary value

// BAD: C++ style for blocks
// This is a longer explanation
// that spans multiple lines
// using C++ style
```

### Function Comments

Document non-obvious functions:

```c
/*
 * calculate_hash - Compute FNV-1a hash of string
 * @str: Input string to hash
 * @len: Length of string
 *
 * Returns: 32-bit hash value
 */
static uint32_t calculate_hash(const char *str, size_t len)
{
	/* Implementation */
}
```

## Header Files

### Include Guards

Use `#ifndef` guards with module-based names:

```c
// In hbf/db/db.h
#ifndef HBF_DB_DB_H
#define HBF_DB_DB_H

/* Header content */

#endif /* HBF_DB_DB_H */
```

### Include Order

1. System headers (`<>`)
2. Blank line
3. Project headers (`""`)

```c
// GOOD
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hbf/db/db.h"
#include "hbf/shell/log.h"
```

### Header Content

Only include what's necessary for the interface:

```c
// GOOD: db.h
#ifndef HBF_DB_DB_H
#define HBF_DB_DB_H

#include <stddef.h>  /* size_t */

/* Forward declarations */
typedef struct db db_t;

/* Public API */
int db_open(db_t **db, const char *path);
void db_close(db_t *db);

#endif /* HBF_DB_DB_H */
```

## Error Handling

### Return Values

Use negative values for errors, zero for success:

```c
// GOOD
int function(void)
{
	if (error_condition) {
		return -1;
	}
	return 0;  /* Success */
}
```

### Goto for Cleanup

Use `goto` for cleanup in error paths:

```c
// GOOD
int function(void)
{
	char *buffer = NULL;
	int fd = -1;
	int ret = 0;

	buffer = malloc(SIZE);
	if (buffer == NULL) {
		ret = -1;
		goto cleanup;
	}

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		ret = -1;
		goto cleanup;
	}

	/* Success path */

cleanup:
	if (fd >= 0)
		close(fd);
	free(buffer);  /* free(NULL) is safe */
	return ret;
}
```

## Variable Declarations

### Declare at Block Start (C99)

```c
// GOOD: C99 allows declarations anywhere
int function(void)
{
	int result = 0;
	char *buffer = malloc(100);

	for (int i = 0; i < 10; i++) {
		/* Loop variable declared in for() */
	}

	return result;
}
```

### Initialize Variables

```c
// GOOD
int count = 0;
char *str = NULL;
bool found = false;

// BAD: Uninitialized
int count;
char *str;
if (condition) {
	count = 5;
	str = "hello";
}
/* count and str may be uninitialized */
```

## Common Patterns

### String Safety

```c
// GOOD: Safe string copy
strncpy(dest, src, sizeof(dest) - 1);
dest[sizeof(dest) - 1] = '\0';

// GOOD: Safe printf
snprintf(buffer, sizeof(buffer), "Value: %d", value);
```

### Array Iteration

```c
// GOOD: Using ARRAY_SIZE macro
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

int values[] = {1, 2, 3, 4, 5};
for (size_t i = 0; i < ARRAY_SIZE(values); i++) {
	process(values[i]);
}
```

### Boolean Returns

```c
// GOOD: Use stdbool.h
#include <stdbool.h>

bool is_valid(const char *str)
{
	if (str == NULL || str[0] == '\0') {
		return false;
	}
	return true;
}
```
