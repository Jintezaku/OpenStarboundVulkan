#!/usr/bin/env bash
set -euo pipefail

ensure_starbound_tmpdir() {
  local script_dir source_dir default_tmpdir target_tmpdir
  script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
  source_dir="$(cd "$script_dir/../.." && pwd)"
  default_tmpdir="$source_dir/../tmp/openstarbound-build"
  target_tmpdir="${STAR_TMPDIR:-$default_tmpdir}"

  mkdir -p "$target_tmpdir"

  export TMPDIR="$target_tmpdir"
  export TMP="$target_tmpdir"
  export TEMP="$target_tmpdir"

  echo "Using build temp dir: $target_tmpdir"
}
