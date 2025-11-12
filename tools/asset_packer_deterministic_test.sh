#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Deterministic output test for asset_packer.
# Verifies repeated runs produce identical C source output hash.
set -euo pipefail

ASSET_PACKER="$1"
if [ ! -x "$ASSET_PACKER" ]; then
  echo "asset_packer binary not executable: $ASSET_PACKER" >&2
  exit 1
fi

WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

# Create sample files (intentional unsorted creation order)
printf 'console.log("A");\n' > "$WORKDIR/a.js"
printf 'body { color: #123; }\n' > "$WORKDIR/b.css"

run_pack() {
  local out_prefix="$1"
  "$ASSET_PACKER" \
    --output-source "$WORKDIR/${out_prefix}.c" \
    --output-header "$WORKDIR/${out_prefix}.h" \
    --symbol-name assets_blob \
    "$WORKDIR/b.css" "$WORKDIR/a.js" >/dev/null
}

run_pack first
run_pack second

sha1_first="$(sha256sum "$WORKDIR/first.c" | awk '{print $1}')"
sha1_second="$(sha256sum "$WORKDIR/second.c" | awk '{print $1}')"

if [ "$sha1_first" != "$sha1_second" ]; then
  echo "Hash mismatch: $sha1_first vs $sha1_second" >&2
  exit 1
fi

# Basic symbol presence checks
if ! grep -q 'assets_blob_len' "$WORKDIR/first.c"; then
  echo "Missing assets_blob_len symbol in output" >&2
  exit 1
fi
if ! grep -q 'const unsigned char assets_blob[]' "$WORKDIR/first.c"; then
  echo "Missing assets_blob array in output" >&2
  exit 1
fi

# Ensure header declares extern symbols
if ! grep -q 'extern const unsigned char assets_blob\[' "$WORKDIR/first.h"; then
  echo "Header missing extern array declaration" >&2
  exit 1
fi
if ! grep -q 'extern const size_t assets_blob_len' "$WORKDIR/first.h"; then
  echo "Header missing extern length declaration" >&2
  exit 1
fi

echo "Deterministic asset_packer test passed (hash=$sha1_first)" >&2
