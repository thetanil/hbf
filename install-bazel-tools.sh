#!/bin/bash

set -euo pipefail

echo "Installing bazelisk and ibazel..."

# Detect OS and architecture
OS=$(uname -s | tr '[:upper:]' '[:lower:]')
ARCH=$(uname -m)

case $ARCH in
    x86_64)
        ARCH="amd64"
        ;;
    arm64|aarch64)
        ARCH="arm64"
        ;;
    *)
        echo "Unsupported architecture: $ARCH"
        exit 1
        ;;
esac

# Create local bin directory if it doesn't exist
mkdir -p ~/.local/bin

# Install bazelisk
echo "Installing bazelisk..."
BAZELISK_VERSION="v1.19.0"
BAZELISK_URL="https://github.com/bazelbuild/bazelisk/releases/download/${BAZELISK_VERSION}/bazelisk-${OS}-${ARCH}"

curl -fsSL "$BAZELISK_URL" -o ~/.local/bin/bazelisk
chmod +x ~/.local/bin/bazelisk

# Create bazel symlink
ln -sf ~/.local/bin/bazelisk ~/.local/bin/bazel

# Install ibazel
echo "Installing ibazel..."
IBAZEL_VERSION="v0.24.0"
IBAZEL_URL="https://github.com/bazelbuild/bazel-watcher/releases/download/${IBAZEL_VERSION}/ibazel_${OS}_${ARCH}"

curl -fsSL "$IBAZEL_URL" -o ~/.local/bin/ibazel
chmod +x ~/.local/bin/ibazel

# Add to PATH if not already there
if [[ ":$PATH:" != *":$HOME/.local/bin:"* ]]; then
    echo "Adding ~/.local/bin to PATH..."
    echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.bashrc
    echo "Please run 'source ~/.bashrc' or restart your terminal to use bazel and ibazel"
fi

echo "Installation complete!"
echo "bazelisk installed at: ~/.local/bin/bazelisk"
echo "bazel symlink created at: ~/.local/bin/bazel"
echo "ibazel installed at: ~/.local/bin/ibazel"