#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCE_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
VULKAN_PRESET="${VULKAN_PRESET:-linux-vulkan-dev}"

echo "Using CMake preset: ${VULKAN_PRESET}"

source "$SCRIPT_DIR/ensure_tmpdir.sh"
ensure_starbound_tmpdir

if [[ "${SKIP_SYSTEM_PACKAGES:-0}" != "1" ]]; then
  "$SCRIPT_DIR/install_vulkan_toolchain.sh"
else
  echo "Skipping system package installation (SKIP_SYSTEM_PACKAGES=1)."
fi

"$SCRIPT_DIR/bootstrap_vcpkg.sh"

cmake --preset "$VULKAN_PRESET"
if [[ "${SKIP_BUILD:-0}" != "1" ]]; then
  cmake --build --preset "$VULKAN_PRESET" -j"${BUILD_JOBS:-$(nproc)}"
else
  echo "Skipping build step (SKIP_BUILD=1)."
fi

if [[ "${SKIP_VSCODE_EXTENSIONS:-0}" != "1" ]]; then
  "$SCRIPT_DIR/install_vscode_extensions.sh"
else
  echo "Skipping VS Code extension installation (SKIP_VSCODE_EXTENSIONS=1)."
fi

if [[ "${SKIP_CODEX_ASSETS:-0}" != "1" ]]; then
  "$SCRIPT_DIR/install_codex_assets.sh"
else
  echo "Skipping Codex skill installation (SKIP_CODEX_ASSETS=1)."
fi

"$SCRIPT_DIR/check_vulkan_stack.sh"

echo "OpenStarbound Vulkan local setup completed."
