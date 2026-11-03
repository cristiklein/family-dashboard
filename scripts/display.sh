#!/usr/bin/env bash
set -euo pipefail

target="cklein@zero.local"
display_option=--display
refresh_option=--partial
target_set=false
partial_demo=false
display_options_set=false
for argument in "$@"; do
  case "$argument" in
    --morning) display_option=--display-morning; display_options_set=true ;;
    --afternoon) display_option=--display-afternoon; display_options_set=true ;;
    --test) display_option=--display; display_options_set=true ;;
    --fast) refresh_option=--fast; display_options_set=true ;;
    --full) refresh_option=--full; display_options_set=true ;;
    --partial) refresh_option=--partial; display_options_set=true ;;
    --demo-partial) partial_demo=true ;;
    -*) echo 'Usage: bash scripts/display.sh [ssh-target] [--morning|--afternoon|--test] [--partial|--fast|--full] OR [ssh-target] --demo-partial' >&2; exit 2 ;;
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

# Only fixed, allowlisted arguments are inserted into the remote command.
display_arguments="$display_option $refresh_option"
if [[ "$partial_demo" == true ]]; then
  if [[ "$display_options_set" == true ]]; then
    echo '--demo-partial cannot be combined with layout or refresh options.' >&2
    exit 2
  fi
  display_arguments=--demo-partial
fi

# The executable requests the HAT GPIO lines through the kernel character API.
# Raspberry Pi OS grants access through the spi and gpio groups.
ssh "$target" '
  set -eu
  test -x "$HOME/.local/bin/family-dashboard"
  exec "$HOME/.local/bin/family-dashboard" '"$display_arguments"'
'
