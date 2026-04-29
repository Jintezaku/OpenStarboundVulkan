#!/usr/bin/env bash
set -euo pipefail

if [[ "$(uname -s)" != "Linux" ]]; then
  echo "This helper currently targets Linux package managers."
  exit 0
fi

as_root() {
  if [[ "$(id -u)" -eq 0 ]]; then
    "$@"
  elif command -v sudo >/dev/null 2>&1; then
    sudo "$@"
  else
    echo "Root privileges are required to install packages, but sudo is unavailable."
    exit 1
  fi
}

if command -v apt-get >/dev/null 2>&1; then
  as_root apt-get update
  as_root apt-get install -y \
    build-essential \
    pkg-config \
    cmake \
    ninja-build \
    libvulkan-dev \
    libx11-dev \
    libxft-dev \
    libxext-dev \
    libwayland-dev \
    libxkbcommon-dev \
    libegl1-mesa-dev \
    libibus-1.0-dev \
    libasound2-dev \
    vulkan-tools \
    vulkan-validationlayers \
    glslang-tools \
    spirv-tools
  exit 0
fi

if command -v dnf >/dev/null 2>&1; then
  as_root dnf install -y \
    gcc-c++ \
    pkgconf-pkg-config \
    cmake \
    ninja-build \
    vulkan-loader-devel \
    libX11-devel \
    libXft-devel \
    libXext-devel \
    wayland-devel \
    libxkbcommon-devel \
    mesa-libEGL-devel \
    ibus-devel \
    alsa-lib-devel \
    vulkan-tools \
    vulkan-validation-layers \
    glslang \
    spirv-tools
  exit 0
fi

if command -v pacman >/dev/null 2>&1; then
  as_root pacman -S --needed \
    base-devel \
    cmake \
    ninja \
    vulkan-headers \
    libx11 \
    libxft \
    libxext \
    wayland \
    libxkbcommon \
    mesa \
    ibus \
    alsa-lib \
    vulkan-tools \
    vulkan-validation-layers \
    glslang \
    spirv-tools
  exit 0
fi

echo "Unsupported package manager. Install Vulkan loader, validation layers, glslang, and spirv-tools manually."
