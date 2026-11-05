#!/usr/bin/env bash
# Installed beside family-dashboard and invoked by the minute timer.
set -euo pipefail

binary="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)/family-dashboard"
refresh_option=--partial
if [[ "$(date +%M)" == 00 ]]; then
  refresh_option=--full
fi

exec "$binary" --display-morning "$refresh_option"
