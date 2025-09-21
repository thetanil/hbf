# HBF C++ Component Implementation Guide

This guide provides step-by-step instructions for implementing the C++ component (`thetanil/hbf-examplecpp`) that provides mathematical functions for the HBF demo.

## Prerequisites

- Repository `thetanil/hbf-examplecpp` exists and is cloned locally
- Repository has the eclipse-score devcontainer configured
- Basic understanding of C++, Bazel, and bzlmod
- Access to create GitHub releases and tags

## Implementation Steps

### Step 1: Create MODULE.bazel

Create the root `MODULE.bazel` file:

```bazel
module(
    name = "hbf_examplecpp",
    version = "1.0.0",
    compatibility_level = 1,
)

# Core dependencies
bazel_dep(name = "rules_cc", version = "0.0.9")

# Development dependencies (for testing)
bazel_dep(name = "googletest", version = "1.14.0", dev_dependency = True)
```

### Step 2: Create Root BUILD.bazel

Create the root `BUILD.bazel` file:

```bazel
# Root BUILD file for hbf-examplecpp

package(default_visibility = ["//visibility:public"])

# Re-export main library for easy access
alias(
    name = "math_operations",
    actual = "//src:math_operations",
)

# Documentation filegroup
filegroup(
    name = "docs",
    srcs = [
        "README.md",
        "LICENSE",
    ],
)
```

### Step 3: Create WORKSPACE.bazel

Create an empty `WORKSPACE.bazel` file for bzlmod compatibility:

```bash
touch WORKSPACE.bazel
```

### Step 4: Create Source Directory Structure

```bash
mkdir -p src
mkdir -p tests
```

### Step 5: Create C++ Header File

Create `src/math_operations.h`:

```cpp
#pragma once

#include <stdexcept>

namespace hbf_math {
    /**
     * Add two integers.
     * @param a First integer
     * @param b Second integer
     * @return Sum of a and b
     */
    int add(int a, int b);
    
    /**
     * Multiply two integers.
     * @param a First integer
     * @param b Second integer
     * @return Product of a and b
     */
    int multiply(int a, int b);
    
    /**
     * Calculate factorial of a non-negative integer.
     * @param n Non-negative integer
     * @return Factorial of n
     * @throws std::invalid_argument if n is negative
     */
    long long factorial(int n);
    
    /**
     * Calculate power of a number.
     * @param base Base number
     * @param exponent Integer exponent
     * @return base raised to the power of exponent
     */
    double power(double base, int exponent);
}
```

### Step 6: Create C++ Implementation File

Create `src/math_operations.cpp`:

```cpp
#include "math_operations.h"

namespace hbf_math {
    
    int add(int a, int b) {
        return a + b;
    }
    
    int multiply(int a, int b) {
        return a * b;
    }
    
    long long factorial(int n) {
        if (n < 0) {
            throw std::invalid_argument("Factorial of negative number is undefined");
        }
        if (n <= 1) {
            return 1;
        }
        return n * factorial(n - 1);
    }
    
    double power(double base, int exponent) {
        if (exponent == 0) {
            return 1.0;
        }
        if (exponent < 0) {
            return 1.0 / power(base, -exponent);
        }
        return base * power(base, exponent - 1);
    }
}
```

### Step 7: Create Source BUILD.bazel

Create `src/BUILD.bazel`:

```bazel
load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "math_operations",
    srcs = ["math_operations.cpp"],
    hdrs = ["math_operations.h"],
    visibility = ["//visibility:public"],
    strip_include_prefix = "/src",
)
```

### Step 8: Create C++ Test File

Create `tests/test_math_operations.cpp`:

