# HBF Meta-Repository Implementation Guide

This guide provides step-by-step instructions for implementing the meta-repository (`thetanil/hbf`) that serves as the integration point for the HBF demo components.

## Prerequisites

- Repository `thetanil/hbf` exists and is cloned locally
- Repository has the eclipse-score devcontainer configured
- You have access to create GitHub workflows
- Basic understanding of Bazel and bzlmod

## Implementation Steps

### Step 1: Update MODULE.bazel for Demo Components

Update the root `MODULE.bazel` file to include the demo components:

```bash
# Open MODULE.bazel in editor
```

Add the following content (replace existing if needed):

```bazel
module(
    name = "hbf",
    version = "1.0.0",
    compatibility_level = 1,
)

# Core build dependencies
bazel_dep(name = "rules_cc", version = "0.0.9")
bazel_dep(name = "rules_python", version = "0.25.0")
bazel_dep(name = "pybind11_bazel", version = "2.11.1")

# Demo components - start with v1.0.0
bazel_dep(name = "hbf_examplecpp", version = "1.0.0")
bazel_dep(name = "hbf_examplepy", version = "1.0.0")

# Python toolchain configuration
python = use_extension("@rules_python//python/extensions:python.bzl", "python")
python.toolchain(python_version = "3.11")
```

### Step 2: Create Demo Directory Structure

Create the demo directory and files:

```bash
mkdir -p demo
```

### Step 3: Create Demo BUILD.bazel

Create `demo/BUILD.bazel`:

```bazel
load("@rules_python//python:defs.bzl", "py_test", "py_binary")

py_test(
    name = "integration_test",
    srcs = ["integration_test.py"],
    deps = [
        "@hbf_examplepy//src/hbf_math:hbf_math",
    ],
    python_version = "PY3",
)

py_binary(
    name = "demo_usage",
    srcs = ["demo_usage.py"],
    deps = [
        "@hbf_examplepy//src/hbf_math:hbf_math",
    ],
    python_version = "PY3",
)

# Test for the new GCD functionality (will be added in v1.1.0)
py_test(
    name = "integration_test_v11",
    srcs = ["integration_test_v11.py"],
    deps = [
        "@hbf_examplepy//src/hbf_math:hbf_math",
    ],
    python_version = "PY3",
    # This test will fail until both components are at v1.1.0
    tags = ["manual"],  # Don't run by default initially
)
```

### Step 4: Create Integration Test (v1.0.0)

Create `demo/integration_test.py`:

```python
#!/usr/bin/env python3
"""
End-to-end integration test for HBF demo components v1.0.0.
Validates that the C++ and Python components work together correctly.
"""

import unittest
import sys
import os

try:
    from src.hbf_math import math_operations
except ImportError:
    # Fallback for different import paths
    import math_operations

class TestHBFIntegration(unittest.TestCase):
    """Integration tests for HBF demo components v1.0.0."""
    
    def test_cpp_python_integration_basic(self):
        """Test that Python can successfully call C++ functions."""
        # Test basic operations
        result = math_operations.add(10, 20)
        self.assertEqual(result, 30, "C++ add function via Python binding failed")
        
        result = math_operations.multiply(6, 7)
        self.assertEqual(result, 42, "C++ multiply function via Python binding failed")
    
    def test_cpp_python_integration_advanced(self):
        """Test more complex operations."""
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
    print("Running HBF Integration Tests v1.0.0...")
    unittest.main()
```

### Step 5: Create Demo Usage Example

Create `demo/demo_usage.py`:

```python
#!/usr/bin/env python3
"""
Demo usage example showing HBF integrated components in action.
"""

try:
    from src.hbf_math import math_operations
except ImportError:
    import math_operations

def main():
    print("HBF Demo - Integrated C++ and Python Components")
    print("=" * 50)
    
    # Demonstrate basic operations
    print(f"Addition: 15 + 25 = {math_operations.add(15, 25)}")
    print(f"Multiplication: 8 * 7 = {math_operations.multiply(8, 7)}")
    
    # Demonstrate more complex operations
    print(f"Factorial: 6! = {math_operations.factorial(6)}")
    print(f"Power: 3.0^4 = {math_operations.power(3.0, 4)}")
    
    # Demonstrate error handling
    print("\nTesting error handling:")
    try:
        result = math_operations.factorial(-3)
        print(f"Unexpected success: {result}")
    except RuntimeError as e:
        print(f"Expected error caught: {e}")
    
    print("\nDemo completed successfully!")

if __name__ == '__main__':
    main()
```

