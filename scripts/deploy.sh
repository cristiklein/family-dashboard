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

stage_dir="$(mktemp -d)"
trap 'rm -rf -- "$stage_dir"' EXIT
cp -- "$config" "$stage_dir/dashboard.yaml"
cp -- "$binary" "$stage_dir/family-dashboard"

tar -cf - -C "$stage_dir" dashboard.yaml family-dashboard | ssh -T "$target" '
  set -eu
  test "$(uname -m)" = aarch64 || { echo "Expected 64-bit Raspberry Pi OS." >&2; exit 1; }
  config_dir="$HOME/.config/family-dashboard"
  bin_dir="$HOME/.local/bin"
  mkdir -p "$config_dir" "$bin_dir"

  (
    stage_dir="$(mktemp -d "$config_dir/.deploy.XXXXXXXX")"
    staged_binary=
    cleanup() {
      rm -rf -- "$stage_dir"
      rm -f -- "$staged_binary"
    }
    trap cleanup EXIT

    tar -xf - -C "$stage_dir" dashboard.yaml family-dashboard
    staged_binary="$(mktemp "$bin_dir/.family-dashboard.XXXXXXXX")"
    cat "$stage_dir/family-dashboard" > "$staged_binary"
    chmod 755 "$staged_binary"
    chmod 600 "$stage_dir/dashboard.yaml"

    flock -x 9
    mv -fT -- "$staged_binary" "$bin_dir/family-dashboard"
    mv -fT -- "$stage_dir/dashboard.yaml" "$config_dir/dashboard.yaml"
  ) 9< "$config_dir"

  exec "$bin_dir/family-dashboard"
'
