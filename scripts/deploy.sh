#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
target="${1:-linaro@192.168.1.183}"
binary="$project_dir/build/armv7-native/family-dashboard"

if [[ ! -f "$binary" ]]; then
  echo 'Build first: bash scripts/build.sh' >&2
  exit 1
fi

# SSH prompts for credentials normally; no password is stored in the project.
ssh "$target" '
  set -eu
  mkdir -p "$HOME/.local/bin"
  cat > "$HOME/.local/bin/family-dashboard-hello.new"
  chmod 755 "$HOME/.local/bin/family-dashboard-hello.new"
  mv "$HOME/.local/bin/family-dashboard-hello.new" "$HOME/.local/bin/family-dashboard-hello"
  exec "$HOME/.local/bin/family-dashboard-hello"
' < "$binary"
