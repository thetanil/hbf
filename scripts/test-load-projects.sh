#!/bin/bash

set -euo pipefail

export DAGGER_NO_NAG=1

echo "=== Test Load Projects ==="

# Remove existing projects.json if it exists
echo "1. Removing existing projects.json..."
rm -f projects.json

# Generate new projects.json using dagger
echo "2. Generating new projects.json..."
dagger call init-projects export --path=.

# Verify projects.json was created
if [ ! -f "projects.json" ]; then
    echo "❌ FAIL: projects.json was not created"
    exit 1
fi

echo "✅ projects.json created successfully"

# Check if projects.json contains a Hugo project
echo "3. Checking for Hugo project in generated file..."
if ! grep -q '"HbfType": "hugo"' projects.json; then
    echo "❌ FAIL: No Hugo project found in generated projects.json"
    echo "Generated content:"
    cat projects.json
    exit 1
fi

echo "✅ Hugo project found in projects.json"

# Parse the projects.json using our LoadProjects function
echo "4. Testing project parsing..."
OUTPUT=$(dagger call load-projects --projects-file=./projects.json)

# Verify the output contains a Hugo type project
if ! echo "$OUTPUT" | grep -q "Type: hugo"; then
    echo "❌ FAIL: LoadProjects output does not contain Hugo type"
    echo "Output:"
    echo "$OUTPUT"
    exit 1
fi

echo "✅ LoadProjects successfully parsed Hugo project"

# Show the parsed output
echo "5. Parsed projects:"
echo "$OUTPUT"

echo "🎉 All tests passed!"