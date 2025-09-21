# HBF Demo Implementation Specification

This document specifies a demonstration implementation of the HBF (Harmonizing Build Federation) system using three repositories to showcase the integration of multiple components in a distributed monolith pattern.

## Overview

The demo will consist of three repositories:

1. **`thetanil/hbf`** - Meta-repository (integration repository)
2. **`thetanil/hbf-examplecpp`** - C++ component providing mathematical functions
3. **`thetanil/hbf-examplepy`** - Python component that wraps the C++ functions and provides tests

This demonstrates the core HBF principles:
- Distributed development with centralized integration
- Bazel-first build system with bzlmod
- Cross-language component integration
- Reproducible builds with dependency pinning
- Multi-party collaboration patterns

## Demo Scenario

The demo implements a simple mathematical library where:
- The C++ component provides basic mathematical operations (addition, multiplication, factorial)
- The Python component wraps these functions using pybind11
- The Python component includes comprehensive tests
- The meta-repository coordinates versions and validates integration
- All components follow Bazel module best practices

## Repository Specifications

### 1. thetanil/hbf-examplecpp (C++ Component)

#### Purpose
Provides a simple C++ mathematical library with functions that can be called from other languages.

#### Repository Structure
```
hbf-examplecpp/
├── MODULE.bazel              # Bazel module definition
├── BUILD.bazel               # Root build file
├── WORKSPACE.bazel           # Empty file for bzlmod compatibility
├── src/
│   ├── BUILD.bazel           # Source build rules
│   ├── math_operations.h     # Header file
│   └── math_operations.cpp   # Implementation
├── tests/
│   ├── BUILD.bazel           # Test build rules
│   └── test_math_operations.cpp  # C++ unit tests
├── LICENSE                   # MIT License
└── README.md                 # Component documentation
```

#### Key Files Content

**MODULE.bazel**
```bazel
module(
    name = "hbf_examplecpp",
    version = "1.0.0",
    compatibility_level = 1,
)

bazel_dep(name = "rules_cc", version = "0.0.9")
bazel_dep(name = "googletest", version = "1.14.0", dev_dependency = True)
```

**src/BUILD.bazel**
```bazel
cc_library(
    name = "math_operations",
    srcs = ["math_operations.cpp"],
    hdrs = ["math_operations.h"],
    visibility = ["//visibility:public"],
)
```

**src/math_operations.h**
```cpp
#pragma once

namespace hbf_math {
    int add(int a, int b);
    int multiply(int a, int b);
    long long factorial(int n);
    double power(double base, int exponent);
}
```

**src/math_operations.cpp**
```cpp
#include "math_operations.h"
#include <stdexcept>

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
        if (n <= 1) return 1;
        return n * factorial(n - 1);
    }
    
    double power(double base, int exponent) {
        if (exponent == 0) return 1.0;
        if (exponent < 0) {
            return 1.0 / power(base, -exponent);
        }
        return base * power(base, exponent - 1);
    }
}
```

#### Integration Requirements
- Semantic versioning following SemVer
- Automated CI that triggers integration requests to meta-repo
- License compliance (MIT)
- Passes all integration validation steps

### 2. thetanil/hbf-examplepy (Python Component)

#### Purpose
Python wrapper around the C++ mathematical library with comprehensive testing and additional Python-specific functionality.

#### Repository Structure
```
hbf-examplepy/
├── MODULE.bazel              # Bazel module definition
├── BUILD.bazel               # Root build file
├── WORKSPACE.bazel           # Empty file for bzlmod compatibility
├── src/
│   ├── BUILD.bazel           # Python package build rules
│   ├── hbf_math/
│   │   ├── __init__.py       # Python package init
│   │   ├── BUILD.bazel       # Package build rules
│   │   └── python_wrapper.cpp  # pybind11 wrapper
│   └── py_math_extensions.py # Additional Python functions
├── tests/
│   ├── BUILD.bazel           # Test build rules
│   ├── test_math_operations.py  # Python unit tests
│   └── test_integration.py  # Integration tests
├── LICENSE                   # MIT License
└── README.md                 # Component documentation
```

#### Key Files Content