### Step 6: Create Integration Test for v1.1.0 (GCD Function)

Create `demo/integration_test_v11.py`:

```python
#!/usr/bin/env python3
"""
Integration test for HBF demo components v1.1.0 with GCD functionality.
This test will fail until both components are upgraded to v1.1.0.
"""

import unittest

try:
    from src.hbf_math import math_operations
except ImportError:
    import math_operations

class TestHBFIntegrationV11(unittest.TestCase):
    """Integration tests for HBF demo components v1.1.0."""
    
    def test_all_v10_functions_still_work(self):
        """Ensure backward compatibility with v1.0.0 functions."""
        self.assertEqual(math_operations.add(5, 3), 8)
        self.assertEqual(math_operations.multiply(4, 6), 24)
        self.assertEqual(math_operations.factorial(4), 24)
        self.assertAlmostEqual(math_operations.power(2.0, 3), 8.0)
    
    def test_new_gcd_function(self):
        """Test the newly added GCD function works in integration."""
        # Test basic GCD cases
        result = math_operations.gcd(48, 18)
        self.assertEqual(result, 6, "GCD(48, 18) should be 6")
        
        # Test coprime numbers
        result = math_operations.gcd(17, 13)
        self.assertEqual(result, 1, "GCD of coprime numbers should be 1")
        
        # Test with one number being zero
        result = math_operations.gcd(25, 0)
        self.assertEqual(result, 25, "GCD(25, 0) should be 25")
        
        # Test with negative numbers
        result = math_operations.gcd(-12, 8)
        self.assertEqual(result, 4, "GCD should handle negative numbers")
    
    def test_gcd_integration_with_other_functions(self):
        """Test GCD in combination with other functions."""
        # Use GCD result in other calculations
        a, b = 24, 36
        gcd_result = math_operations.gcd(a, b)
        self.assertEqual(gcd_result, 12)
        
        # Use GCD in power calculation
        power_result = math_operations.power(float(gcd_result), 2)
        self.assertAlmostEqual(power_result, 144.0)

if __name__ == '__main__':
    print("Running HBF Integration Tests v1.1.0 (with GCD)...")
    unittest.main()
```

### Step 7: Create GitHub Workflows Directory

```bash
mkdir -p .github/workflows
```

### Step 8: Create Integration CI Workflow

Create `.github/workflows/integration-ci.yml`:

```yaml
name: HBF Integration CI

on:
  push:
    branches: [ main ]
  pull_request:
    branches: [ main ]
  repository_dispatch:
    types: [component-update]

jobs:
  integration-test:
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
        key: ${{ runner.os }}-bazel-${{ hashFiles('.bazelversion', '.bazelrc', 'MODULE.bazel') }}
        restore-keys: |
          ${{ runner.os }}-bazel-
    
    - name: Build all targets
      run: bazel build //...
    
    - name: Run integration tests v1.0.0
      run: bazel test //demo:integration_test
    
    - name: Run demo usage
      run: bazel run //demo:demo_usage
    
    - name: Test GCD functionality (v1.1.0) - Optional
      run: |
        # This will pass only when both components are at v1.1.0
        bazel test //demo:integration_test_v11 || echo "GCD test failed - components not yet at v1.1.0"
    
    - name: Validate dependency compatibility
      run: |
        # Check for dependency conflicts
        bazel query 'deps(//...)' > /tmp/deps.txt
        echo "Dependency graph validated"
        
    - name: Generate build report
      run: |
        echo "## Build Report" >> $GITHUB_STEP_SUMMARY
        echo "- Integration tests: ✅ Passed" >> $GITHUB_STEP_SUMMARY
        echo "- Demo usage: ✅ Passed" >> $GITHUB_STEP_SUMMARY
        echo "- Dependencies: ✅ Compatible" >> $GITHUB_STEP_SUMMARY
```

### Step 9: Create Renovate Configuration

Create `.github/renovate.json`:

