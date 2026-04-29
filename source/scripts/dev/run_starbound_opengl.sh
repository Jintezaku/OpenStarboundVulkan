#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCE_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
DIST_DIR="$SOURCE_DIR/../dist"
LIB_DIR="$SOURCE_DIR/../lib/linux"
APP="$DIST_DIR/starbound"

if [[ ! -x "$APP" ]]; then
  echo "Executable not found at $APP. Build first with: cmake --build --preset linux-vulkan-dev"
  exit 1
fi

# Keep runtime shared libraries (Steam/Discord) discoverable when launched via Steam shortcuts.
export LD_LIBRARY_PATH="$LIB_DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export STAR_RENDERER_BACKEND=opengl

cd "$DIST_DIR"
exec "$APP" +renderer=opengl "$@"