**MODULE.bazel**
```bazel
module(
    name = "hbf_examplepy",
    version = "1.0.0",
    compatibility_level = 1,
)

bazel_dep(name = "rules_python", version = "0.25.0")
bazel_dep(name = "pybind11_bazel", version = "2.11.1")
bazel_dep(name = "hbf_examplecpp", version = "1.0.0")

python = use_extension("@rules_python//python/extensions:python.bzl", "python")
python.toolchain(python_version = "3.11")
```

**src/hbf_math/BUILD.bazel**
```bazel
load("@pybind11_bazel//:build_defs.bzl", "pybind_extension")

pybind_extension(
    name = "math_operations",
    srcs = ["python_wrapper.cpp"],
    deps = ["@hbf_examplecpp//src:math_operations"],
)

py_library(
    name = "hbf_math",
    srcs = ["__init__.py"],
    data = [":math_operations"],
    visibility = ["//visibility:public"],
)
```

**src/hbf_math/python_wrapper.cpp**
```cpp
#include <pybind11/pybind11.h>
#include "src/math_operations.h"

namespace py = pybind11;

PYBIND11_MODULE(math_operations, m) {
    m.doc() = "HBF Math Operations Python Bindings";
    
    m.def("add", &hbf_math::add, "Add two integers");
    m.def("multiply", &hbf_math::multiply, "Multiply two integers");
    m.def("factorial", &hbf_math::factorial, "Calculate factorial");
    m.def("power", &hbf_math::power, "Calculate power");
}
```

**tests/test_math_operations.py**
```python
import unittest
from src.hbf_math import math_operations

class TestMathOperations(unittest.TestCase):
    def test_add(self):
        self.assertEqual(math_operations.add(2, 3), 5)
        self.assertEqual(math_operations.add(-1, 1), 0)
    
    def test_multiply(self):
        self.assertEqual(math_operations.multiply(3, 4), 12)
        self.assertEqual(math_operations.multiply(-2, 5), -10)
    
    def test_factorial(self):
        self.assertEqual(math_operations.factorial(0), 1)
        self.assertEqual(math_operations.factorial(5), 120)
        with self.assertRaises(RuntimeError):
            math_operations.factorial(-1)
    
    def test_power(self):
        self.assertAlmostEqual(math_operations.power(2.0, 3), 8.0)
        self.assertAlmostEqual(math_operations.power(4.0, -1), 0.25)

if __name__ == '__main__':
    unittest.main()
```

#### Integration Requirements
- Depends on hbf_examplecpp version 1.0.0
- Python 3.11+ compatibility
- Comprehensive test coverage
- Integration tests validating C++ bindings

### 3. thetanil/hbf (Meta-Repository)

#### Purpose
Integration repository that coordinates versions and validates compatibility between components.

#### Additional Structure for Demo
```
hbf/
├── MODULE.bazel              # Updated with demo components
├── BUILD.bazel               # Root build file
├── demo/
│   ├── BUILD.bazel           # Demo build rules
│   ├── integration_test.py   # End-to-end integration test
│   └── demo_usage.py         # Usage examples
├── .github/
│   └── workflows/
│       ├── integration-ci.yml     # CI for integration testing
│       └── component-update.yml   # Automated dependency updates
└── [existing files...]
```

#### Updated MODULE.bazel
```bazel
module(
    name = "hbf",
    version = "1.0.0",
    compatibility_level = 1,
)

# Core build dependencies
bazel_dep(name = "rules_cc", version = "0.0.9")
bazel_dep(name = "rules_python", version = "0.25.0")

# Demo components
bazel_dep(name = "hbf_examplecpp", version = "1.0.0")
bazel_dep(name = "hbf_examplepy", version = "1.0.0")

# Python toolchain
python = use_extension("@rules_python//python/extensions:python.bzl", "python")
python.toolchain(python_version = "3.11")
```

