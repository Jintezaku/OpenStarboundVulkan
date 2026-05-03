# OpenStarboundVulkan

OpenStarboundVulkan is a personal fork of [OpenStarbound](https://github.com/OpenStarbound/OpenStarbound), focused on Vulkan renderer development and Steam Deck-oriented performance/quality work.

This repository is published for people who want to test or use those fork-specific changes. It is not an official Starbound project.

## Fork Focus

This README intentionally documents what is specific to this fork.

Current focus areas:

- Vulkan renderer backend implementation and tuning
- Steam Deck-first stability/performance iteration
- Renderer backend parity testing (`vulkan` vs `opengl`)
- Personal quality-of-life and bug-fix changes that are useful in day-to-day play

## What Is Different Here

Compared with upstream OpenStarbound, this fork currently adds and prioritizes:

- Vulkan-first local dev workflow and scripts in `source/scripts/dev/`
- Backend selection flow via `+renderer=...` and `STAR_RENDERER_BACKEND`
- Vulkan runtime configuration via `/rendering/vulkan.config`
- Vulkan-specific environment overrides (`STAR_VK_*` keys)
- Vulkan device/present/swapchain/cache tuning hooks documented in `source/docs/vulkan-renderer-config.md`

## Optimization Delta Vs Upstream OpenStarbound

This fork includes the following engine/performance optimizations that are not part of upstream OpenStarbound:

- Vulkan renderer compatibility path for world lighting/fog-of-war:
  - Implements `lightMapEnabled`, `lightMapMultiplier`, `lightMapScale`, and `lightMapOffset` effect parameters in Vulkan.
  - Implements `lightMap` effect texture ingestion in Vulkan (including float lightmap formats).
  - Applies world lightmap shading during Vulkan vertex packing so world shadowing/fog-of-war is functional.
  - Disables world lightmap effect state after world rendering so UI/HUD/inventory/item icons are not affected by world lighting.

- Tile rendering architecture upgrades (`StarTilePainter`):
  - Adaptive terrain/liquid chunk build budgets with overload-aware scaling.
  - Adaptive chunk-hash refresh cadence.
  - Distance-weighted chunk-hash cadence for far chunks.
  - Visible chunk priority scheduling (camera-near chunks first).
  - Critical synchronous build budget for visible chunks to prevent visible holes.
  - Chunk prefetch ring around visible range with per-frame prefetch budgeting.
  - Hash-retention window aligned to prefetch range.
  - Config-plumbed controls for all of the above in `rendering.config`.

- Benchmark/stress framework expansion (`StarClientApplication`):
  - Isolated benchmark storage path support for repeatable runs separated from user data.
  - Automatic benchmark player provisioning for each run (new benchmark profile/ship lifecycle).
  - Late-game terrestrial target world selection with threat-floor control.
  - Expanded ~5 minute stress sequence covering:
    - movement/jump pulses
    - AI/pathfinding load waves
    - high-volume item/NPC/monster/liquid stress
    - terrain destruction/rebuild pulses
    - repeated explosion pulses
    - heavy weather pulses
  - Extreme weather cycle selection from available world weather definitions (meteor/ember/storm-style effects prioritized when present).
  - Weather stress gated to planetary worlds (disabled while in ship world).
  - Cursor-independent direct stress spawning via server-side entity/item/liquid creation (no cursor placement dependency).
  - Stress entity trimming pass to prevent unbounded buildup during long runs.
  - Dead-player recovery handling during stress updates to preserve benchmark continuity.
  - Extended stress telemetry in benchmark JSON output (AI waves, liquid writes, terrain damage/rebuild counts, trim counters, weather pulses, etc.).

- Upstream test-target compatibility fix:
  - `StarTestUniverse` updated to use `entityDrawablesArena` path expected by current render data layout, restoring test target buildability.

## What Is Inherited From Upstream

This project still carries the large OpenStarbound feature base. Instead of duplicating the upstream feature list here, use:

- OpenStarbound repository: <https://github.com/OpenStarbound/OpenStarbound>
- Upstream README: <https://github.com/OpenStarbound/OpenStarbound/blob/main/README.md>

## Legal Requirement

You must own a legal copy of Starbound.

This fork does not ship the base game assets. You need at minimum:

```text
Starbound/assets/packed.pak
```

Copy that file into this fork's `assets/` directory before running.

## Install (Prebuilt Releases)

1. Download a release from: <https://github.com/Jintezaku/OpenStarboundVulkan/releases>
2. Extract/install it into a separate directory.
3. Copy `packed.pak` from your Starbound install.
4. Optional: copy your `storage/` and `mods/` folders.
5. Launch the forked client/server binaries.

Recommended: back up `storage/` before testing dev/nightly builds.

## Build From Source

Primary source tree is `source/`.

### Vulkan-first setup (Linux)

```bash
cd source
./scripts/dev/setup_starbound_vulkan_local.sh
```

### Manual setup path

```bash
cd source
./scripts/dev/install_vulkan_toolchain.sh
./scripts/dev/bootstrap_vcpkg.sh
cmake --preset linux-vulkan-dev
cmake --build --preset linux-vulkan-dev -j
```

If linker/IPO pressure is too high on your machine:

```bash
cd source
cmake --preset linux-vulkan-dev-no-ipo
cmake --build --preset linux-vulkan-dev-no-ipo -j
```

## Run And Compare Backends

From `source/`:

```bash
./scripts/dev/run_starbound_vulkan.sh
./scripts/dev/run_starbound_opengl.sh
```

Equivalent direct backend controls:

- CLI args: `+renderer=vulkan` or `+renderer=opengl`
- Environment: `STAR_RENDERER_BACKEND=vulkan` or `STAR_RENDERER_BACKEND=opengl`

## Vulkan Configuration

Fork-specific Vulkan keys and env overrides are documented in:

- `source/docs/vulkan-renderer-config.md`
- `source/docs/vulkan-dev.md`

These include present mode, frames in flight, upload/scratch buffer sizing, swapchain image count, pipeline cache persistence, and GPU preference controls.

## Platform Build Presets

CMake presets are defined in `source/CMakePresets.json` for:

- Linux (`linux-release`, `linux-vulkan-dev`, `linux-vulkan-dev-no-ipo`, clang/arm variants)
- Windows (`windows-release`, `windows-release-VS2022`)
- macOS (`macos-release`, `macos-arm-release`)

## Status

This is an actively changing personal fork. Some changes are experimental and may be rewritten.

For safest usage:

- prefer tagged releases over in-progress branch builds
- keep save backups before upgrading
- test without large mod stacks first

## Credits

- Base engine fork and broad feature foundation: OpenStarbound contributors
- Original game/IP and assets: Chucklefish (Starbound)

OpenStarboundVulkan is independent and unofficial.
