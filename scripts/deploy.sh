#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
target="${1:-zero.local}"
binary="$project_dir/build/arm64-native/family-dashboard"

if (( $# > 1 )); then
  echo 'Usage: ./scripts/deploy.sh [ssh-target]' >&2
  exit 2
fi

if [[ ! -f "$binary" || ! -s "$binary" ]]; then
  echo 'Build first: ./scripts/build.sh' >&2
  exit 1
fi

expected_sha256="$(shasum -a 256 < "$binary" | cut -d ' ' -f 1)"
remote_script="$(cat <<'REMOTE'
  set -eu
  test "$(uname -m)" = aarch64 || { echo "Expected 64-bit Raspberry Pi OS." >&2; exit 1; }
  bin_dir="$HOME/.local/bin"
  mkdir -p "$bin_dir"
  staged_binary="$(mktemp "$bin_dir/.family-dashboard.XXXXXXXX")"
  trap 'rm -f -- "$staged_binary"' EXIT
  cat > "$staged_binary"
  actual_sha256="$(sha256sum < "$staged_binary")"
  actual_sha256="${actual_sha256%% *}"
  if [ "$actual_sha256" != "$expected_sha256" ]; then
    echo "Executable upload failed SHA-256 verification." >&2
    exit 1
  fi
  printf "Verified staged executable SHA-256: %s\n" "$actual_sha256"
  chmod 755 "$staged_binary"
  mv -fT -- "$staged_binary" "$bin_dir/family-dashboard"
  printf "Installed %s\n" "$bin_dir/family-dashboard"
REMOTE
)"

ssh -T -- "$target" "expected_sha256=$expected_sha256; $remote_script" < "$binary"
