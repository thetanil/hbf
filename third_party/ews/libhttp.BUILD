"""
libhttp (Embedded Web Server) external build file
MIT licensed HTTP server library from https://github.com/lammertb/libhttp
"""

# Stub for buggy httplib_set_thread_name function
cc_library(
    name = "libhttp_stub",
    srcs = ["httplib_set_thread_name_stub.c"],
    copts = [
        "-Wno-error",
        "-Wno-missing-prototypes",
        "-Wno-missing-declarations",
    ],
)

cc_library(
    name = "libhttp",
    srcs = glob([
        "src/**/*.c",
        "src/**/*.h",
    ], exclude = [
        "src/**/LibHTTPServer.cpp",  # C++ file, we don't need it
        "src/**/osx_clock_gettime.c",  # Platform-specific
        "src/**/win32_clock_gettime.c",  # Platform-specific
        "src/**/wince_*.c",  # Windows CE specific
        "src/**/lh_ip_to_ipt.c",  # Has syntax errors in upstream, not needed (USE_IPV6=0)
        "src/**/lh_ipt_to_ip.c",  # IPv6 related, not needed
        "src/**/lh_ipt_to_ip4.c",  # IPv6 related, not needed
        "src/**/lh_ipt_to_ip6.c",  # IPv6 related, not needed
        "src/**/httplib_set_thread_name.c",  # Has undefined variable error in upstream
    ]),
    hdrs = glob(["include/**/*.h"]),
    copts = [
        "-std=c99",
        "-Wall",
        "-Wextra",
        # Disable errors for third-party code
        "-Wno-error",
        "-Wno-sign-conversion",
        "-Wno-conversion",
        "-Wno-shadow",
        "-Wno-cast-qual",
        "-Wno-unused-parameter",
        "-Wno-unused-variable",
        "-Wno-unused-function",
        "-Wno-cpp",  # Disable preprocessor warnings (musl's sys/poll.h redirect)
        "-fcommon",  # Allow multiple definitions (libhttp has static_assert_replacement bug)
        # libhttp configuration flags
        "-DNO_SSL",  # No SSL/TLS support (use reverse proxy)
        "-DNO_CGI",  # No CGI support
        "-DNO_FILES",  # No built-in file serving (we handle via SQLite)
        "-DUSE_IPV6=0",  # IPv4 only
        "-DUSE_WEBSOCKET",  # Enable WebSocket support for future SSE/WS
        "-DNO_THREAD_NAME",  # Disable thread naming (httplib_set_thread_name has bugs)
    ],
    includes = ["include", "src"],
    deps = [":libhttp_stub"],  # Provide stub for buggy functions
    visibility = ["//visibility:public"],
)
