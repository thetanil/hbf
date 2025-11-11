"""
miniz - Single-file public domain deflate/inflate, zlib-subset implementation
License: Public Domain (Unlicense)
Source: https://github.com/richgel999/miniz
"""

# Generate miniz_export.h for export declarations (empty for static build)
genrule(
    name = "gen_export_header",
    outs = ["miniz_export.h"],
    cmd = "echo '#ifndef MINIZ_EXPORT\n#define MINIZ_EXPORT\n#endif' > $@",
)

cc_library(
    name = "miniz",
    srcs = [
        "miniz.c",
        "miniz_common.h",
        "miniz_tdef.c",
        "miniz_tdef.h",
        "miniz_tinfl.c",
        "miniz_tinfl.h",
        "miniz_zip.c",
        "miniz_zip.h",
    ],
    hdrs = [
        "miniz.h",
        ":gen_export_header",
    ],
    copts = [
        "-std=c99",
        "-Wall",
        "-Wextra",
        # Disable warnings-as-errors for third-party code
        "-Wno-error",
        "-Wno-sign-conversion",
        "-Wno-conversion",
        "-Wno-shadow",
        "-Wno-cast-qual",
        "-Wno-implicit-fallthrough",
    ],
    includes = ["."],
    visibility = ["//visibility:public"],
)