#### Integration Test
**demo/integration_test.py**
```python
#!/usr/bin/env python3
"""
End-to-end integration test for HBF demo components.
Validates that the C++ and Python components work together correctly.
"""

import unittest
import sys
import os

# Add the Python component to path
sys.path.append(os.path.join(os.path.dirname(__file__), '..'))

from hbf_examplepy.src.hbf_math import math_operations

class TestHBFIntegration(unittest.TestCase):
    """Integration tests for HBF demo components."""
    
    def test_cpp_python_integration(self):
        """Test that Python can successfully call C++ functions."""
        # Test basic operations
        result = math_operations.add(10, 20)
        self.assertEqual(result, 30, "C++ add function via Python binding failed")
        
        result = math_operations.multiply(6, 7)
        self.assertEqual(result, 42, "C++ multiply function via Python binding failed")
        
        # Test more complex operations
        result = math_operations.factorial(5)
        self.assertEqual(result, 120, "C++ factorial function via Python binding failed")
        
        result = math_operations.power(2.0, 10)
        self.assertAlmostEqual(result, 1024.0, places=5, 
                              msg="C++ power function via Python binding failed")
    
    def test_error_handling(self):
        """Test that error handling works across the C++/Python boundary."""
        with self.assertRaises(RuntimeError, 
                              msg="Expected exception for negative factorial"):
            math_operations.factorial(-5)
    
    def test_type_handling(self):
        """Test that type conversions work correctly."""
        # Integer operations
        self.assertIsInstance(math_operations.add(1, 2), int)
        self.assertIsInstance(math_operations.multiply(3, 4), int)
        self.assertIsInstance(math_operations.factorial(3), int)
        
        # Float operations
        self.assertIsInstance(math_operations.power(2.5, 2), float)

if __name__ == '__main__':
    unittest.main()
```

## Build and Integration Workflow

### Component Development Workflow

1. **Component Development**
   - Developers work on individual component repositories
   - Components follow Bazel module best practices
   - Each component has its own CI/CD pipeline

2. **Integration Request**
   - Component repositories trigger integration builds via GitHub Actions
   - Meta-repository receives repository dispatch events
   - Integration CI validates compatibility

3. **Validation Process**
   - Bazel build success across all components
   - Unit tests pass for individual components
   - Integration tests pass in meta-repository
   - License compliance checks
   - Security scanning

4. **Dependency Updates**
   - Bot creates PRs in meta-repository with version updates
   - CI validates the updated dependency graph
   - Merge queue ensures compatibility

### Build Commands

#### Building Individual Components

```bash
# Build C++ component
cd hbf-examplecpp
bazel build //...
bazel test //...

# Build Python component  
cd hbf-examplepy
bazel build //...
bazel test //...
```

#### Building Integrated System

```bash
# Build entire integrated system
cd hbf
bazel build //...
bazel test //...

# Run integration tests specifically
bazel test //demo:integration_test

# Run demo usage examples
bazel run //demo:demo_usage
```

### Validation Criteria

#### Component-Level Validation
- [ ] Builds successfully in isolation
- [ ] All unit tests pass
- [ ] LICENSE file present with SPDX identifier
- [ ] MODULE.bazel follows semantic versioning
- [ ] No high-severity security vulnerabilities
- [ ] Documentation is complete and accurate

#### Integration-Level Validation
- [ ] Builds successfully with all dependencies
- [ ] Integration tests pass
- [ ] No dependency conflicts
- [ ] Compatible with existing component versions
- [ ] Performance benchmarks meet requirements
- [ ] Memory usage within acceptable limits

## Demonstration Script

### Setup Phase
1. Clone all three repositories
2. Verify devcontainer environment
3. Install any additional dependencies

### Build Phase
1. Build C++ component independently
2. Build Python component independently
3. Build integrated system
4. Run all tests

### Core Demo: Dependency Update and Integration Failure/Recovery

This scenario demonstrates the critical importance of proper dependency ordering and how HBF protects against integration failures through its validation pipeline.

#### Step 1: Add New Function to C++ Component (v1.1.0)

1. **Update hbf-examplecpp** with a new `gcd` (Greatest Common Divisor) function:

**src/math_operations.h** (add to existing functions):
```cpp
int gcd(int a, int b);  // New function
```

**src/math_operations.cpp** (add implementation):
```cpp
int gcd(int a, int b) {
    if (b == 0) return a;
    return gcd(b, a % b);
}
```

**MODULE.bazel** (update version):
```bazel
module(
    name = "hbf_examplecpp", 
    version = "1.1.0",  # Bump version
    compatibility_level = 1,
)
```

