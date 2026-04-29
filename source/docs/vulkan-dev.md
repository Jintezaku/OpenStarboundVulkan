# Vulkan Development (Local)

## What is wired

- Build options:
  - `STAR_ENABLE_VULKAN_RENDERER`
  - `STAR_ENABLE_OPENGL_RENDERER`
  - `STAR_ENABLE_IPO` (LTO/IPO when toolchain supports it)
- Renderer backend selection:
  - `+renderer=vulkan`
  - `+renderer=opengl`
  - `STAR_RENDERER_BACKEND` env variable
  - `auto` now prefers Vulkan when both backends are built
- SDL platform setup:
  - Creates `SDL_WINDOW_VULKAN` windows for Vulkan backend
  - Keeps existing OpenGL context path for OpenGL backend
- Renderer implementation:
  - `application/StarRenderer_vulkan.cpp` creates instance/device/surface/swapchain
  - Per-frame clear + present loop is implemented
- Multi-frame sync objects (2 frames in flight) are implemented
- Separate graphics/present queue-family configurations are supported
- Dedicated transfer queue selection and transient upload pools are implemented
- Host upload buffer + device-local scratch buffer allocation is implemented
- Upload buffer allocation now prefers cached/coherent host memory strategies with fallback
- Physical device scoring prefers higher-VRAM and discrete GPUs by default
- Swapchain recreation on resize/out-of-date is implemented
- Optional static pre-recorded swapchain command buffers reduce per-frame CPU recording overhead
- Vulkan pipeline cache persistence is available (disk-backed cache blob)
- Texture/effect paths are scaffolds for incremental migration
- Client-side renderer config lookup now prefers `/rendering/vulkan.config` for Vulkan and falls back to `/rendering/opengl.config`

## Optional Renderer Config

- `presentMode`: one of `mailbox`, `fifo`, `immediate`, `fifo_relaxed`
- `framesInFlight`: integer, clamped to `2..4`
- `uploadBufferMiB`: host/GPU upload scratch size in MiB (set `0` to disable)
- `swapchainImages`: explicit swapchain image count (`0` = auto)
- `staticCommandBuffers`: pre-record swapchain command buffers and reuse each frame
- `enablePipelineCache`: enable pipeline cache creation + persistence
- `pipelineCachePath`: file path for persisted cache blob (`""` uses OS-default cache path)
- `enableTransferQueue`: use dedicated transfer queue when available (`restart required`)
- `preferDiscreteGpu`: boolean
- `preferredGpuName`: substring filter for GPU device name
- `STAR_VK_PRESENT_MODE` env override with same values
- `STAR_VK_FRAMES_IN_FLIGHT`, `STAR_VK_UPLOAD_BUFFER_MB`, `STAR_VK_SWAPCHAIN_IMAGES`, `STAR_VK_STATIC_COMMAND_BUFFERS`
- `STAR_VK_ENABLE_PIPELINE_CACHE`, `STAR_VK_PIPELINE_CACHE_PATH`, `STAR_VK_ENABLE_TRANSFER_QUEUE`
- `STAR_VK_PREFER_DISCRETE`, `STAR_VK_GPU_NAME`

Detailed key reference: `docs/vulkan-renderer-config.md`.

## Local bootstrap

```bash
./scripts/dev/install_vulkan_toolchain.sh
./scripts/dev/bootstrap_vcpkg.sh
cmake --preset linux-vulkan-dev
cmake --build --preset linux-vulkan-dev -j

# one-shot local bootstrap (system deps + vcpkg + configure/build + vscode extensions + vulkan check)
./scripts/dev/setup_starbound_vulkan_local.sh

# install Codex skill assets explicitly
./scripts/dev/install_codex_assets.sh

# optional skips
SKIP_SYSTEM_PACKAGES=1 SKIP_VSCODE_EXTENSIONS=1 SKIP_CODEX_ASSETS=1 SKIP_BUILD=1 ./scripts/dev/setup_starbound_vulkan_local.sh

# choose no-IPO preset during one-shot setup
VULKAN_PRESET=linux-vulkan-dev-no-ipo ./scripts/dev/setup_starbound_vulkan_local.sh
```

If LTO/IPO links run out of disk space, switch to the no-IPO preset:

```bash
cmake --preset linux-vulkan-dev-no-ipo
cmake --build --preset linux-vulkan-dev-no-ipo -j
```

## Run

```bash
./scripts/dev/run_starbound_vulkan.sh

# comparison path
./scripts/dev/run_starbound_opengl.sh
```

## Short-term migration plan

1. Move primitive packing from `OpenGlRenderer::GlRenderBuffer` logic into a backend-neutral representation.
2. Implement Vulkan GPU buffers + descriptor sets for textured quads/triangles.
3. Add Vulkan shader pipeline and port existing default shader behavior.
4. Wire scriptable effect parameters/textures into Vulkan descriptor updates.
5. Add Vulkan ImGui renderer backend integration.
