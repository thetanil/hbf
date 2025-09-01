#!/bin/bash

set -o errexit -o nounset -o pipefail

apt-get update && apt-get install -y --no-install-recommends \
    curl \
    ca-certificates \
    && apt-get clean && rm -rf /var/lib/apt/lists/*