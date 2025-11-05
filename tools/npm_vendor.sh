#!/bin/bash
# npm_vendor.sh - Vendor npm packages into pods for reproducible builds
#
# Usage: tools/npm_vendor.sh --pod <base|test|...> <package@version> [...]
#
# Example: tools/npm_vendor.sh --pod base @codemirror/view@6.25.0

set -euo pipefail

POD=""
PACKAGES=()

# Parse arguments
while [[ $# -gt 0 ]]; do
  case $1 in
    --pod)
      POD="$2"
      shift 2
      ;;
    *)
      PACKAGES+=("$1")
      shift
      ;;
  esac
done

# Validate inputs
if [[ -z "$POD" ]]; then
  echo "Error: --pod flag is required"
  echo "Usage: $0 --pod <base|test|...> <package@version> [...]"
  exit 1
fi

if [[ ${#PACKAGES[@]} -eq 0 ]]; then
  echo "Error: At least one package specification is required"
  echo "Usage: $0 --pod <base|test|...> <package@version> [...]"
  exit 1
fi

POD_DIR="pods/$POD"
if [[ ! -d "$POD_DIR" ]]; then
  echo "Error: Pod directory $POD_DIR does not exist"
  exit 1
fi

# Ensure required tools are available
if ! command -v npm &> /dev/null; then
  echo "Error: npm is required but not installed"
  exit 1
fi

if ! command -v npx &> /dev/null; then
  echo "Error: npx is required but not installed"
  exit 1
fi

# Create output directories
VENDOR_DIR="$POD_DIR/static/vendor/esm"
THIRD_PARTY_DIR="third_party/npm"
mkdir -p "$VENDOR_DIR"
mkdir -p "$THIRD_PARTY_DIR"

# Initialize or load import map
IMPORT_MAP="$POD_DIR/static/importmap.json"
if [[ -f "$IMPORT_MAP" ]]; then
  IMPORT_MAP_CONTENT=$(cat "$IMPORT_MAP")
else
  IMPORT_MAP_CONTENT='{"imports":{}}'
  echo "$IMPORT_MAP_CONTENT" > "$IMPORT_MAP"
fi

# Function to convert package name to safe filename
# @codemirror/view -> codemirror-view
# lodash-es -> lodash-es
safe_name() {
  echo "$1" | sed 's/@//g' | sed 's/\//-/g'
}

# Function to extract package name and version from spec
# @codemirror/view@6.25.0 -> name=@codemirror/view version=6.25.0
parse_package_spec() {
  local spec="$1"

  # Handle scoped packages: @scope/name@version
  if [[ "$spec" =~ ^(@[^@]+)@(.+)$ ]]; then
    echo "${BASH_REMATCH[1]} ${BASH_REMATCH[2]}"
  # Handle regular packages: name@version
  elif [[ "$spec" =~ ^([^@]+)@(.+)$ ]]; then
    echo "${BASH_REMATCH[1]} ${BASH_REMATCH[2]}"
  else
    echo "Error: Invalid package spec format: $spec"
    echo "Expected format: package@version or @scope/package@version"
    exit 1
  fi
}

# Function to update import map
update_import_map() {
  local pkg_name="$1"
  local safe_pkg_name="$2"
  local vendor_path="/static/vendor/esm/${safe_pkg_name}.js"

  # Use jq to update the import map
  if command -v jq &> /dev/null; then
    jq --arg key "$pkg_name" --arg value "$vendor_path" \
      '.imports[$key] = $value' "$IMPORT_MAP" > "${IMPORT_MAP}.tmp"
    mv "${IMPORT_MAP}.tmp" "$IMPORT_MAP"
  else
    # Fallback: manual JSON manipulation (simple case)
    echo "Warning: jq not found, using basic JSON manipulation"
    local content=$(cat "$IMPORT_MAP")
    if [[ "$content" =~ \{\} ]]; then
      echo "{\"imports\":{\"$pkg_name\":\"$vendor_path\"}}" > "$IMPORT_MAP"
    else
      # This is a simplified approach - in production you'd want jq
      echo "Warning: Import map exists but jq is not available for safe updates"
      echo "Please manually add: \"$pkg_name\": \"$vendor_path\" to $IMPORT_MAP"
    fi
  fi
}

# Process each package
for PACKAGE_SPEC in "${PACKAGES[@]}"; do
  echo ""
  echo "Processing $PACKAGE_SPEC..."

  # Parse package spec
  read -r PKG_NAME PKG_VERSION <<< "$(parse_package_spec "$PACKAGE_SPEC")"

  echo "  Package: $PKG_NAME"
  echo "  Version: $PKG_VERSION"

  # Create safe names for directories and files
  SAFE_NAME=$(safe_name "$PKG_NAME")
  PKG_DIR_NAME="${PKG_NAME//\//__}@${PKG_VERSION}"
  EXTRACT_DIR="$THIRD_PARTY_DIR/$PKG_DIR_NAME"

  # Create temporary work directory
  WORK_DIR=$(mktemp -d)

  echo "  Downloading package..."
  npm pack "$PACKAGE_SPEC" --silent --pack-destination="$WORK_DIR"

  # Find the downloaded tarball
  TARBALL=$(ls "$WORK_DIR"/*.tgz 2>/dev/null | head -n1)
  if [[ -z "$TARBALL" ]]; then
    echo "Error: Failed to download package $PACKAGE_SPEC"
    rm -rf "$WORK_DIR"
    exit 1
  fi

  echo "  Extracting to $EXTRACT_DIR..."
  mkdir -p "$EXTRACT_DIR/pkg"
  tar -xzf "$TARBALL" -C "$EXTRACT_DIR/pkg" --strip-components=1

  # Clean up temporary directory
  rm -rf "$WORK_DIR"

  # Copy LICENSE file
  if [[ -f "$EXTRACT_DIR/pkg/LICENSE" ]]; then
    cp "$EXTRACT_DIR/pkg/LICENSE" "$EXTRACT_DIR/"
  elif [[ -f "$EXTRACT_DIR/pkg/LICENSE.md" ]]; then
    cp "$EXTRACT_DIR/pkg/LICENSE.md" "$EXTRACT_DIR/"
  elif [[ -f "$EXTRACT_DIR/pkg/LICENSE.txt" ]]; then
    cp "$EXTRACT_DIR/pkg/LICENSE.txt" "$EXTRACT_DIR/"
  else
    echo "Warning: No LICENSE file found for $PKG_NAME"
  fi

  # Find entry point from package.json
  PKG_JSON="$EXTRACT_DIR/pkg/package.json"
  if [[ ! -f "$PKG_JSON" ]]; then
    echo "Error: package.json not found in $EXTRACT_DIR/pkg"
    exit 1
  fi

  # Determine entry point (prefer module field, fallback to main)
  if command -v jq &> /dev/null; then
    ENTRY=$(jq -r '.module // .main // "index.js"' "$PKG_JSON")
  else
    # Simple grep-based fallback
    ENTRY=$(grep -m1 '"module"' "$PKG_JSON" | cut -d'"' -f4)
    if [[ -z "$ENTRY" ]]; then
      ENTRY=$(grep -m1 '"main"' "$PKG_JSON" | cut -d'"' -f4)
    fi
    if [[ -z "$ENTRY" ]]; then
      ENTRY="index.js"
    fi
  fi

  ENTRY_PATH="$EXTRACT_DIR/pkg/$ENTRY"
  if [[ ! -f "$ENTRY_PATH" ]]; then
    echo "Error: Entry point $ENTRY_PATH does not exist"
    exit 1
  fi

  echo "  Entry point: $ENTRY"

  # Install dependencies in the package directory
  echo "  Installing dependencies..."
  (cd "$EXTRACT_DIR/pkg" && npm install --production --no-package-lock --no-audit --ignore-scripts > /dev/null 2>&1)

  # Bundle with esbuild
  OUTPUT_FILE="$VENDOR_DIR/${SAFE_NAME}.js"
  echo "  Bundling to $OUTPUT_FILE..."

  npx esbuild "$ENTRY_PATH" \
    --bundle \
    --format=esm \
    --sourcemap \
    --outfile="$OUTPUT_FILE"

  echo "  Bundle size: $(du -h "$OUTPUT_FILE" | cut -f1)"

  # Update import map
  echo "  Updating import map..."
  update_import_map "$PKG_NAME" "$SAFE_NAME"

  echo "✓ Successfully vendored $PKG_NAME@$PKG_VERSION"
done

echo ""
echo "✓ All packages vendored successfully for pod '$POD'"
echo ""
echo "Created/updated:"
echo "  - $VENDOR_DIR/*.js (bundled ESM)"
echo "  - $VENDOR_DIR/*.js.map (source maps)"
echo "  - $IMPORT_MAP (import map)"
echo "  - $THIRD_PARTY_DIR/*/ (source packages + licenses)"
echo ""
echo "Next steps:"
echo "  1. Review the generated files"
echo "  2. Test the import in your HTML:"
echo "     <script type=\"importmap\" src=\"/static/importmap.json\"></script>"
echo "  3. Commit the changes to git"