2. **Publish C++ component v1.1.0** without integrating into meta-repo yet
   - Tag release in hbf-examplecpp repository
   - Publish to registry/artifact store
   - **Critically: Do NOT merge integration PR to meta-repo yet**

#### Step 2: Update Python Component to Use New Function (v1.1.0)

1. **Update hbf-examplepy** to use the new `gcd` function:

**src/hbf_math/python_wrapper.cpp** (add binding):
```cpp
m.def("gcd", &hbf_math::gcd, "Calculate greatest common divisor");
```

**tests/test_math_operations.py** (add test):
```python
def test_gcd(self):
    self.assertEqual(math_operations.gcd(48, 18), 6)
    self.assertEqual(math_operations.gcd(7, 13), 1)
    self.assertEqual(math_operations.gcd(100, 25), 25)
```

**MODULE.bazel** (update versions):
```bazel
module(
    name = "hbf_examplepy",
    version = "1.1.0",  # Bump version
    compatibility_level = 1,
)

bazel_dep(name = "hbf_examplecpp", version = "1.1.0")  # Use new C++ version
```

2. **Validate Python component builds independently**:
```bash
cd hbf-examplepy
bazel build //...
bazel test //...  # Should pass - Python can access new C++ function directly
```

3. **Publish Python component v1.1.0**
   - Tag release in hbf-examplepy repository
   - This triggers Dependabot to create PR in meta-repo

#### Step 3: Demonstrate Integration Failure

1. **Dependabot creates PR in meta-repo** to update Python component:
```bazel
# META-REPO MODULE.bazel - Dependabot PR
bazel_dep(name = "hbf_examplecpp", version = "1.0.0")  # Still old version!
bazel_dep(name = "hbf_examplepy", version = "1.1.0")   # New version with gcd dependency
```

2. **Integration CI fails** because:
   - Python v1.1.0 expects `gcd` function from C++ v1.1.0
   - Meta-repo still has C++ v1.0.0 which doesn't have `gcd`
   - Build fails with "undefined symbol" or similar error

**Expected Error Message**:
```
ERROR: @hbf_examplepy//src/hbf_math:python_wrapper.cpp
undefined reference to 'hbf_math::gcd(int, int)'
Integration test failed: missing dependency hbf_examplecpp v1.1.0
```

3. **PR automatically fails CI checks**:
   - Integration tests fail
   - Dependency validation fails
   - PR is blocked from merging

#### Step 4: Demonstrate Recovery Process

1. **First, integrate C++ component v1.1.0**:
   - Dependabot should create separate PR for C++ update
   - Or manually create PR updating C++ dependency:

```bazel
# META-REPO MODULE.bazel - C++ update PR
bazel_dep(name = "hbf_examplecpp", version = "1.1.0")  # Update C++ first
bazel_dep(name = "hbf_examplepy", version = "1.0.0")   # Keep Python old for now
```

2. **Validate C++ integration passes**:
```bash
cd hbf
bazel build //...
bazel test //...  # Should pass - backward compatible
```

3. **Merge C++ integration PR first**

4. **Now Python integration PR can succeed**:
   - Either rebase existing PR or create new one
   - Both dependencies now at compatible versions:

```bazel
# META-REPO MODULE.bazel - Final working state
bazel_dep(name = "hbf_examplecpp", version = "1.1.0")  # Has gcd function
bazel_dep(name = "hbf_examplepy", version = "1.1.0")   # Uses gcd function
```

5. **Validate complete integration**:
```bash
cd hbf
bazel build //...
bazel test //demo:integration_test  # Should pass with new gcd function
```

#### Step 5: Enhanced Integration Test

Add test for new functionality to **demo/integration_test.py**:
```python
def test_new_gcd_function(self):
    """Test the newly added GCD function works in integration."""
    result = math_operations.gcd(48, 18)
    self.assertEqual(result, 6, "New GCD function integration failed")
    
    result = math_operations.gcd(17, 13)
    self.assertEqual(result, 1, "GCD of coprime numbers should be 1")
```

### Key Learning Outcomes

This scenario demonstrates:

