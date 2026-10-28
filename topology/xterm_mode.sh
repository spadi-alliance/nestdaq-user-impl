# Shared switch for run scripts.
# topology/xterm-mode is "xterm" (open an xterm per tmux session) or
# "viewer" (leave tmux detached; watch it in the topology page).
# A missing file means viewer: do not open xterms.
# NESTDAQ_XTERM=xterm|viewer overrides the file for that command.

want_xterm () {
  local mode="" f here
  case "${NESTDAQ_XTERM:-}" in
    0|off|no|viewer|topology) return 1 ;;
    1|on|yes|xterm) return 0 ;;
  esac
  here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
  for f in "$here/xterm-mode" "$here/../xterm-mode" "$here/../topology/xterm-mode"; do
    if [ -f "$f" ]; then
      mode=$(tr -d '[:space:]' < "$f")
      break
    fi
  done
  case "$mode" in
    xterm|1|on|yes) return 0 ;;
  esac
  return 1
}
