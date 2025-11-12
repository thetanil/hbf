"""Pod binary build macro for HBF.

This macro generates a complete HBF binary from a pod directory.
It handles building the final cc_binary with all dependencies including
the asset bundle from asset_packer.
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
    pod_assets = pod + ":embedded_assets"
    binary = name + "_bin"

    # Create cc_binary that links everything together
    native.cc_binary(
        name = binary,
        srcs = ["//hbf/shell:main.c"],
        deps = [
            pod_assets,  # Link the asset bundle
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
