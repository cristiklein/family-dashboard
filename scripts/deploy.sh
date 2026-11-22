#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
target="${1:-cklein@zero.local}"
binary="$project_dir/build/arm64-native/family-dashboard"

if (( $# > 1 )); then
  echo 'Usage: ./scripts/deploy.sh [ssh-target]' >&2
  exit 2
fi

if [[ ! -f "$binary" || ! -s "$binary" ]]; then
  echo 'Build first: ./scripts/build.sh' >&2
  exit 1
fi

expected_size="$(wc -c < "$binary" | tr -d '[:space:]')"
ssh -T -- "$target" 'expected_size='"$expected_size"'
  set -eu
  test "$(uname -m)" = aarch64 || { echo "Expected 64-bit Raspberry Pi OS." >&2; exit 1; }
  bin_dir="$HOME/.local/bin"
  mkdir -p "$bin_dir"
  staged_binary="$(mktemp "$bin_dir/.family-dashboard.XXXXXXXX")"
  trap '\''rm -f -- "$staged_binary"'\'' EXIT
  cat > "$staged_binary"
  if [ "$(wc -c < "$staged_binary")" -ne "$expected_size" ]; then
    echo "Incomplete executable upload." >&2
    exit 1
  fi
  chmod 755 "$staged_binary"
  mv -fT -- "$staged_binary" "$bin_dir/family-dashboard"
  printf "Installed %s\n" "$bin_dir/family-dashboard"
' < "$binary"
