# QuickJS-NG build configuration (MIT License)
# External build file for git_repository

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "quickjs",
    srcs = [
        "quickjs.c",
        "cutils.c",
        "libregexp.c",
        "libunicode.c",
        "libbf.c",
    ],
    hdrs = [
        "quickjs.h",
        "quickjs-atom.h",
        "quickjs-opcode.h",
        "quickjs-c-atomics.h",
        "cutils.h",
        "libregexp.h",
        "libregexp-opcode.h",
        "libunicode.h",
        "libunicode-table.h",
        "libbf.h",
        "list.h",
    ],
    copts = [
        "-std=c99",
        "-Wall",
        "-Wextra",
        "-Wno-unused-parameter",
        "-Wno-sign-compare",
        "-Wno-missing-field-initializers",
        "-Wno-implicit-fallthrough",
        "-Wno-unused-function",
        "-Wno-undef",
        "-Wno-conversion",
        "-Wno-sign-conversion",
        "-Wno-cast-qual",
        "-Wno-switch-default",
        "-Wno-unused-variable",
        "-Wno-unused-but-set-variable",
        "-Wno-shadow",
        "-DCONFIG_VERSION=\\\"0.8.0\\\"",
        "-DCONFIG_BIGNUM",
        "-D_GNU_SOURCE",
    ],
    includes = ["."],
)
