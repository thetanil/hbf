FROM mcr.microsoft.com/vscode/devcontainers/python:3.13

# Install system dependencies including GUI support for Wayland
RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
    && apt-get -y install --no-install-recommends \
    curl \
    git \
    xxd \
    gdb \
    vim \
    sqlite3 \
    clang-tidy-18 \
    llvm-18 \
    apache2-utils \
    apt-transport-https \
    gnupg \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Add Bazel APT repository and install Bazel
RUN curl -fsSL https://bazel.build/bazel-release.pub.gpg | gpg --dearmor > /usr/share/keyrings/bazel-archive-keyring.gpg \
    && echo "deb [signed-by=/usr/share/keyrings/bazel-archive-keyring.gpg] https://storage.googleapis.com/bazel-apt stable jdk1.8" > /etc/apt/sources.list.d/bazel.list \
    && apt-get update \
    && apt-get install -y bazel \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Install Buildifier (Bazel build file formatter/linter)
# Uses the latest release binary from bazelbuild/buildtools
RUN set -eux; \
    arch="$(uname -m)"; \
    case "$arch" in \
        x86_64|amd64) suffix=amd64 ;; \
        aarch64|arm64) suffix=arm64 ;; \
        *) echo "Unsupported architecture: $arch"; exit 1 ;; \
    esac; \
    curl -fsSL -o /usr/local/bin/buildifier "https://github.com/bazelbuild/buildtools/releases/latest/download/buildifier-linux-${suffix}"; \
    chmod +x /usr/local/bin/buildifier; \
    /usr/local/bin/buildifier -version

# Install Docker CLI (for docker-in-docker)
# RUN curl -fsSL https://get.docker.com | sh

# Install docker-compose
# RUN curl -L "https://github.com/docker/compose/releases/latest/download/docker-compose-$(uname -s)-$(uname -m)" -o /usr/local/bin/docker-compose \
#     && chmod +x /usr/local/bin/docker-compose

# Create Python virtual environment
RUN python -m venv /opt/venv
ENV PATH="/opt/venv/bin:$PATH"

# Install common Python packages
# COPY ../requirements-django.txt ../requirements-fastapi.txt /tmp/
# RUN pip install --no-cache-dir -r /tmp/requirements-django.txt -r /tmp/requirements-fastapi.txt

# Configure existing vscode user
ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=$USER_UID

# Install sudo and configure user (vscode user already exists in base image)
RUN apt-get update \
    && apt-get install -y sudo \
    && echo $USERNAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USERNAME \
    && chmod 0440 /etc/sudoers.d/$USERNAME \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Add user to docker group
# RUN usermod -aG docker $USERNAME

USER $USERNAME

# Set workspace directory
WORKDIR /workspaces/hbf

# Install Claude CLI according to official Anthropic documentation
RUN curl -fsSL https://claude.ai/install.sh | bash \
    && echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.bashrc

# Ensure Claude is in PATH
ENV PATH="/home/vscode/.local/bin:$PATH"