```cpp
#include <gtest/gtest.h>
#include "math_operations.h"

namespace hbf_math {
namespace test {

class MathOperationsTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test addition function
TEST_F(MathOperationsTest, AddPositiveNumbers) {
    EXPECT_EQ(add(2, 3), 5);
    EXPECT_EQ(add(10, 20), 30);
    EXPECT_EQ(add(0, 5), 5);
}

TEST_F(MathOperationsTest, AddNegativeNumbers) {
    EXPECT_EQ(add(-1, 1), 0);
    EXPECT_EQ(add(-5, -3), -8);
    EXPECT_EQ(add(-10, 5), -5);
}

// Test multiplication function
TEST_F(MathOperationsTest, MultiplyPositiveNumbers) {
    EXPECT_EQ(multiply(3, 4), 12);
    EXPECT_EQ(multiply(7, 8), 56);
    EXPECT_EQ(multiply(1, 100), 100);
}

TEST_F(MathOperationsTest, MultiplyWithZero) {
    EXPECT_EQ(multiply(0, 5), 0);
    EXPECT_EQ(multiply(10, 0), 0);
    EXPECT_EQ(multiply(0, 0), 0);
}

TEST_F(MathOperationsTest, MultiplyNegativeNumbers) {
    EXPECT_EQ(multiply(-2, 5), -10);
    EXPECT_EQ(multiply(3, -4), -12);
    EXPECT_EQ(multiply(-6, -7), 42);
}

// Test factorial function
TEST_F(MathOperationsTest, FactorialBasicCases) {
    EXPECT_EQ(factorial(0), 1);
    EXPECT_EQ(factorial(1), 1);
    EXPECT_EQ(factorial(5), 120);
    EXPECT_EQ(factorial(6), 720);
}

TEST_F(MathOperationsTest, FactorialNegativeThrows) {
    EXPECT_THROW(factorial(-1), std::invalid_argument);
    EXPECT_THROW(factorial(-5), std::invalid_argument);
}

// Test power function
TEST_F(MathOperationsTest, PowerBasicCases) {
    EXPECT_DOUBLE_EQ(power(2.0, 0), 1.0);
    EXPECT_DOUBLE_EQ(power(2.0, 3), 8.0);
    EXPECT_DOUBLE_EQ(power(5.0, 2), 25.0);
}

TEST_F(MathOperationsTest, PowerNegativeExponent) {
    EXPECT_DOUBLE_EQ(power(2.0, -1), 0.5);
    EXPECT_DOUBLE_EQ(power(4.0, -2), 0.0625);
}

TEST_F(MathOperationsTest, PowerZeroBase) {
    EXPECT_DOUBLE_EQ(power(0.0, 5), 0.0);
    EXPECT_DOUBLE_EQ(power(0.0, 0), 1.0);  // 0^0 = 1 by convention
}

}  // namespace test
}  // namespace hbf_math
```

### Step 9: Create Test BUILD.bazel

Create `tests/BUILD.bazel`:

```bazel
load("@rules_cc//cc:defs.bzl", "cc_test")

cc_test(
    name = "test_math_operations",
    srcs = ["test_math_operations.cpp"],
    deps = [
        "//src:math_operations",
        "@googletest//:gtest_main",
    ],
)
```

### Step 10: Create .bazelrc Configuration

Create `.bazelrc`:

```bash
# Build configuration for C++ component
build --enable_bzlmod
build --experimental_enable_bzlmod

# C++ configuration
build --cxxopt=-std=c++17
build --host_cxxopt=-std=c++17

# Test configuration
test --test_output=errors
test --test_verbose_timeout_warnings

# Performance optimizations
build --jobs=auto

# Debugging options (uncomment as needed)
# build --verbose_failures
# build --compilation_mode=dbg
```

### Step 11: Create README.md

Create `README.md`:

```markdown
# HBF Example C++ Component

A simple C++ mathematical library component for the HBF (Harmonizing Build Federation) demo.

## Overview

This component provides basic mathematical operations that can be consumed by other components in the HBF ecosystem. It demonstrates:

- Bazel module (bzlmod) integration
- Cross-language interoperability
- Semantic versioning
- Comprehensive testing

## Functions Provided

### v1.0.0 Functions

- `add(int a, int b)` - Add two integers
- `multiply(int a, int b)` - Multiply two integers  
- `factorial(int n)` - Calculate factorial of non-negative integer
- `power(double base, int exponent)` - Calculate power with integer exponent

### v1.1.0 Functions (Added Later)

- `gcd(int a, int b)` - Calculate greatest common divisor

## Building

### Prerequisites

- Bazel 6.0+ with bzlmod support
- C++17 compatible compiler

### Build Commands

```bash
# Build the library
bazel build //src:math_operations

# Build everything
bazel build //...

# Run tests
bazel test //...

