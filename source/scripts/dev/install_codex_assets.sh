#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCE_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
CODEX_HOME="${CODEX_HOME:-$HOME/.codex}"

install_skill() {
  local skill_name="$1"
  local source_skill_dir="$SOURCE_DIR/.codex/skills/$skill_name"
  local dest_skill_dir="$CODEX_HOME/skills/$skill_name"

  if [[ ! -d "$source_skill_dir" ]]; then
    echo "Skill source not found: $source_skill_dir"
    return
  fi

  mkdir -p "$dest_skill_dir"
  if command -v rsync >/dev/null 2>&1; then
    rsync -a --delete "$source_skill_dir/" "$dest_skill_dir/"
  else
    rm -rf "$dest_skill_dir"
    mkdir -p "$dest_skill_dir"
    cp -a "$source_skill_dir/." "$dest_skill_dir/"
  fi
  echo "Installed Codex skill: $skill_name -> $dest_skill_dir"
}

install_skill "starbound-vulkan-dev"

echo "Codex assets installed."
