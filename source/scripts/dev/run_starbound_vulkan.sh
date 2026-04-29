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

export STAR_RENDERER_BACKEND=vulkan

# Steam can inject Vulkan overrides/layer paths that break native SDL Vulkan window creation
# on some Deck sessions. Keep launch deterministic for this shortcut path.
if [[ -n "${SteamAppId:-}" || -n "${SteamGameId:-}" ]]; then
  # Discord init can stall startup in some Steam launch contexts; keep this
  # launcher deterministic and focused on renderer bring-up.
  export OPENSTARBOUND_DISABLE_DISCORD=1

  unset VK_ICD_FILENAMES
  unset VK_DRIVER_FILES
  unset VK_LAYER_PATH
  unset LD_PRELOAD

  unset ENABLE_VK_LAYER_VALVE_steam_overlay_1
  unset ENABLE_VK_LAYER_VALVE_steam_fossilize_1
  export DISABLE_VK_LAYER_VALVE_steam_overlay_1=1
  export DISABLE_VK_LAYER_VALVE_steam_fossilize_1=1

  {
    echo "=== OpenStarbound Steam Vulkan Debug ==="
    date -Is
    echo "SteamAppId=${SteamAppId:-}"
    echo "SteamGameId=${SteamGameId:-}"
    echo "--- environment ---"
    env | sort
    echo "--- vulkaninfo --summary ---"
    if command -v vulkaninfo >/dev/null 2>&1; then
      vulkaninfo --summary 2>&1 || true
    else
      echo "vulkaninfo not found"
    fi
    echo "=== end ==="
  } > "$DIST_DIR/steam-shortcut-debug.txt"
fi

# Opt-in validation layer support instead of forcing it by default.
if [[ "${STAR_VK_ENABLE_VALIDATION:-0}" == "1" && -z "${VK_INSTANCE_LAYERS:-}" ]]; then
  export VK_INSTANCE_LAYERS="VK_LAYER_KHRONOS_validation"
fi
export STAR_VK_PRESENT_MODE="${STAR_VK_PRESENT_MODE:-mailbox}"
export STAR_VK_FRAMES_IN_FLIGHT="${STAR_VK_FRAMES_IN_FLIGHT:-3}"
export STAR_VK_UPLOAD_BUFFER_MB="${STAR_VK_UPLOAD_BUFFER_MB:-128}"
export STAR_VK_SWAPCHAIN_IMAGES="${STAR_VK_SWAPCHAIN_IMAGES:-0}"
export STAR_VK_STATIC_COMMAND_BUFFERS="${STAR_VK_STATIC_COMMAND_BUFFERS:-1}"
export STAR_VK_ENABLE_PIPELINE_CACHE="${STAR_VK_ENABLE_PIPELINE_CACHE:-1}"
export STAR_VK_PIPELINE_CACHE_PATH="${STAR_VK_PIPELINE_CACHE_PATH:-${HOME:-$DIST_DIR}/.cache/openstarbound/vulkan.pipeline.cache}"
export STAR_VK_ENABLE_TRANSFER_QUEUE="${STAR_VK_ENABLE_TRANSFER_QUEUE:-1}"
export STAR_VK_PREFER_DISCRETE="${STAR_VK_PREFER_DISCRETE:-1}"

cd "$DIST_DIR"
exec "$APP" +renderer=vulkan "$@"