```json
{
  "$schema": "https://docs.renovatebot.com/renovate-schema.json",
  "extends": [
    "config:base"
  ],
  "enabledManagers": ["bazel-module"],
  "bazel-module": {
    "enabled": true,
    "fileMatch": ["MODULE\\.bazel$"]
  },
  "packageRules": [
    {
      "matchManagers": ["bazel-module"],
      "matchPackageNames": ["hbf_examplecpp", "hbf_examplepy"],
      "separateMinorPatch": false,
      "groupName": "HBF Demo Components",
      "schedule": ["at any time"],
      "prCreation": "immediate",
      "automerge": false,
      "reviewersFromCodeOwners": true,
      "addLabels": ["dependencies", "hbf-component"]
    },
    {
      "matchManagers": ["bazel-module"],
      "matchPackageNames": ["rules_cc", "rules_python", "pybind11_bazel"],
      "groupName": "Bazel Rules",
      "schedule": ["before 6am on monday"],
      "automerge": false,
      "addLabels": ["dependencies", "bazel-rules"]
    }
  ],
  "prConcurrentLimit": 2,
  "prHourlyLimit": 4,
  "branchPrefix": "renovate/",
  "commitMessagePrefix": "chore(deps): ",
  "commitMessageAction": "update",
  "commitMessageTopic": "{{depName}}",
  "commitMessageExtra": "to {{newVersion}}",
  "semanticCommits": "enabled",
  "dependencyDashboard": true,
  "dependencyDashboardTitle": "🤖 HBF Dependency Dashboard",
  "assignees": ["@thetanil"],
  "reviewers": ["thetanil"],
  "vulnerabilityAlerts": {
    "enabled": true,
    "addLabels": ["security"]
  },
  "lockFileMaintenance": {
    "enabled": true,
    "schedule": ["before 6am on sunday"]
  }
}
```

Create `.github/renovate.json5` (alternative format with comments):

```json5
{
  "$schema": "https://docs.renovatebot.com/renovate-schema.json",
  "extends": ["config:base"],
  
  // Enable Bazel module manager for MODULE.bazel files
  "enabledManagers": ["bazel-module"],
  "bazel-module": {
    "enabled": true,
    "fileMatch": ["MODULE\\.bazel$"]
  },
  
  "packageRules": [
    {
      // HBF demo components - immediate updates for demo purposes
      "matchManagers": ["bazel-module"],
      "matchPackageNames": ["hbf_examplecpp", "hbf_examplepy"],
      "separateMinorPatch": false,
      "groupName": "HBF Demo Components",
      "schedule": ["at any time"],
      "prCreation": "immediate",
      "automerge": false, // Never automerge - we want to demonstrate the failure/recovery
      "reviewersFromCodeOwners": true,
      "addLabels": ["dependencies", "hbf-component"],
      "prBodyTemplate": `
## HBF Component Update

**Component**: {{depName}}
**Version**: {{currentVersion}} → {{newVersion}}

This PR updates an HBF demo component. Integration tests will validate compatibility.

### Validation Checklist
- [ ] Integration tests pass
- [ ] Component builds successfully
- [ ] No breaking changes introduced
- [ ] Dependencies are in correct order

### Demo Notes
This update is part of the HBF dependency management demonstration.
      `
    },
    {
      // Bazel rules - weekly updates
      "matchManagers": ["bazel-module"],
      "matchPackageNames": ["rules_cc", "rules_python", "pybind11_bazel"],
      "groupName": "Bazel Rules",
      "schedule": ["before 6am on monday"],
      "automerge": false,
      "addLabels": ["dependencies", "bazel-rules"]
    }
  ],
  
  // Limit concurrent PRs to avoid overwhelming CI
  "prConcurrentLimit": 2,
  "prHourlyLimit": 4,
  
  // Branch and commit configuration
  "branchPrefix": "renovate/",
  "commitMessagePrefix": "chore(deps): ",
  "commitMessageAction": "update",
  "commitMessageTopic": "{{depName}}",
  "commitMessageExtra": "to {{newVersion}}",
  "semanticCommits": "enabled",
  
  // Dashboard and notifications
  "dependencyDashboard": true,
  "dependencyDashboardTitle": "🤖 HBF Dependency Dashboard",
  "assignees": ["@thetanil"],
  "reviewers": ["thetanil"],
  
  // Security and maintenance
  "vulnerabilityAlerts": {
    "enabled": true,
    "addLabels": ["security"]
  },
  "lockFileMaintenance": {
    "enabled": true,
    "schedule": ["before 6am on sunday"]
  }
}
```
```

### Step 10: Create Renovate Workflow

Create `.github/workflows/renovate.yml`:

```yaml
name: Renovate

on:
  schedule:
    # Run every 4 hours for demo responsiveness
    - cron: '0 */4 * * *'
  workflow_dispatch:
    inputs:
      logLevel:
        description: 'Log level'
        required: false
        default: 'info'
        type: choice
        options:
          - info
          - debug
          - trace

