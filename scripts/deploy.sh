#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
target="${1:-cklein@zero.local}"
binary="$project_dir/build/arm64-native/family-dashboard"

if [[ ! -f "$binary" ]]; then
  echo 'Build first: bash scripts/build.sh' >&2
  exit 1
fi

# SSH prompts for credentials normally; no password is stored in the project.
ssh "$target" '
  set -eu
  test "$(uname -m)" = aarch64 || { echo "Expected 64-bit Raspberry Pi OS." >&2; exit 1; }
  mkdir -p "$HOME/.local/bin"
  cat > "$HOME/.local/bin/family-dashboard.new"
  chmod 755 "$HOME/.local/bin/family-dashboard.new"
  mv "$HOME/.local/bin/family-dashboard.new" "$HOME/.local/bin/family-dashboard"
  exec "$HOME/.local/bin/family-dashboard"
' < "$binary"
