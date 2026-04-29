#!/usr/bin/env bash
set -euo pipefail

if ! command -v code >/dev/null 2>&1; then
  echo "VS Code CLI not found ('code'). Install extensions manually from .vscode/extensions.json."
  exit 0
fi

extensions=(
  "ms-vscode.cpptools"
  "ms-vscode.cmake-tools"
  "llvm-vs-code-extensions.vscode-clangd"
  "xaver.clang-format"
  "slevesque.shader"
  "ms-vscode.hexeditor"
)

for extension in "${extensions[@]}"; do
  echo "Installing VS Code extension: $extension"
  code --install-extension "$extension" --force
done

echo "VS Code extensions installed."
