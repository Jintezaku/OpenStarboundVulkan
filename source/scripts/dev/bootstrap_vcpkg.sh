#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCE_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
VCPKG_ROOT="${VCPKG_ROOT:-$HOME/vcpkg}"

if [[ ! -x "$VCPKG_ROOT/vcpkg" ]]; then
  if [[ ! -d "$VCPKG_ROOT" ]]; then
    git clone https://github.com/microsoft/vcpkg "$VCPKG_ROOT"
  fi

  "$VCPKG_ROOT/bootstrap-vcpkg.sh"
fi

"$VCPKG_ROOT/vcpkg" install \
  --x-manifest-root "$SOURCE_DIR" \
  --x-install-root "$SOURCE_DIR/../vcpkg_installed"

echo "vcpkg dependencies installed for manifest at: $SOURCE_DIR/vcpkg.json"
