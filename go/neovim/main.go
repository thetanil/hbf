// +build linux

package main

import (
	"bytes"
	_ "embed"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"syscall"
)

//go:embed nvim_binary
var nvimBinary []byte

func main() {
	// Create a temporary directory for the nvim binary
	tmpDir, err := os.MkdirTemp("", "embedded-nvim-*")
	if err != nil {
		fmt.Fprintf(os.Stderr, "Failed to create temp directory: %v\n", err)
		os.Exit(1)
	}
	defer os.RemoveAll(tmpDir)

	// Write the embedded nvim binary to a temporary file
	nvimPath := filepath.Join(tmpDir, "nvim")
	if err := writeEmbeddedBinary(nvimPath); err != nil {
		fmt.Fprintf(os.Stderr, "Failed to extract nvim binary: %v\n", err)
		os.Exit(1)
	}

	// Make the binary executable
	if err := os.Chmod(nvimPath, 0755); err != nil {
		fmt.Fprintf(os.Stderr, "Failed to make nvim executable: %v\n", err)
		os.Exit(1)
	}

	// Forward all arguments to nvim and replace the current process
	args := append([]string{nvimPath}, os.Args[1:]...)
	if err := syscall.Exec(nvimPath, args, os.Environ()); err != nil {
		fmt.Fprintf(os.Stderr, "Failed to exec nvim: %v\n", err)
		os.Exit(1)
	}
}

func writeEmbeddedBinary(path string) error {
	file, err := os.Create(path)
	if err != nil {
		return fmt.Errorf("create file: %w", err)
	}
	defer file.Close()

	_, err = io.Copy(file, bytes.NewReader(nvimBinary))
	if err != nil {
		return fmt.Errorf("write binary data: %w", err)
	}

	return nil
}