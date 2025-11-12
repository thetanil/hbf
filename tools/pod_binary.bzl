"""Pod binary build macro for HBF.

This macro generates a complete HBF binary from a pod directory.
It handles:
1. Converting pod SQLAR database to C byte array via db_to_c
2. Creating a cc_library with the embedded data
3. Building the final cc_binary with all dependencies
"""

def pod_binary(name, pod, visibility = None, tags = None):
    """Build an HBF binary with an embedded pod.

    Args:
        name: Name of the binary target (e.g., "hbf" or "hbf_test")
        pod: Label of the pod target (e.g., "//pods/base")
        visibility: Visibility for the binary target
        tags: Tags to apply to all targets (e.g., ["hbf_pod"])
    """

    # Internal target names
    pod_db = pod + ":pod_db"
    pod_assets = pod + ":embedded_assets"
    embedded_gen = name + "_embedded_gen"
    embedded_lib = name + "_embedded"
    binary = name + "_bin"

    # Convert pod database to C source files using db_to_c
    # Generate with standard names (fs_embedded.c/h) then the build system handles them
    native.genrule(
        name = embedded_gen,
        srcs = [pod_db],
        outs = [
            name + "_fs_embedded.c",
            name + "_fs_embedded.h",
        ],
        cmd = """
            # Generate with standard names
            $(location //tools:db_to_c) $< $(location """ + name + """_fs_embedded.c) $(location """ + name + """_fs_embedded.h)

            # Fix the include in the C file to use the actual generated header name
            sed -i 's|#include "fs_embedded.h"|#include \"""" + name + """_fs_embedded.h"|' $(location """ + name + """_fs_embedded.c)
        """,
        tools = ["//tools:db_to_c"],
        tags = tags,
    )

    # Create library with embedded pod database
    native.cc_library(
        name = embedded_lib,
        srcs = [":" + name + "_fs_embedded.c"],
        hdrs = [":" + name + "_fs_embedded.h"],
        visibility = ["//visibility:public"],
        tags = tags,
        alwayslink = True,  # Ensure symbols are included even if not directly referenced
    )

    # Create cc_binary that links everything together
    # This binary depends on the embedded library we just created
    native.cc_binary(
        name = binary,
        srcs = ["//hbf/shell:main.c"],
        deps = [
            ":" + embedded_lib,
            pod_assets,  # Link the new asset bundle
            "//hbf/shell:config",
            "//hbf/shell:log",
            "//hbf/db:db",
            "//hbf/http:server",
            "//hbf/qjs:engine",
            "@sqlite3",
        ],
        copts = [],
        linkopts = [],
        visibility = ["//visibility:private"],
        tags = tags,
    )

    # Create final binary target in bin/ directory
    # The binary is copied as-is; use --strip=always for stripped builds
    native.genrule(
        name = name,
        srcs = [":" + binary],
        outs = ["bin/" + name],
        cmd = "mkdir -p `dirname $@` && cp $< $@ && chmod +x $@",
        executable = True,
        visibility = visibility,
        tags = tags,
    )

    # Optional convenience alias for unstripped binary (points to same target)
    native.alias(
        name = name + "_unstripped",
        actual = ":" + name,
        visibility = visibility,
        tags = tags,
    )
