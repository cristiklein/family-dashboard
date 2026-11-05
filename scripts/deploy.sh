#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
target="${1:-cklein@zero.local}"
binary="$project_dir/build/arm64-native/family-dashboard"
config="${2:-$project_dir/dashboard.yaml}"

if [[ ! -f "$binary" ]]; then
  echo 'Build first: bash scripts/build.sh' >&2
  exit 1
fi

if [[ ! -f "$config" ]]; then
  echo 'Copy dashboard.example.yaml to dashboard.yaml and edit your plans first (or supply the config path as the second argument).' >&2
  exit 1
fi

ssh "$target" '
  set -eu
  mkdir -p "$HOME/.config/family-dashboard"
  umask 077
  cat > "$HOME/.config/family-dashboard/dashboard.yaml.new"
  mv "$HOME/.config/family-dashboard/dashboard.yaml.new" "$HOME/.config/family-dashboard/dashboard.yaml"
' < "$config"

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
