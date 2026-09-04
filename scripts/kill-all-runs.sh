#!/usr/bin/env bash
# scripts/kill-all-runs.sh
# Cross-machine hard gate: kill every carrom_arena / xvfb-run / stale Xvfb
# on every known test host before allowing a new run (or a git push).
#
# Exit code:
#   0 - every host either has no runs, or all runs were killed cleanly
#   1 - at least one host reachable but killing failed
#   2 - at least one host UNREACHABLE (soft-warn unless --strict)
#
# Env:
#   KILL_TIMEOUT_S    per-host SSH timeout, default 10
#   SKIP_HOSTS        space-separated substrings to exclude (e.g. "10.0" or ".77")
#
# Flags:
#   --strict          exit 2 if any host is unreachable
#   --dry-run         print what would be killed, kill nothing
#   --verbose         extra per-host output

set -u

STRICT=0
DRY=0
VERBOSE=0
for a in "$@"; do
  case "$a" in
    --strict)  STRICT=1 ;;
    --dry-run) DRY=1 ;;
    --verbose) VERBOSE=1 ;;
    --help|-h)
      grep '^#' "$0" | sed 's/^# \?//'
      exit 0
      ;;
  esac
done

: "${KILL_TIMEOUT_S:=10}"
: "${SKIP_HOSTS:=}"

# Hosts as: kind|user@ip|label
HOSTS=(
  "linux|sanyalnet@192.168.4.76|Linux (this box)"
  "win|sanyalnet@192.168.4.75|Windows 10"
  "win|vagab@192.168.4.103|Windows 11"
  "mac|rumtuk@192.168.4.77|macOS Intel"
)

log() { [ "$VERBOSE" = "1" ] && echo "  $*" >&2; }

# Prefer key auth (already installed on all four). Fall back to sshpass with harryseldon.
ssh_run() {
  local target="$1" cmd="$2"
  local peer_ip="${target##*@}"
  local my_ips
  my_ips=$(hostname -I 2>/dev/null; echo 127.0.0.1 localhost)
  for ip in $my_ips; do
    if [ "$peer_ip" = "$ip" ]; then
      bash -c "$cmd" 2>&1
      return $?
    fi
  done
  local base_opts=(-o BatchMode=yes -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o ConnectTimeout="$KILL_TIMEOUT_S")
  if [ -f "$HOME/.ssh/id_ed25519" ]; then
    ssh "${base_opts[@]}" -i "$HOME/.ssh/id_ed25519" "$target" "$cmd" 2>&1
    return $?
  fi
  if command -v sshpass >/dev/null 2>&1; then
    sshpass -p harryseldon ssh "${base_opts[@]/-o BatchMode=yes/}" "$target" "$cmd" 2>&1
    return $?
  fi
  echo "no ssh auth available (need ~/.ssh/id_ed25519 or sshpass)" >&2
  return 255
}

kill_linux() {
  local target="$1"
  local cmd='ps -o pid,cmd | grep -E "[c]arrom_arena|[x]vfb-run|[X]vfb " | awk "{print \$1}"'
  local pids
  pids=$(ssh_run "$target" "$cmd") || return 2
  pids=$(echo "$pids" | grep -E '^[0-9]+$' | tr '\n' ' ')
  [ -z "${pids// }" ] && { log "  no runs"; return 0; }
  log "  killing: $pids"
  [ "$DRY" = "1" ] && return 0
  ssh_run "$target" "kill -TERM $pids 2>/dev/null; sleep 2; kill -KILL $pids 2>/dev/null; true" >/dev/null
  return 0
}

kill_win() {
  local target="$1"
  [ "$DRY" = "1" ] && { log "  would run: taskkill /F /IM carrom_arena.exe /T"; return 0; }
  ssh_run "$target" 'taskkill /F /IM carrom_arena.exe /T 2>&1 & taskkill /F /IM xvfb-run 2>&1' >/dev/null
  return 0
}

kill_mac() {
  local target="$1"
  local cmd='ps -o pid,command | grep -E "[c]arrom_arena|[X]vfb" | awk "{print \$1}"'
  local pids
  pids=$(ssh_run "$target" "$cmd") || return 2
  pids=$(echo "$pids" | grep -E '^[0-9]+$' | tr '\n' ' ')
  [ -z "${pids// }" ] && { log "  no runs"; return 0; }
  log "  killing: $pids"
  [ "$DRY" = "1" ] && return 0
  ssh_run "$target" "kill -TERM $pids 2>/dev/null; sleep 2; kill -KILL $pids 2>/dev/null; true" >/dev/null
  return 0
}

rc_overall=0
for entry in "${HOSTS[@]}"; do
  IFS='|' read -r kind target label <<< "$entry"
  # SKIP_HOSTS substring match
  if [ -n "$SKIP_HOSTS" ]; then
    for s in $SKIP_HOSTS; do
      [ "${target#*$s}" != "$target" ] && { echo "SKIP $label ($target)"; continue 2; }
    done
  fi
  # reachability probe
  if ! ssh_run "$target" 'echo up' | grep -q '^up'; then
    echo "UNREACHABLE $label ($target)"
    [ "$STRICT" = "1" ] && rc_overall=2
    continue
  fi
  echo -n "KILL     $label ($target) ... "
  case "$kind" in
    linux) kill_linux "$target" && echo ok || { echo FAIL; rc_overall=1; } ;;
    win)   kill_win   "$target" && echo ok || { echo FAIL; rc_overall=1; } ;;
    mac)   kill_mac   "$target" && echo ok || { echo FAIL; rc_overall=1; } ;;
  esac
done
[ "$DRY" = "1" ] && echo "(dry-run: no processes were actually killed)"
exit $rc_overall
