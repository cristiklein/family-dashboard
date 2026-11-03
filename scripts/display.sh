#!/usr/bin/env bash
set -euo pipefail

target="linaro@192.168.1.183"
display_option=--display
target_set=false
for argument in "$@"; do
  case "$argument" in
    --morning) display_option=--display-morning ;;
    --afternoon) display_option=--display-afternoon ;;
    --test) display_option=--display ;;
    -*) echo 'Usage: bash scripts/display.sh [ssh-target] [--morning|--afternoon|--test]' >&2; exit 2 ;;
    *)
      if [[ "$target_set" == true ]]; then
        echo 'Only one SSH target may be supplied.' >&2
        exit 2
      fi
      target="$argument"
      target_set=true
      ;;
  esac
done

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
  exec sudo -n "$HOME/.local/bin/family-dashboard-hello" '"$display_option"'
'
