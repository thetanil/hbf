we are on the raylib-vendored branch, where we will attempt to get raylib-go
working with bazel as the build system. we have the following plan:

🔹 High-Level Goal

Use Bazel to build:

A vendored cc_library for raylib, including its bundled copy of glad.c.

A vendored cc_library for GLFW (built from source).

Vendored X11-related libraries (xcb, x11, xrandr, xinerama, xi, xkbcommon, etc.) — enough for GLFW to work on any Linux with X11 runtime.

A go_binary that depends on raylib via cgo, linking against those vendored cc targets.

Output: a single portable ELF executable that works on Debian/Arch as long as the system has X11 running (i.e. user has a desktop environment).

🔹 Build Flow Overview

Workspace setup

Bazel WORKSPACE defines:

rules_go (for Go + cgo support).

rules_cc (for C/C++ builds).

http_archive or local_repository for vendored raylib + glfw source.

Vendored Sources

Place sources under third_party/:

third_party/raylib/

third_party/glfw/

(Optionally) vendored X11 protocol headers if you want zero external dev packages.

Include glad.c (raylib already bundles its GLAD fork).

Bazel Build Targets

cc_library(name="glfw", ...) — compile GLFW sources.

cc_library(name="raylib", deps=[":glfw"], ...) — compile raylib C sources, include glad.

cc_library(name="raylib_shim", deps=[":raylib"], ...) — tiny shim for cgo.

go_library(name="raylib_go", cdeps=[":raylib_shim"]) — wraps shim.

go_binary(name="mygame", deps=[":raylib_go"]) — your Go code.

C Build Flags

For raylib:

-DPLATFORM_DESKTOP -DGRAPHICS_API_OPENGL_33

For GLFW:

Disable wayland unless you want it.

Enable X11 support (-D_GLFW_X11).

For both: -fPIC -O2.

Linker Flags

Must vendor & link these libs statically (or build them as cc targets):

-lm -ldl -lpthread

-lx11 -lxrandr -lxi -lxcursor -lxinerama -lXxf86vm

These can be satisfied by vendoring X11 source packages (e.g., from xorg/libX11