#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCE_DIR="$(cd "$SCRIPT_DIR/../../.." && pwd)"

"$SOURCE_DIR/scripts/dev/install_vulkan_toolchain.sh"
"$SOURCE_DIR/scripts/dev/bootstrap_vcpkg.sh"
"$SOURCE_DIR/scripts/dev/install_codex_assets.sh"
