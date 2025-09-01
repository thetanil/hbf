#!/bin/bash

# Test script for the Go Neovim wrapper - Linux only, no purego

echo "Testing Go Neovim wrapper (Linux x86_64, Neovim 0.11.3)..."
echo "============================================================"

# Test version
echo "1. Testing version:"
bazel run //go/neovim:neovim -- --version

echo -e "\n2. Testing help:"
bazel run //go/neovim:neovim -- --help

echo -e "\n3. Binary size comparison:"
echo "Embedded Go wrapper: $(du -h bazel-bin/go/neovim/neovim_/neovim | cut -f1)"
echo "Note: Neovim binary (~11MB) is downloaded automatically by Bazel"

echo -e "\n4. Build info:"
echo "Target: Linux x86_64 only (no Windows, no purego)"
echo "Go constraints: +build linux"
echo "Bazel GOOS: linux, GOARCH: amd64, pure: off"

echo -e "\nTo use the Go Neovim wrapper interactively:"
echo "bazel run //go/neovim:neovim -- [nvim-options] [files...]"
echo -e "\nTo create a standalone binary:"
echo "bazel build //go/neovim:neovim"
echo "cp bazel-bin/go/neovim/neovim_/neovim /usr/local/bin/nvim-go"