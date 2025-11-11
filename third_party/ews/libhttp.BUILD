"""
libhttp (Embedded Web Server) external build file
MIT licensed HTTP server library from https://github.com/lammertb/libhttp
"""

cc_library(
    name = "libhttp",
    srcs = ["libhttp.c"],
    hdrs = ["libhttp.h"],
    copts = [
        "-std=c99",
        "-Wall",
        "-Wextra",
        # Disable errors for third-party code
        # Note: libhttp is a fork of CivetWeb with similar architecture
        "-Wno-error",
        "-Wno-sign-conversion",
        "-Wno-conversion",
        "-Wno-shadow",
        "-Wno-cast-qual",
        # libhttp configuration flags (similar to CivetWeb)
        "-DNO_SSL",  # No SSL/TLS support (use reverse proxy)
        "-DNO_CGI",  # No CGI support
        "-DNO_FILES",  # No built-in file serving (we handle via SQLite)
        "-DUSE_IPV6=0",  # IPv4 only
        "-DUSE_WEBSOCKET",  # Enable WebSocket support for future SSE/WS
    ],
    includes = ["."],
    visibility = ["//visibility:public"],
)
