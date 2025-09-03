#!/bin/bash

# Wails Build Script for Enhanced Compatibility
# This script ensures the proper environment for building Wails applications

set -e

echo "🔧 Setting up Wails build environment..."

# Check if we're in a Wails project
if [ ! -f "wails.json" ]; then
    echo "❌ Error: Not in a Wails project directory (wails.json not found)"
    exit 1
fi

# Set environment variables for webkit
export PKG_CONFIG_PATH="${PKG_CONFIG_PATH}:/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/lib/pkgconfig"
export CGO_ENABLED=1

# Check for webkit2gtk development libraries
echo "🔍 Checking for webkit2gtk libraries..."
if ! pkg-config --exists webkit2gtk-4.1; then
    if pkg-config --exists webkit2gtk-4.0; then
        echo "✅ Found webkit2gtk-4.0, will use webkit2_41 build tags"
        BUILD_TAGS="-tags webkit2_41"
    else
        echo "❌ Error: webkit2gtk development libraries not found"
        echo "Please ensure the container has webkit2gtk development packages installed"
        exit 1
    fi
else
    echo "✅ Found webkit2gtk-4.1"
    BUILD_TAGS="-tags webkit2_41"
fi

# Determine build command
BUILD_TYPE="${1:-build}"

case $BUILD_TYPE in
    "dev"|"develop")
        echo "🚀 Starting Wails development mode..."
        exec wails dev $BUILD_TAGS
        ;;
    "build"|"prod"|"production")
        echo "🏗️ Building Wails application for production..."
        exec wails build $BUILD_TAGS
        ;;
    "doctor")
        echo "🔬 Running Wails doctor..."
        wails doctor
        ;;
    *)
        echo "Usage: $0 [dev|build|doctor]"
        echo "  dev    - Start development mode"
        echo "  build  - Build for production (default)"
        echo "  doctor - Run Wails diagnostics"
        exit 1
        ;;
esac