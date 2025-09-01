# hbf

https://github.com/libsdl-org/SDL/releases/tag/release-2.32.8

build sdl2 with audio and opengl 3.3 with bazel, which i will import into a
golang bazel project

we are on the bazel-sdl2 branch. use rules_foreign_cc
compiles SDL2 as a Bazel cc_library.
this is a bzlmod bazel project
do not create a WORKSPACE

just write a plan to do this to plan.md

# add bazel to container

modify .devcontainer/Dockerfile with the following. the goal is to get bazel
into the existing dockerfile which is for claude and should remain node based

FROM ubuntu:22.04 AS base_image

RUN --mount=source=bazel/oci/install_packages.sh,target=/mnt/install_packages.sh,type=bind \
 /mnt/install_packages.sh

FROM base_image AS downloader

ARG TARGETARCH BAZEL_VERSION

WORKDIR /var/bazel
RUN --mount=source=bazel/oci/install_bazel.sh,target=/mnt/install_bazel.sh,type=bind \
 /mnt/install_bazel.sh

FROM base_image

RUN useradd --system --create-home --home-dir=/home/ubuntu --shell=/bin/bash --gid=root --groups=sudo --uid=1000 ubuntu
USER ubuntu
WORKDIR /home/ubuntu
COPY --from=downloader /var/bazel/bazel /usr/local/bin/bazel
ENTRYPOINT ["/usr/local/bin/bazel"]

the dockerfile will now look for the folling script when it starts

```sh
# install_bazel.sh
#!/bin/bash

set -o errexit -o nounset -o pipefail

if [ "$TARGETARCH" = "amd64" ]; then
  TARGETARCH="x86_64"
fi

curl \
    --fail \
    --location \
    --remote-name \
    "https://github.com/bazelbuild/bazel/releases/download/${BAZEL_VERSION}/bazel-${BAZEL_VERSION}-linux-${TARGETARCH}"

curl \
    --fail \
    --location \
    "https://github.com/bazelbuild/bazel/releases/download/${BAZEL_VERSION}/bazel-${BAZEL_VERSION}-linux-${TARGETARCH}.sha256" \
    | sha256sum --check

mv "bazel-${BAZEL_VERSION}-linux-${TARGETARCH}" bazel
chmod +x bazel
```

the result should be the devcontainer we have now and add the bazel feature to
it.

now i can

claude --dangerously-skip-permissions
and claude can run bazel. do not run bazelisk
node@6d77ca68381d:/workspace$ bazel --version
bazel 8.3.1
do not install bazelisk
