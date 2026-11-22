#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
target="${1:-cklein@zero.local}"
config="$project_dir/dashboard.yaml"

if (( $# > 1 )); then
  echo 'Usage: ./scripts/deploy-config.sh [ssh-target]' >&2
  exit 2
fi

if [[ ! -f "$config" || ! -s "$config" ]]; then
  echo 'Create a nonempty dashboard.yaml from dashboard.example.yaml and edit your plans first.' >&2
  exit 1
fi

expected_sha256="$(shasum -a 256 < "$config" | cut -d ' ' -f 1)"
remote_script="$(cat <<'REMOTE'
  set -eu
  umask 077
  config_dir="$HOME/.config/family-dashboard"
  mkdir -p "$config_dir"
  staged_config="$(mktemp "$config_dir/.dashboard.yaml.XXXXXXXX")"
  trap 'rm -f -- "$staged_config"' EXIT
  cat > "$staged_config"
  actual_sha256="$(sha256sum < "$staged_config")"
  actual_sha256="${actual_sha256%% *}"
  if [ "$actual_sha256" != "$expected_sha256" ]; then
    echo "Configuration upload failed SHA-256 verification." >&2
    exit 1
  fi
  printf "Verified staged configuration SHA-256: %s\n" "$actual_sha256"
  mv -fT -- "$staged_config" "$config_dir/dashboard.yaml"
  printf "Installed %s\n" "$config_dir/dashboard.yaml"
REMOTE
)"

ssh -T -- "$target" "expected_sha256=$expected_sha256; $remote_script" < "$config"
