# OpenStarboundVulkan

**OpenStarboundVulkan** is a personal fork of [OpenStarbound](https://github.com/OpenStarbound/OpenStarbound), itself a modernized fork of **Starbound 1.4.4**.

This fork started as an experiment to bring **Vulkan-focused improvements** to Starbound for use on my own **Steam Deck**. Since other players may also benefit from those changes, the project is being shared publicly.

The project may grow over time with Vulkan-related work, Steam Deck improvements, bug fixes, performance changes, and personal quality-of-life fixes that I want to include.

> This is not an official Starbound project.
> This is not the upstream OpenStarbound project.
> This is a personal fork based on OpenStarbound.

---

## Important requirement

You **must own a legal copy of Starbound**.

OpenStarboundVulkan does **not** provide Starbound’s base game assets. You need to copy the required assets from your own Starbound installation before playing.

At minimum, you currently need the following file from your Starbound installation:

```text
packed.pak
```

This file is normally found in Starbound’s `assets` directory.

---

## What is this project?

OpenStarboundVulkan is intended to be a Vulkan-oriented fork of OpenStarbound.

The initial goal is simple:

* make Starbound/OpenStarbound run better on my own Steam Deck;
* experiment with Vulkan support and rendering improvements;
* fix bugs or annoyances I personally run into;
* keep the project available for others who may benefit from the same changes.

This is not meant to replace upstream OpenStarbound. Changes may be experimental, opinionated, or focused on my own use case first.

---

## Current status

This project is under active personal development.

Expect changes to happen when I have time, interest, or a specific issue I want to solve. Some changes may be incomplete, experimental, or subject to being rewritten later.

Recommended expectations:

* use stable releases when available;
* treat nightly or development builds as experimental;
* back up your saves before testing new builds;
* expect compatibility to improve over time rather than be perfect immediately.

---

## Relationship to OpenStarbound

OpenStarboundVulkan is based on OpenStarbound.

OpenStarbound already provides many improvements over vanilla Starbound, including bug fixes, performance improvements, new Lua functionality, asset loading improvements, quality-of-life additions, command changes, and partial compatibility with StarExtensions-style features.

This fork keeps the OpenStarbound foundation and builds on top of it with Vulkan-focused development and personal changes.

Credit for the OpenStarbound base belongs to the OpenStarbound project and its contributors.

Latest upstream sync:

* merged from `OpenStarbound/OpenStarbound` on **2026-05-03**;
* upstream commit: `f34e0026` (`Update README.md`).

---

## Main focus areas

Planned and ongoing focus areas may include:

* Vulkan renderer experiments;
* Steam Deck-focused improvements;
* performance improvements;
* bug fixes;
* quality-of-life changes;
* personal fixes for issues or annoyances in Starbound/OpenStarbound;
* compatibility improvements where practical.

Not every change will be useful to every player. This fork is intentionally personal first, public second.

---

## Installation

### 1. Download a build

Download the latest available release from this repository:

```text
https://github.com/Jintezaku/OpenStarboundVulkan/releases
```

When available, stable releases are recommended over development or nightly builds.

---

### 2. Install or extract the build

Depending on your platform and release type, either:

* run the installer, if one is provided; or
* extract the client/server archive manually.

OpenStarboundVulkan is a separate installation and executable from Starbound. It does not overwrite your normal Starbound installation unless you manually place files over it, which is not recommended.

---

### 3. Copy Starbound assets

Copy the following file from your normal Starbound installation:

```text
Starbound/assets/packed.pak
```

Place it into the OpenStarboundVulkan assets directory.

You may also optionally copy your `user` folder from the same Starbound directory if you want to carry over vanilla playable instrument songs.

---

### 4. Copy save data, if desired

OpenStarboundVulkan uses a separate installation directory.

To transfer your existing characters, worlds, and settings, copy your Starbound `storage` folder into the OpenStarboundVulkan directory.

Recommended:

```text
Back up your storage folder before copying or testing anything.
```

---

### 5. Mods

If you launch OpenStarboundVulkan while Steam is open, subscribed Steam Workshop mods should load as they do with OpenStarbound.

If you use locally installed non-Steam mods, copy your `mods` folder into the OpenStarboundVulkan directory as needed.

Mod compatibility should generally follow OpenStarbound compatibility, but this fork may introduce experimental changes over time.

---

## Steam Deck notes

This fork was originally created with the Steam Deck in mind.

The project may include Steam Deck-specific improvements or experiments over time, especially around rendering and Vulkan behavior.

General Steam Deck recommendations:

* back up your saves first;
* prefer stable builds when available;
* test without heavy mod lists first;
* add mods gradually if troubleshooting;
* expect Vulkan-related behavior to evolve during development.

---

## Changes inherited from OpenStarbound

Because this project is based on OpenStarbound, it inherits many of OpenStarbound’s improvements over vanilla Starbound.

The exact feature set may change depending on which OpenStarbound version this fork is currently based on, but inherited improvements may include the following areas.

---

### Lighting

OpenStarbound moved lightmap generation off the main thread and added support for a higher color range.

Notable lighting improvements include:

* additive point lights;
* better color mixing between different light sources;
* hybrid conversion for object spread lights.

---

### Assets

OpenStarbound expands what assets can do during loading.

Inherited asset-related features may include:

* Lua scripts that run when assets load;
* Lua scripts that run after all asset sources have loaded;
* scripts that can modify, read, patch, and create assets;
* `.patch.lua` support;
* JSON asset patching;
* image patching.

---

### Commands

OpenStarbound includes additional and changed commands.

Use this in-game to view available commands:

```text
/help
```

Example inherited command improvement:

```text
/settileprotection
```

OpenStarbound allows specifying multiple dungeon IDs or ranges, such as:

```text
/settileprotection 69 420 false
/settileprotection 0..65535 true
```

---

### Bug fixes

Inherited OpenStarbound bug fixes may include:

* invalid character inventories being updated when loading;
* better handling when swapping inventory mods on existing characters;
* fixes for vanilla world file size bloating;
* reduced unnecessary networking when modifying a single status property.

Some networking-related fixes may require both client and server to run a compatible OpenStarbound/OpenStarboundVulkan version.

---

### Player and character features

Inherited OpenStarbound features may include:

* player functions for saving and loading;
* humanoid identity modification;
* inventory manipulation functions;
* command-based character swapping;
* `/swap name` support with case-insensitive substring matching.

---

### Input and interface improvements

Inherited improvements may include:

* custom user input support;
* keybindings menu;
* UI polish;
* musical instrument volume slider;
* scriptPane override support for the Matter Manipulator or Collections sidebar button.

---

### Voice chat

OpenStarbound includes positional voice chat support that can work on vanilla servers.

This uses Opus for audio encoding and is available through the options menu in OpenStarbound-based builds.

---

### Fonts

Inherited font improvements may include:

* multiple font support;
* inline font switching with directives such as:

```text
^font=name;
```

* automatic detection for `.ttf` and `.woff2` assets.

`.woff2` fonts are usually much smaller than `.ttf` fonts.

---

### Directives and performance

OpenStarbound includes experimental changes to how directives are stored in memory.

This can reduce copying and improve frametimes in cases where extremely long directives are used, such as:

* generated clothing;
* custom items;
* custom objects;
* vanilla multiplayer-compatible creations that rely on long directive strings.

---

### Items and inventory behavior

Inherited improvements may include:

* Perfectly Generic Items retaining data about what item they originally were;
* automatic restoration attempts if the missing mod is reinstalled;
* item usage while lounging.

---

### Placement improvements

Inherited placement changes may include:

* client-side tile placement prediction;
* resizing tile placement areas on the fly;
* support for foreground tiles with custom collision types when supported by the server;
* object placement under non-solid foreground tiles;
* unlimited and unobstructed admin interaction/placement ranges.

---

### Skybox improvements

Inherited skybox improvements may include:

* the skybox sun matching the current system type;
* access to skybox sun scale;
* access to default ray colors.

Previously generated planets may still display the default sun. Modded system types may require additional patches to display their custom sun correctly.

---

## Building from source

These instructions are based on the OpenStarbound build process and may change as this fork develops.

### Template `sbinit.config` for `dist/`

After building, create or copy an `sbinit.config` file into the `dist/` directory.

```json
{
  "assetDirectories" : [
    "../assets/",
    "./mods/"
  ],

  "storageDirectory" : "./",
  "logDirectory" : "./logs/"
}
```

---

## Windows build instructions

### Requirements

Install the following:

* [vcpkg](https://github.com/microsoft/vcpkg)
* [Ninja](https://ninja-build.org/)
* CMake support through your IDE or standalone CMake
* Visual Studio build tools or a compatible C++ build environment

vcpkg is recommended to be installed globally in a short directory such as:

```text
C:\src\vcpkg
C:\dev\vcpkg
```

Set the `VCPKG_ROOT` environment variable to your vcpkg directory.

If you are using Visual Studio, run:

```text
vcpkg integrate install
```

---

### Build

Open the repository directory in your IDE. It should detect the CMake project.

Alternatively, build manually from the `source/` directory:

```text
cmake --preset=windows-release
cmake --build --preset=windows-release
```

Built binaries will be placed in:

```text
dist/
```

After building, copy the required DLLs from:

```text
lib/windows/
```

into:

```text
dist/
```

Also copy or create the `sbinit.config` file in `dist/`.

---

## Linux build instructions

### Ubuntu

Make sure you have CMake 3.23 or newer.

Install dependencies:

```bash
sudo apt-get install pkg-config libxmu-dev libxi-dev libgl-dev libglu1-mesa-dev libsdl2-dev python3-jinja2 ninja-build
```

Clone and bootstrap vcpkg outside the repository, then set `VCPKG_ROOT`:

```bash
export VCPKG_ROOT=/replace/with/full/path/to/your/vcpkg/directory/
```

Build from the `source/` directory:

```bash
cmake --preset=linux-release
cmake --build --preset=linux-release
```

Built binaries will be placed in:

```text
dist/
```

Copy the required `.so` libraries from:

```text
lib/linux/
```

into:

```text
dist/
```

Also copy or create the `sbinit.config` file in `dist/`.

From the root of the repository, you may also run the assembly script used by the GitHub Action:

```bash
scripts/ci/linux/assemble.sh
```

This assembles client and server distribution folders.

---

### Fedora

Install system dependencies:

```bash
sudo dnf upgrade --refresh
sudo dnf install cmake pkg-config libXmu-devel libXi-devel libGL-devel mesa-libGLU-devel SDL2-devel python3-jinja2 ninja-build
```

Install and initialize vcpkg. One possible method is:

```bash
. <(curl https://aka.ms/vcpkg-init.sh -L)
. ~/.vcpkg/vcpkg-init
```

Verify that `VCPKG_ROOT` is set:

```bash
printenv VCPKG_ROOT
```

Change to the `source/` directory and build:

```bash
cmake --preset=linux-release
cmake --build --preset=linux-release
```

If CMake fails with cache-related errors after changing vcpkg paths or build configuration, clear the CMake cache and regenerate the build files.

Built binaries will be placed in:

```text
dist/
```

After building, copy the required `.so` libraries and create or copy `sbinit.config` into `dist/`.

You may also use:

```bash
scripts/ci/linux/assemble.sh
```

from the repository root to assemble client and server distribution folders.

---

## macOS build instructions

Install Homebrew first if needed:

```text
https://brew.sh/
```

Install dependencies:

```bash
brew install cmake ninja pkg-config
```

Install vcpkg:

```bash
cd ~
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh
export VCPKG_ROOT=~/vcpkg
export PATH=$VCPKG_ROOT:$PATH
```

The `export` commands only apply to the current terminal session unless you add them to your shell profile.

Clone or download this repository, then enter the `source/` directory.

For Apple Silicon Macs:

```bash
cmake --preset macos-arm-release
cmake --build --preset macos-arm-release
```

For Intel Macs:

```bash
cmake --preset macos-release
cmake --build --preset macos-release
```

Built binaries will be placed in:

```text
dist/
```

After building:

1. Copy `libsteam_api.dylib` from `lib/osx/` into `dist/`.
2. Copy or rename the Discord SDK library for your architecture so the final file is named:

```text
discord_game_sdk.dylib
```

3. Copy your legally owned Starbound `packed.pak` into the correct assets directory.
4. Create or copy `sbinit.config` into `dist/`.

If macOS blocks the executable because it is from an unidentified developer, you may need to remove the quarantine flag from the binary:

```bash
xattr -d com.apple.quarantine starbound
```

---

## Save compatibility

OpenStarboundVulkan is based on Starbound/OpenStarbound save structures, but experimental forks can still introduce unexpected issues.

Before testing this fork, back up:

```text
storage/
```

This includes characters, ships, worlds, universe data, and settings.

---

## Mod compatibility

Mod compatibility should generally follow OpenStarbound, but this fork may diverge as Vulkan and personal changes are added.

Recommended testing approach:

1. Run once without mods.
2. Add essential mods only.
3. Add larger mod packs after confirming the base game launches.
4. Back up saves before testing heavy mod lists.

Some mods may depend on OpenStarbound or StarExtensions-specific behavior. Compatibility may improve over time, but is not guaranteed for every mod.

---

## Reporting issues

Issues are welcome, especially when they include useful details.

Helpful reports include:

* operating system;
* hardware/GPU;
* whether you are using Steam Deck;
* whether Vulkan-specific behavior is involved;
* build or release version;
* crash logs;
* mod list;
* steps to reproduce the issue.

For Steam Deck issues, also include whether you are running through Gaming Mode, Desktop Mode, Proton, or a native Linux build.

---

## Contributions

Contributions are welcome, but this is a personal fork and may not accept every change.

Good contributions are:

* focused;
* tested;
* easy to review;
* compatible with the project direction;
* respectful of upstream OpenStarbound where possible.

Large or invasive changes may be rejected or postponed if they conflict with the personal goals of this fork.

---

## Disclaimer

OpenStarboundVulkan is experimental software.

Use it at your own risk. Back up your saves before testing. Bugs, crashes, rendering issues, performance regressions, and mod compatibility problems may happen.

This project does not include Starbound assets and does not grant ownership of Starbound. You need to own the game separately.

---

## Credits

Based on [OpenStarbound](https://github.com/OpenStarbound/OpenStarbound).

OpenStarbound is based on Starbound 1.4.4 and includes extensive fixes, improvements, and new functionality from its own contributors.

Starbound belongs to its respective owners.

This fork is maintained separately as OpenStarboundVulkan.
