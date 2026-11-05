#!/usr/bin/env bash
# Run on the Pi: bash install-morning-timer.sh [dashboard-user]
set -euo pipefail

if [[ "$EUID" != 0 ]]; then
  exec sudo bash "$0" "$@"
fi

dashboard_user="${1:-${SUDO_USER:-cklein}}"
dashboard_home="$(getent passwd "$dashboard_user" | cut -d: -f6)"
binary="$dashboard_home/.local/bin/family-dashboard"

[[ "$(uname -m)" == aarch64 ]] || { echo 'Run this installer on the ARM64 Pi.' >&2; exit 1; }
[[ -n "$dashboard_home" && -x "$binary" ]] || {
  echo "Deploy the current dashboard executable for $dashboard_user first." >&2
  exit 1
}

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
refresh_script="$dashboard_home/.local/bin/family-dashboard-refresh"
install -o "$dashboard_user" -m 755 "$script_dir/refresh-morning.sh" "$refresh_script"

stage_dir="$(mktemp -d)"
trap 'rm -rf "$stage_dir"' EXIT

cat > "$stage_dir/family-dashboard.service" <<EOF
[Unit]
Description=Refresh the family dashboard morning layout

[Service]
Type=oneshot
User=$dashboard_user
SupplementaryGroups=spi gpio
Environment=TZ=Europe/Stockholm
ExecStart="$refresh_script"
EOF

cat > "$stage_dir/family-dashboard.timer" <<'EOF'
[Unit]
Description=Refresh the family dashboard at the start of every minute

[Timer]
OnCalendar=*-*-* *:*:00
AccuracySec=1us
RandomizedDelaySec=0
Persistent=false

[Install]
WantedBy=timers.target
EOF

systemd-analyze verify "$stage_dir/family-dashboard.service" "$stage_dir/family-dashboard.timer"
install -m 644 "$stage_dir/family-dashboard.service" /etc/systemd/system/family-dashboard.service
install -m 644 "$stage_dir/family-dashboard.timer" /etc/systemd/system/family-dashboard.timer
systemctl daemon-reload
systemctl enable family-dashboard.timer
systemctl restart family-dashboard.timer
systemctl list-timers family-dashboard.timer --no-pager

printf '\nMorning layout scheduled at each minute boundary, including after reboot.\n'
printf 'A full refresh runs at the start of each hour; other minutes use partial refresh.\n'
printf 'Logs: sudo journalctl -u family-dashboard.service -f\n'
printf 'Stop: sudo systemctl disable --now family-dashboard.timer\n'
