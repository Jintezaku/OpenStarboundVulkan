#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCE_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

VULKANINFO_BIN=""
if command -v vulkaninfo >/dev/null 2>&1; then
  VULKANINFO_BIN="$(command -v vulkaninfo)"
elif [[ -x "$SOURCE_DIR/build/linux-vulkan-dev/vcpkg_installed/x64-linux/tools/vulkan-tools/vulkaninfo" ]]; then
  VULKANINFO_BIN="$SOURCE_DIR/build/linux-vulkan-dev/vcpkg_installed/x64-linux/tools/vulkan-tools/vulkaninfo"
elif [[ -x "$SOURCE_DIR/../vcpkg_installed/x64-linux/tools/vulkan-tools/vulkaninfo" ]]; then
  VULKANINFO_BIN="$SOURCE_DIR/../vcpkg_installed/x64-linux/tools/vulkan-tools/vulkaninfo"
else
  echo "vulkaninfo not found. Install vulkan-tools or build with the linux-vulkan-dev preset."
  exit 1
fi

echo "=== Vulkan Loader / Driver Summary ==="
echo "Using vulkaninfo binary: $VULKANINFO_BIN"
"$VULKANINFO_BIN" --summary