1. **Dependency Ordering Matters**: Components must be integrated in dependency order
2. **Integration Validation Works**: The system correctly rejects invalid dependency combinations
3. **Recovery is Possible**: Failed integrations can be recovered through proper sequencing
4. **Automated Protection**: Dependabot + CI prevents breaking changes from reaching production
5. **Clear Failure Messages**: Developers get actionable error messages to resolve issues

### Additional Demo Scenarios

#### Scenario A: Compatible Update (Happy Path)
- Update C++ component with backward-compatible change
- Python component doesn't need updates
- Both integrations succeed independently

#### Scenario B: Breaking Change Detection
- Update C++ component with breaking API change
- Python component fails to build against new version
- Integration CI catches the incompatibility before merge

#### Scenario C: Multi-Component Synchronization
- Multiple components need coordinated updates
- Demonstrate batched integration strategy
- Show how merge queue handles complex dependency graphs

## Success Criteria

The demo will be considered successful when:

1. **Component Independence**: Each component builds and tests successfully in isolation
2. **Cross-Language Integration**: Python successfully calls C++ functions with proper type handling
3. **Error Propagation**: Exceptions in C++ are properly handled in Python
4. **Version Management**: Meta-repository correctly pins and manages component versions
5. **CI/CD Integration**: Automated workflows successfully validate integration
6. **Documentation Quality**: All repositories have clear, accurate documentation
7. **Reproducible Builds**: Builds are hermetic and reproducible across environments
8. **Dependency Order Validation**: Integration fails when dependencies are out of order and recovers correctly
9. **Failure Recovery**: Failed integrations can be resolved through proper dependency sequencing
10. **Integration Protection**: Invalid dependency combinations are blocked before reaching production

### Critical Demo Validation Points

#### Failure Case Validation
- [ ] Python v1.1.0 PR fails when C++ is still v1.0.0
- [ ] Build error clearly indicates missing `gcd` function
- [ ] CI blocks the PR from merging
- [ ] Error messages are actionable for developers

#### Recovery Case Validation  
- [ ] C++ v1.1.0 integrates successfully first
- [ ] Python v1.1.0 PR then succeeds after C++ integration
- [ ] New `gcd` functionality works in integration tests
- [ ] System reaches consistent working state

#### Process Validation
- [ ] Dependabot creates appropriate PRs for version updates
- [ ] CI/CD pipeline correctly validates dependency compatibility
- [ ] Merge queue prevents invalid states from being committed
- [ ] Recovery path is clear and documented

## Extension Opportunities

This demo can be extended to show:

1. **Multi-Party Integration**: Add a third party with private dependencies
2. **Container Deployment**: Package integrated system as container images
3. **Performance Benchmarking**: Add performance tests and regression detection
4. **Security Scanning**: Integrate SAST/DAST tools
5. **Compliance Reporting**: Generate SBOMs and license compliance reports
6. **Cross-Platform Support**: Demonstrate Windows/macOS compatibility

## Timeline

- **Phase 1** (Week 1): Set up repositories and basic structure
- **Phase 2** (Week 2): Implement C++ component with tests  
- **Phase 3** (Week 3): Implement Python wrapper with pybind11
- **Phase 4** (Week 4): Integration and CI/CD setup
- **Phase 5** (Week 5): Dependency update scenario implementation
- **Phase 6** (Week 6): Demo refinement and failure/recovery testing

### Demo Execution Timeline

#### Initial Setup (30 minutes)
1. Set up all three repositories with v1.0.0
2. Validate basic integration works
3. Run initial integration tests

#### Failure Scenario (15 minutes)  
1. Update C++ component to v1.1.0 (add `gcd` function)
2. Update Python component to v1.1.0 (use `gcd` function)
3. Demonstrate integration failure when Python PR attempts to merge first
4. Show clear error messages and blocked CI

#### Recovery Scenario (15 minutes)
1. Integrate C++ v1.1.0 first 
2. Show successful integration
3. Then integrate Python v1.1.0
4. Validate complete system works with new functionality

#### Validation and Discussion (15 minutes)
1. Run comprehensive integration tests
2. Discuss key learning points
3. Show how this prevents production failures
4. Demonstrate additional scenarios if time permits

This specification provides a comprehensive foundation for implementing a working demonstration of the HBF system that showcases its key capabilities, integration patterns, and critical failure protection mechanisms.