env:
  # Use GitHub App token or PAT with repo scope
  RENOVATE_TOKEN: ${{ secrets.RENOVATE_TOKEN }}
  LOG_LEVEL: ${{ github.event.inputs.logLevel || 'info' }}

jobs:
  renovate:
    runs-on: ubuntu-latest
    
    steps:
    - name: Checkout
      uses: actions/checkout@v4
    
    - name: Self-hosted Renovate
      uses: renovatebot/github-action@v39.2.4
      with:
        configurationFile: .github/renovate.json
        token: ${{ secrets.RENOVATE_TOKEN }}
        renovate-version: '37.59.8'
      env:
        LOG_LEVEL: ${{ env.LOG_LEVEL }}
        RENOVATE_REPOSITORIES: ${{ github.repository }}
        # Force Renovate to create PRs immediately for demo purposes
        RENOVATE_REQUIRE_CONFIG: 'optional'
        RENOVATE_ONBOARDING: 'false'
        # Enable Bazel support
        RENOVATE_ENABLE_MANAGERS: 'bazel-module'
```

### Step 11: Create Manual Component Update Workflow (Fallback)

Create `.github/workflows/manual-component-update.yml`:

```yaml
name: Manual Component Update

on:
  workflow_dispatch:
    inputs:
      component:
        description: 'Component name to update'
        required: true
        type: choice
        options:
          - hbf_examplecpp
          - hbf_examplepy
      version:
        description: 'New version'
        required: true
        type: string
      create_pr:
        description: 'Create pull request'
        required: false
        default: true
        type: boolean

jobs:
  update-component:
    runs-on: ubuntu-latest
    
    steps:
    - name: Checkout code
      uses: actions/checkout@v4
      with:
        token: ${{ secrets.RENOVATE_TOKEN || secrets.GITHUB_TOKEN }}
    
    - name: Update MODULE.bazel
      run: |
        COMPONENT="${{ github.event.inputs.component }}"
        VERSION="${{ github.event.inputs.version }}"
        
        echo "Updating $COMPONENT to $VERSION"
        
        # Update the MODULE.bazel file
        sed -i "s/bazel_dep(name = \"$COMPONENT\", version = \"[^\"]*\")/bazel_dep(name = \"$COMPONENT\", version = \"$VERSION\")/" MODULE.bazel
        
        echo "Updated MODULE.bazel:"
        grep "$COMPONENT" MODULE.bazel
        
        # Validate the change
        if ! grep -q "bazel_dep(name = \"$COMPONENT\", version = \"$VERSION\")" MODULE.bazel; then
          echo "ERROR: Failed to update $COMPONENT to $VERSION"
          exit 1
        fi
    
    - name: Validate Bazel configuration
      run: |
        # Check if the MODULE.bazel file is still valid
        if command -v bazel &> /dev/null; then
          bazel query --enable_bzlmod @${{ github.event.inputs.component }}//... || echo "Warning: Component not yet available in registry"
        fi
    
    - name: Create Pull Request
      if: ${{ github.event.inputs.create_pr == 'true' }}
      uses: peter-evans/create-pull-request@v5
      with:
        token: ${{ secrets.RENOVATE_TOKEN || secrets.GITHUB_TOKEN }}
        commit-message: "chore(deps): update ${{ github.event.inputs.component }} to ${{ github.event.inputs.version }}"
        title: "chore(deps): update ${{ github.event.inputs.component }} to ${{ github.event.inputs.version }}"
        body: |
          ## Manual Component Update
          
          **Component**: ${{ github.event.inputs.component }}
          **Version**: ${{ github.event.inputs.version }}
          
          This PR was created manually for testing dependency update scenarios.
          
          ### Validation Checklist
          - [ ] Integration tests pass
          - [ ] Component builds successfully  
          - [ ] No breaking changes introduced
          - [ ] Dependencies are in correct order
          
          ### Demo Notes
          This update demonstrates HBF's dependency validation capabilities.
          The CI pipeline will validate integration compatibility before allowing merge.
        branch: manual-update-${{ github.event.inputs.component }}-${{ github.event.inputs.version }}
        delete-branch: true
        labels: |
          dependencies
          hbf-component
          manual-update

### Step 12: Create Root BUILD.bazel

Update or create the root `BUILD.bazel`:

