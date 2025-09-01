# Neovim build file for external repository

# Export the Neovim executable
filegroup(
    name = "nvim_executable",
    srcs = ["bin/nvim"],
    visibility = ["//visibility:public"],
)

# Export the entire Neovim distribution
filegroup(
    name = "nvim_dist",
    srcs = glob(["**/*"]),
    visibility = ["//visibility:public"],
)