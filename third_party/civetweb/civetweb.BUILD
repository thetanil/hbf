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
        # NOTE: Do not define NO_FILESYSTEMS: CivetWeb requires either filesystem support
        # or custom hooks (mg_cry_internal_impl/log_access). We don't provide custom hooks,
        # so keep filesystem primitives enabled while still disabling file serving.
        "-DNO_THREAD_NAME",  # Disable thread naming (not needed)
        "-DUSE_IPV6=0",  # Disable IPv6 (use IPv4 only)
        # WebSocket explicitly disabled (do not define USE_WEBSOCKET)
        # Experimental interfaces disabled (do not define MG_EXPERIMENTAL_INTERFACES)
        # Reproducible builds: set a fixed build date so CivetWeb does not embed __DATE__/__TIME__
        "-DBUILD_DATE=\\\"1970-01-01T00:00:00Z\\\"",
        # Suppress warnings for third-party code
        "-Wno-cast-qual",
        "-Wno-switch-enum",
        "-Wno-switch-default",
        "-Wno-unused-but-set-variable",
        "-Wno-undef",
    ],
    visibility = ["//visibility:public"],
)