```bazel
# Root BUILD file for HBF meta-repository

package(default_visibility = ["//visibility:public"])

# Re-export demo targets for easy access
alias(
    name = "integration_test",
    actual = "//demo:integration_test",
)

alias(
    name = "demo_usage",
    actual = "//demo:demo_usage",
)

# Filegroup for documentation
filegroup(
    name = "docs",
    srcs = [
        "README.md",
        "demo_specification.md",
        "demo_hbf_impl.md",
        "//demo:integration_test.py",
        "//demo:demo_usage.py",
    ],
)
```

### Step 13: Create .bazelrc Configuration

Create or update `.bazelrc`:

```bash
# Build configuration for HBF demo
build --enable_bzlmod
build --experimental_enable_bzlmod

# Python configuration
build --python_top=@python_3_11//:bin/python

# Test configuration
test --test_output=errors
test --test_verbose_timeout_warnings

# Performance optimizations
build --jobs=auto
build --local_ram_resources=2048
build --local_cpu_resources=4

# Debugging options (uncomment as needed)
# build --verbose_failures
# build --sandbox_debug
```

### Step 12: Testing the Implementation

#### Test Basic Integration (v1.0.0)

```bash
# Build everything
bazel build //...

# Run integration tests
bazel test //demo:integration_test

# Run demo
bazel run //demo:demo_usage

# Verify GCD test fails (as expected)
bazel test //demo:integration_test_v11 --test_output=all || echo "Expected failure - GCD not available yet"
```

#### Validate CI Configuration

```bash
# Test that workflows are valid
cd .github/workflows
yamllint *.yml  # If yamllint is available

# Test manual component update workflow (requires GitHub CLI and RENOVATE_TOKEN)
# gh workflow run manual-component-update.yml --field component=hbf_examplecpp --field version=1.0.0
```

#### Setup Renovate Token

1. **Create GitHub Personal Access Token (PAT)**:
   - Go to GitHub Settings → Developer settings → Personal access tokens → Tokens (classic)
   - Create new token with `repo` scope
   - Copy the token value

2. **Add token to repository secrets**:
   - Go to repository Settings → Secrets and variables → Actions
   - Create new repository secret named `RENOVATE_TOKEN`
   - Paste the PAT value

3. **Test Renovate workflow**:
```bash
# Trigger Renovate manually to test configuration
gh workflow run renovate.yml --field logLevel=debug
```

### Step 14: Prepare for v1.1.0 Updates

When ready to test the dependency update scenario:

1. **Enable GCD tests** by removing the `manual` tag:
```bazel
# In demo/BUILD.bazel, change:
py_test(
    name = "integration_test_v11",
    srcs = ["integration_test_v11.py"],
    deps = [
        "@hbf_examplepy//src/hbf_math:hbf_math",
    ],
    python_version = "PY3",
    # Remove the manual tag when both components are at v1.1.0
)
```

2. **Update MODULE.bazel versions** when component updates are ready:
```bazel
# Update to v1.1.0 when components are ready
bazel_dep(name = "hbf_examplecpp", version = "1.1.0")
bazel_dep(name = "hbf_examplepy", version = "1.1.0")
```

## Validation Checklist

- [ ] MODULE.bazel includes both demo components at v1.0.0
- [ ] Demo directory structure created correctly
- [ ] Integration tests pass with v1.0.0 components
- [ ] Demo usage example runs successfully
- [ ] GitHub workflows are configured and valid
- [ ] CI/CD pipeline validates integration correctly
- [ ] GCD test exists but is marked manual (will fail until v1.1.0)
- [ ] Renovate configuration is valid and token is set
- [ ] Renovate can create dependency update PRs
- [ ] Manual component update workflow works as fallback
- [ ] All Bazel targets build successfully
- [ ] Documentation is complete and accurate

### Renovate-Specific Validation
- [ ] `.github/renovate.json` configuration is valid
- [ ] `RENOVATE_TOKEN` secret is configured with repo scope
- [ ] Renovate workflow runs without errors
- [ ] Bazel module manager is enabled and working
- [ ] Component updates create separate PRs with proper labels
- [ ] Dependency dashboard is accessible and informative

## Next Steps

1. Implement the C++ component (`thetanil/hbf-examplecpp`) following `demo_examplecpp_impl.md`
2. Implement the Python component (`thetanil/hbf-examplepy`) following `demo_examplepy_impl.md`
3. Test the complete integration workflow
4. Execute the dependency update scenario (v1.0.0 → v1.1.0)

This meta-repository implementation provides the foundation for demonstrating HBF's integration capabilities and dependency validation features.