# Run specific test
bazel test //tests:test_math_operations
```

## Usage in Other Components

Add to your `MODULE.bazel`:

```bazel
bazel_dep(name = "hbf_examplecpp", version = "1.0.0")
```

Use in your BUILD.bazel:

```bazel
cc_binary(
    name = "my_app",
    srcs = ["main.cpp"],
    deps = ["@hbf_examplecpp//src:math_operations"],
)
```

## Versioning

This component follows semantic versioning:

- **v1.0.0**: Initial release with basic mathematical operations
- **v1.1.0**: Added GCD function (breaking for consumers expecting only v1.0.0 API)

## Testing

The component includes comprehensive unit tests covering:

- Positive and negative number handling
- Edge cases (zero, large numbers)
- Error conditions and exception handling
- Boundary value testing

## Integration

This component is designed to work with:

- `hbf_examplepy` - Python wrapper providing language bindings
- `hbf` meta-repository - Integration testing and version coordination

## License

MIT License - see LICENSE file for details.
```

### Step 12: Create LICENSE File

Create `LICENSE`:

```text
MIT License

Copyright (c) 2025 HBF Demo

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

### Step 13: Create GitHub Workflow for CI

Create `.github/workflows/ci.yml`:

```yaml
name: C++ Component CI

on:
  push:
    branches: [ main ]
  pull_request:
    branches: [ main ]
  release:
    types: [published]

jobs:
  build-and-test:
    runs-on: ubuntu-latest
    
    steps:
    - name: Checkout code
      uses: actions/checkout@v4
    
    - name: Setup Bazel
      uses: bazelbuild/setup-bazelisk@v2
    
    - name: Cache Bazel
      uses: actions/cache@v3
      with:
        path: |
          ~/.cache/bazel
        key: ${{ runner.os }}-bazel-cpp-${{ hashFiles('.bazelversion', '.bazelrc', 'MODULE.bazel') }}
        restore-keys: |
          ${{ runner.os }}-bazel-cpp-
    
    - name: Build library
      run: bazel build //src:math_operations
    
    - name: Build all targets
      run: bazel build //...
    
    - name: Run all tests
      run: bazel test //... --test_output=errors
    
    - name: Validate module structure
      run: |
        echo "Validating Bazel module structure..."
        bazel query //... --output=package
        
    - name: Check code formatting (if clang-format available)
      run: |
        if command -v clang-format &> /dev/null; then
          find src tests -name "*.cpp" -o -name "*.h" | xargs clang-format --dry-run --Werror
        else
          echo "clang-format not available, skipping format check"
        fi

  trigger-renovate:
    needs: build-and-test
    runs-on: ubuntu-latest
    if: github.event_name == 'release'
    
    steps:
    - name: Trigger Renovate update in meta-repo
      run: |
        VERSION="${{ github.event.release.tag_name }}"
        echo "Triggering Renovate update for new version: $VERSION"
        
        # Extract version number (remove 'v' prefix if present)
        VERSION_NUMBER="${VERSION#v}"
        
        # Create a release note that Renovate will pick up
        echo "Component hbf_examplecpp updated to version $VERSION_NUMBER"
        echo "Release URL: ${{ github.event.release.html_url }}"
        
        # Optional: Force Renovate run in meta-repo (if RENOVATE_WEBHOOK_SECRET is configured)
        if [ -n "${{ secrets.RENOVATE_WEBHOOK_SECRET }}" ]; then
          curl -X POST \
            -H "Authorization: token ${{ secrets.META_REPO_TOKEN }}" \
            -H "Accept: application/vnd.github.v3+json" \
            "https://api.github.com/repos/thetanil/hbf/dispatches" \
            -d '{
              "event_type": "renovate-trigger",
              "client_payload": {
                "component": "hbf_examplecpp",
                "version": "'$VERSION_NUMBER'",
                "repository": "thetanil/hbf-examplecpp",
                "release_url": "${{ github.event.release.html_url }}"
              }
            }'
        fi
```

### Step 14: Test the v1.0.0 Implementation

```bash
# Build everything
bazel build //...

# Run all tests
bazel test //...

# Verify specific library builds
bazel build //src:math_operations

