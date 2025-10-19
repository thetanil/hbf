# CivetWeb HTTP server library (MIT License)

cc_library(
    name = "civetweb",
    srcs = [
        "src/civetweb.c",
    ] + glob(["src/*.inl"]),
    hdrs = ["include/civetweb.h"],
    includes = [
        "include",
        "src",  # For .inl files
    ],
    copts = [
        "-std=c99",
        # CivetWeb feature flags
        "-DNO_SSL",  # Disable SSL/TLS (out of scope, use reverse proxy)
        "-DNO_CGI",  # Disable CGI
        "-DNO_FILES",  # Disable file serving (we handle this in app)
        "-DUSE_IPV6=0",  # Disable IPv6 (use IPv4 only)
        "-DUSE_WEBSOCKET",  # Enable WebSocket support for Phase 8
        "-DMG_EXPERIMENTAL_INTERFACES",  # Enable additional APIs
        # Suppress warnings for third-party code
        "-Wno-cast-qual",
        "-Wno-switch-enum",
        "-Wno-switch-default",
        "-Wno-unused-but-set-variable",
        "-Wno-undef",
    ],
    visibility = ["//visibility:public"],
)
