#!/usr/bin/env bash
set -euo pipefail

target="${1:-linaro@192.168.1.183}"

# WiringPi/TinkerOS pin numbers here are different from Linux GPIO numbers.
# This temporarily switches UART4's header pins and the HAT power pin to GPIO;
# it does not edit boot configuration. Use only with the directly attached HAT.
ssh "$target" '
  set -eu
  test -x "$HOME/.local/bin/family-dashboard-hello"
  command -v gpio >/dev/null
  sudo -n gpio mode 1 out
  sudo -n gpio mode 0 out
  sudo -n gpio mode 6 out
  sudo -n gpio mode 5 in
  exec sudo -n "$HOME/.local/bin/family-dashboard-hello" --display
'
