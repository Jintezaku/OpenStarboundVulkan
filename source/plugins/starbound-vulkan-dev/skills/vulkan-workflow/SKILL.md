---
name: vulkan-workflow
description: Use the local OpenStarbound Vulkan development workflow to bootstrap dependencies, build with the Vulkan preset, validate the machine stack, and run the client with the Vulkan backend.
---

# Vulkan Workflow

## Commands

```bash
# 1) Install platform tooling and vcpkg deps
./plugins/starbound-vulkan-dev/scripts/bootstrap.sh

# Optional one-shot setup from plugin wrapper
./plugins/starbound-vulkan-dev/scripts/setup-local.sh

# Optional all-in-one local setup
./scripts/dev/setup_starbound_vulkan_local.sh

# 2) Configure + build Vulkan dev preset
cmake --preset linux-vulkan-dev
cmake --build --preset linux-vulkan-dev -j

# 3) Verify Vulkan runtime stack
./plugins/starbound-vulkan-dev/scripts/check-vulkan.sh

# 4) Run the client on Vulkan
./plugins/starbound-vulkan-dev/scripts/run-vulkan.sh

# 5) Optional OpenGL comparison run
./plugins/starbound-vulkan-dev/scripts/run-opengl.sh

# 6) Install workspace-recommended VS Code extensions
./plugins/starbound-vulkan-dev/scripts/install-vscode-extensions.sh

# 7) Install Codex skill assets
./plugins/starbound-vulkan-dev/scripts/install-codex-assets.sh

# Optional: tweak swap behavior
STAR_VK_PRESENT_MODE=immediate ./plugins/starbound-vulkan-dev/scripts/run-vulkan.sh

# Optional deeper queue/upload tuning
STAR_VK_FRAMES_IN_FLIGHT=4 STAR_VK_UPLOAD_BUFFER_MB=256 STAR_VK_SWAPCHAIN_IMAGES=5 STAR_VK_STATIC_COMMAND_BUFFERS=1 STAR_VK_ENABLE_PIPELINE_CACHE=1 STAR_VK_PIPELINE_CACHE_PATH="$HOME/.cache/openstarbound/vulkan.pipeline.cache" STAR_VK_ENABLE_TRANSFER_QUEUE=1 STAR_VK_PREFER_DISCRETE=1 ./plugins/starbound-vulkan-dev/scripts/run-vulkan.sh
```

The `linux-vulkan-dev` preset enables `STAR_ENABLE_IPO` for LTO/IPO when the compiler toolchain supports it.
If low disk space causes link failures, use `linux-vulkan-dev-no-ipo`.
Setup skip knobs: `SKIP_SYSTEM_PACKAGES=1`, `SKIP_VSCODE_EXTENSIONS=1`, `SKIP_CODEX_ASSETS=1`, `SKIP_BUILD=1`.
Preset override for one-shot setup: `VULKAN_PRESET=linux-vulkan-dev-no-ipo`.

## Notes

- The runtime sets `STAR_RENDERER_BACKEND=vulkan`.
- The plugin bootstrap installs Codex skill assets in addition to toolchain deps.
- To force OpenGL fallback for comparison: `./scripts/dev/run_starbound_opengl.sh`.
- Vulkan renderer config path is `/rendering/vulkan.config` with automatic fallback to `/rendering/opengl.config`.
