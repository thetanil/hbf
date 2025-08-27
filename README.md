Sync deps from go.mod:Sync deps from go.mod:
bazel run //:gazelle -- update-repos -from_file=go.mod
bazel run //:gazelle
bazel build //cmd/hbf:hbf
bazel run //cmd/hbf:hbf
