#!/bin/sh
set -u

# Non-invasive target-machine diagnostics for fenrig-apparmor-profiles.
# This script never loads, unloads, or changes AppArmor policy and never
# launches a desktop application.

since=${FENRIG_AA_SINCE:-"15 minutes ago"}
failures=0
warnings=0

usage() {
  cat <<'EOF'
Usage: runtime-check.sh [--since JOURNAL_TIME]

Inspect the installed package, loaded profile labels, recent AppArmor
denials, and (when root/aa-exec are available) read-only negative probes.
Examples:
  runtime-check.sh
  runtime-check.sh --since '2026-11-23 17:39:00'
  sudo /usr/share/doc/fenrig-apparmor-profiles/tests/runtime-check.sh
EOF
}

pass() { printf 'PASS: %s\n' "$*"; }
warn() { warnings=$((warnings + 1)); printf 'REVIEW: %s\n' "$*"; }
fail() { failures=$((failures + 1)); printf 'FAIL: %s\n' "$*"; }
skip() { printf 'SKIP: %s\n' "$*"; }

while [ "$#" -gt 0 ]; do
  case $1 in
    --since)
      [ "$#" -ge 2 ] || { usage >&2; exit 2; }
      since=$2
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      usage >&2
      exit 2
      ;;
  esac
done

printf '%s\n' 'fenrig AppArmor runtime check'
printf 'Journal window: %s\n' "$since"

if command -v pacman >/dev/null 2>&1; then
  if pacman -Q fenrig-apparmor-profiles >/dev/null 2>&1; then
    pass 'fenrig-apparmor-profiles is installed'
  else
    fail 'fenrig-apparmor-profiles is not installed'
  fi
else
  skip 'pacman is unavailable'
fi

profile_dir=/etc/apparmor.d
profiles='fenrig-bwrap fenrig-firefox fenrig-thunderbird fenrig-libreoffice
fenrig-evince fenrig-gwenview fenrig-okular fenrig-ark fenrig-spectacle
fenrig-chromium fenrig-chrome fenrig-cromite fenrig-wowup-cf fenrig-ferdium fenrig-mumble
fenrig-transmission-qt fenrig-mpv fenrig-lutris fenrig-electron fenrig-discord
fenrig-steam fenrig-wine fenrig-clamav-native fenrig-clamav-quarantine-gui
fenrig-clamav-scan-gui fenrig-clamav-scan-quarantine-helper'
missing=0
for profile in $profiles; do
  if [ ! -r "$profile_dir/$profile" ]; then
    printf '  missing: %s\n' "$profile_dir/$profile"
    missing=1
  fi
done
if [ "$missing" -eq 0 ]; then
  pass 'all packaged profile files are present'
else
  fail 'one or more packaged profile files are missing'
fi

apparmor_active=0
loaded=''
if loaded=$(cat /sys/kernel/security/apparmor/profiles 2>/dev/null); then
  apparmor_active=1
  pass 'AppArmor profile filesystem is readable'
else
  skip 'AppArmor profile filesystem is unavailable or requires root'
fi

if [ "$apparmor_active" -eq 1 ]; then
  missing_loaded=0
  for profile in $profiles; do
    if ! printf '%s\n' "$loaded" | awk '{print $1}' | grep -Fxq "$profile"; then
      printf '  not loaded: %s\n' "$profile"
      missing_loaded=1
    fi
  done
  if [ "$missing_loaded" -eq 0 ]; then
    pass 'all packaged profiles are loaded'
  else
    fail 'one or more packaged profiles are not loaded'
  fi
fi

check_labels() {
  command_name=$1
  expected=$2
  found=0
  for pid in $(pgrep -x "$command_name" 2>/dev/null || true); do
    found=1
    label=$(cat "/proc/$pid/attr/current" 2>/dev/null || true)
    printf '  %s pid=%s label=%s\n' "$command_name" "$pid" "${label:-unreadable}"
    case "$label" in
      *"$expected"*) ;;
      *) warn "$command_name pid $pid does not show expected label $expected" ;;
    esac
  done
  [ "$found" -eq 1 ] || skip "$command_name is not running"
}

check_labels firefox firefox
check_labels thunderbird thunderbird
check_labels bwrap fenrig-bwrap
check_labels cromite cromite
check_labels chromium chromium
check_labels chrome chrome
check_labels lutris lutris
check_labels mpv mpv

if command -v journalctl >/dev/null 2>&1; then
  denials=$(journalctl -k --since "$since" --no-pager 2>/dev/null |
    grep 'apparmor="DENIED"' || true)
  if [ -n "$denials" ]; then
    warn 'AppArmor denials were recorded; review the grouped entries below'
    printf '%s\n' "$denials" |
      sed -n 's/.*profile="\([^"]*\)".*comm="\([^"]*\)".*operation="\([^"]*\)".*/  profile=\1 comm=\2 operation=\3/p' |
      sort | uniq -c | sort -nr | head -n 80
  else
    pass 'no AppArmor denials in the requested journal window'
  fi
else
  skip 'journalctl is unavailable'
fi

if [ "$(id -u)" -eq 0 ] && command -v aa-exec >/dev/null 2>&1 && command -v runuser >/dev/null 2>&1; then
  user_name=${SUDO_USER:-${USER:-}}
  target_home=''
  if [ -n "$user_name" ] && command -v getent >/dev/null 2>&1; then
    target_home=$(getent passwd "$user_name" | cut -d: -f6)
  fi
  if [ -z "$target_home" ]; then
    skip 'could not resolve the desktop user home for negative probes'
    user_name=''
  fi
  protected=''
  if [ -n "$target_home" ]; then
    for path in "$target_home/.ssh" "$target_home/.gnupg" "$target_home/.aws" "$target_home/.kube" \
      "$target_home/.password-store" "$target_home/.local/share/keyrings" "$target_home/.xonshrc" \
      "$target_home/.bashrc" "$target_home/.profile"; do
      [ -e "$path" ] && protected="$protected $path"
    done
  fi
  if [ -z "$protected" ]; then
    skip 'no protected test paths exist for the negative probe'
  elif [ -n "$user_name" ] && runuser -u "$user_name" -- aa-exec -p firefox -- sh -c '
      for path do
        if test -r "$path"; then exit 1; fi
      done
      exit 0
    ' sh $protected; then
    pass 'Firefox profile denied read probes for existing protected paths'
  else
    fail 'Firefox profile could read at least one protected path'
  fi
else
  skip 'negative probes require root and aa-exec; run this script with sudo'
fi

printf '\nSummary: failures=%s review_items=%s\n' "$failures" "$warnings"
if [ "$failures" -gt 0 ]; then
  exit 1
fi
exit 0
