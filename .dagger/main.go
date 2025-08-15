// A generated module for Hbf functions
//
// This module has been generated via dagger init and serves as a reference to
// basic module structure as you get started with Dagger.
//
// Two functions have been pre-created. You can modify, delete, or add to them,
// as needed. They demonstrate usage of arguments and return types using simple
// echo and grep commands. The functions can be called from the dagger CLI or
// from one of the SDKs.
//
// The first line in this comment block is a short description line and the
// rest is a long description with more detail on the module's purpose or usage,
// if appropriate. All modules should have a short description.

package main

import (
	"context"
	"dagger/hbf/internal/dagger"
	"encoding/json"
)

type Project struct {
	Repo string `json:"repo"`
	Ref  string `json:"ref"`
}

type Config map[string]Project

type Hbf struct{}

// Returns a container that echoes whatever string argument is provided
func (m *Hbf) ContainerEcho(stringArg string) *dagger.Container {
	return dag.Container().From("alpine:latest").WithExec([]string{"echo", stringArg})
}

// Returns lines that match a pattern in the files of the provided Directory
func (m *Hbf) GrepDir(ctx context.Context, directoryArg *dagger.Directory, pattern string) (string, error) {
	return dag.Container().
		From("alpine:latest").
		WithMountedDirectory("/mnt", directoryArg).
		WithWorkdir("/mnt").
		WithExec([]string{"grep", "-R", pattern, "."}).
		Stdout(ctx)
}

// Generates a basic projects.json file
func (m *Hbf) InitProjects(ctx context.Context) (*dagger.Directory, error) {
	// Default example content
	initial := Config{
		"projA": Project{
			Repo: "https://github.com/dagger/dagger.git",
			Ref:  "196f232a4d6b2d1d3db5f5e040cf20b6a76a76c5",
		},
		"projB": Project{
			Repo: "https://github.com/example/repoB.git",
			Ref:  "tags/v0.0.1",
		},
	}

	data, err := json.MarshalIndent(initial, "", "    ")
	if err != nil {
		return nil, err
	}

	// Create a directory with the file content and return it
	// The caller can then export this directory to wherever they want
	return dag.Directory().WithNewFile("projects.json", string(data)), nil
}
