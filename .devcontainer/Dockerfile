FROM debian:bookworm-slim

# Install essential packages and dependencies
RUN apt-get update && apt-get install -y \
    curl \
    git \
    build-essential \
    pkg-config \
    ca-certificates \
    sudo \
    wget \
    unzip \
    # raylib go dependencies
    libgl1-mesa-dev \
    libxi-dev \
    libxcursor-dev \
    libxrandr-dev \
    libxinerama-dev \
    libwayland-dev \
    libxkbcommon-dev \
    # python3 \
    # python3-pip \
    # Raylib dependencies
    # libasound2-dev \
    # mesa-common-dev \
    # libx11-dev \
    # libxrandr-dev \
    # libxi-dev \
    # xorg-dev \
    # libgl1-mesa-dev \
    # libglu1-mesa-dev \
    # Neovim dependencies
    neovim \
    && rm -rf /var/lib/apt/lists/*

# Install Go
ARG GO_VERSION=1.25.0
RUN wget -O go.tar.gz "https://golang.org/dl/go${GO_VERSION}.linux-amd64.tar.gz" \
    && tar -C /usr/local -xzf go.tar.gz \
    && rm go.tar.gz

# Create vscode user
RUN useradd -m -s /bin/bash vscode \
    && echo "vscode ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

# Set up environment
ENV PATH="/usr/local/go/bin:${PATH}"
ENV GOPROXY="https://proxy.golang.org,direct"
ENV GOSUMDB="sum.golang.org"

# Create workspace directory
RUN mkdir -p /workspace && chown vscode:vscode /workspace

# Switch to vscode user
USER vscode
WORKDIR /workspace

# Set GOPATH to user's home directory and install gopls as vscode user
ENV GOPATH="/home/vscode/go"
ENV PATH="/home/vscode/go/bin:/usr/local/go/bin:${PATH}"
RUN go install golang.org/x/tools/gopls@latest

# Install Bazel as vscode user
ARG BAZEL_VERSION=8.3.1
RUN wget -O bazel-installer.sh "https://github.com/bazelbuild/bazel/releases/download/${BAZEL_VERSION}/bazel-${BAZEL_VERSION}-installer-linux-x86_64.sh" \
    && chmod +x bazel-installer.sh \
    && ./bazel-installer.sh --user \
    && rm bazel-installer.sh

# Add bazel to PATH for vscode user
ENV PATH="/home/vscode/.bazel/bin:${PATH}"

# Initialize Bazel for the user
RUN bazel version

# Set default shell
SHELL ["/bin/bash", "-c"]