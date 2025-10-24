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
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

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