# Run specific tests with verbose output
bazel test //tests:test_math_operations --test_output=all
```

### Step 15: Create Release v1.0.0

```bash
# Ensure everything is committed
git add .
git commit -m "Initial implementation of C++ math operations v1.0.0"
git push origin main

# Create and push tag
git tag v1.0.0
git push origin v1.0.0
```

## Preparing for v1.1.0 Update (GCD Function)

When ready to demonstrate the dependency update scenario, follow these steps:

### Step 16: Add GCD Function (v1.1.0)

Update `src/math_operations.h` to add the GCD function declaration:

```cpp
// Add this function declaration to the existing header:

/**
 * Calculate greatest common divisor of two integers.
 * @param a First integer
 * @param b Second integer  
 * @return Greatest common divisor of a and b
 */
int gcd(int a, int b);
```

Update `src/math_operations.cpp` to add the GCD implementation:

```cpp
// Add this function implementation to the existing source:

int gcd(int a, int b) {
    // Handle negative numbers by taking absolute values
    a = a < 0 ? -a : a;
    b = b < 0 ? -b : b;
    
    // Euclidean algorithm
    if (b == 0) {
        return a;
    }
    return gcd(b, a % b);
}
```

### Step 17: Add GCD Tests

Add these tests to `tests/test_math_operations.cpp`:

```cpp
// Add these test cases to the existing test file:

// Test GCD function
TEST_F(MathOperationsTest, GCDBasicCases) {
    EXPECT_EQ(gcd(48, 18), 6);
    EXPECT_EQ(gcd(17, 13), 1);  // Coprime numbers
    EXPECT_EQ(gcd(100, 25), 25);
    EXPECT_EQ(gcd(7, 7), 7);    // Same numbers
}

TEST_F(MathOperationsTest, GCDWithZero) {
    EXPECT_EQ(gcd(25, 0), 25);
    EXPECT_EQ(gcd(0, 30), 30);
    EXPECT_EQ(gcd(0, 0), 0);
}

TEST_F(MathOperationsTest, GCDWithNegatives) {
    EXPECT_EQ(gcd(-12, 8), 4);
    EXPECT_EQ(gcd(12, -8), 4);
    EXPECT_EQ(gcd(-12, -8), 4);
}

TEST_F(MathOperationsTest, GCDLargeNumbers) {
    EXPECT_EQ(gcd(1071, 462), 21);
    EXPECT_EQ(gcd(270, 192), 6);
}
```

### Step 18: Update Version to v1.1.0

Update `MODULE.bazel`:

```bazel
module(
    name = "hbf_examplecpp",
    version = "1.1.0",  # Bump version
    compatibility_level = 1,
)

# Dependencies remain the same
bazel_dep(name = "rules_cc", version = "0.0.9")
bazel_dep(name = "googletest", version = "1.14.0", dev_dependency = True)
```

### Step 19: Test v1.1.0 Implementation

```bash
# Build everything with new function
bazel build //...

# Run all tests including new GCD tests
bazel test //...

# Verify GCD function works
bazel test //tests:test_math_operations --test_output=all
```

### Step 20: Release v1.1.0

```bash
# Commit changes
git add .
git commit -m "Add GCD function for v1.1.0"
git push origin main

# Create and push tag
git tag v1.1.0
git push origin v1.1.0
```

## Validation Checklist

### v1.0.0 Checklist
- [ ] MODULE.bazel configured with correct name and version
- [ ] All source files created with proper implementations
- [ ] All tests pass successfully
- [ ] BUILD.bazel files have correct dependencies
- [ ] Library can be built and linked successfully
- [ ] GitHub CI workflow runs successfully
- [ ] README.md documentation is complete
- [ ] LICENSE file is present

### v1.1.0 Checklist
- [ ] GCD function added to header and implementation
- [ ] GCD function tests added and passing
- [ ] Version bumped to 1.1.0 in MODULE.bazel
- [ ] All existing tests still pass (backward compatibility)
- [ ] New functionality tested thoroughly
- [ ] Release tagged and pushed

## Integration Notes

- This component is designed to be consumed by `hbf_examplepy`
- The `//src:math_operations` target should be used by dependent components
- Version updates trigger notifications to the meta-repository
- The component follows semantic versioning for compatibility

This implementation provides a solid foundation for demonstrating Bazel module integration and cross-component dependency management in the HBF system.
