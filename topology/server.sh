#!/bin/bash
# Start or stop the topology viewer.
#
#   ./server.sh start
#   ./server.sh stop
#
# start listens on 0.0.0.0:8765 and prints the addresses to open.
# Extra arguments are passed to serve.py:
#   ./server.sh start --port 8765 --redis redis://127.0.0.1:6379
#   ./server.sh start --nolog
#   ./server.sh start --debug

set -euo pipefail

HERE=$(cd "$(dirname "$0")" && pwd)
cd "$HERE"

HOST="${HOST:-0.0.0.0}"
PORT="${PORT:-8765}"
PIDFILE="${PIDFILE:-$HERE/serve.pid}"
LOGFILE="${LOGFILE:-$HERE/serve.log}"

NOLOG=0
DEBUG=0
PASS=()

usage () {
  cat <<EOF
Usage:
  $0 start [options]
  $0 stop

start runs the viewer in the background and prints where to connect.
stop ends the process recorded in serve.pid.

Options for start:
  --host HOST     bind address (default 0.0.0.0)
  --port PORT     listen port (default 8765)
  --nolog         do not write serve.log
  --debug         foreground; Ctrl-C stops it
  --              remaining arguments go to serve.py
EOF
}

system_ips () {
  local raw="" ip
  if command -v hostname >/dev/null 2>&1; then
    raw=$(hostname -I 2>/dev/null || true)
  fi
  if [ -z "$raw" ] && command -v ip >/dev/null 2>&1; then
    raw=$(ip -o addr show scope global 2>/dev/null | awk '{print $4}' | cut -d/ -f1 | tr '\n' ' ')
  fi
  for ip in $raw; do
    case "$ip" in
      127.*|::1|fe80:*|fe80::*) continue ;;
    esac
    printf '%s\n' "$ip"
  done
}

print_url () {
  local ip="$1" port="$2"
  case "$ip" in
    *:*)
      printf '  http://[%s]:%s/\n' "$ip" "$port"
      printf '  http://[%s]:%s/guide\n' "$ip" "$port"
      ;;
    *)
      printf '  http://%s:%s/\n' "$ip" "$port"
      printf '  http://%s:%s/guide\n' "$ip" "$port"
      ;;
  esac
}

print_connect () {
  local host="$1" port="$2" ip name local_shown=0
  echo "listen  ${host}:${port}"
  echo "connect"
  case "$host" in
    0.0.0.0|::|"")
      while IFS= read -r ip; do
        [ -n "$ip" ] || continue
        print_url "$ip" "$port"
        if [ "$ip" = "127.0.0.1" ]; then
          local_shown=1
        fi
      done < <(system_ips)
      ;;
    *)
      print_url "$host" "$port"
      if [ "$host" = "127.0.0.1" ]; then
        local_shown=1
      fi
      ;;
  esac
  if [ "$local_shown" -eq 0 ]; then
    printf '  http://127.0.0.1:%s/  (this machine)\n' "$port"
    printf '  http://127.0.0.1:%s/guide\n' "$port"
  fi
  name=$(hostname -s 2>/dev/null || hostname 2>/dev/null || true)
  if [ -n "$name" ]; then
    echo "host    ${name}"
  fi
}

running_pid () {
  local old=""
  if [ -f "$PIDFILE" ]; then
    old=$(cat "$PIDFILE" 2>/dev/null || true)
  fi
  if [ -n "${old:-}" ] && kill -0 "$old" 2>/dev/null; then
    printf '%s\n' "$old"
    return 0
  fi
  return 1
}

do_stop () {
  local old=""
  if ! old=$(running_pid); then
    rm -f "$PIDFILE"
    echo "topology viewer is not running"
    return 0
  fi
  kill "$old" 2>/dev/null || true
  local _
  for _ in 1 2 3 4 5 6 7 8 9 10; do
    kill -0 "$old" 2>/dev/null || break
    sleep 0.2
  done
  if kill -0 "$old" 2>/dev/null; then
    kill -9 "$old" 2>/dev/null || true
  fi
  rm -f "$PIDFILE"
  echo "stopped  pid=${old}  port=${PORT}"
}

do_start () {
  if ! command -v python3 >/dev/null 2>&1; then
    echo "ERROR: python3 not found" >&2
    exit 1
  fi
  if ! python3 -c "import redis" >/dev/null 2>&1; then
    echo "ERROR: Python package 'redis' is missing." >&2
    echo "  python3 -m pip install --user redis" >&2
    exit 1
  fi

  local old=""
  if old=$(running_pid); then
    echo "already running  pid=${old}"
    print_connect "$HOST" "$PORT"
    return 0
  fi
  rm -f "$PIDFILE"

  if command -v ss >/dev/null 2>&1 && ss -ltn | awk '{print $4}' | grep -Eq "(^|:)${PORT}$"; then
    echo "ERROR: port ${PORT} is already in use." >&2
    exit 1
  fi

  local cmd=(python3 "$HERE/serve.py" --host "$HOST" --port "$PORT")
  if [ ${#PASS[@]} -gt 0 ]; then
    cmd+=("${PASS[@]}")
  fi

  if [ "$DEBUG" -eq 1 ]; then
    echo "topology viewer  (debug, foreground)"
    print_connect "$HOST" "$PORT"
    echo "Ctrl-C to stop"
    echo
    exec "${cmd[@]}"
  fi

  if [ "$NOLOG" -eq 1 ]; then
    nohup "${cmd[@]}" >/dev/null 2>&1 &
  else
    nohup "${cmd[@]}" >>"$LOGFILE" 2>&1 &
  fi
  local pid=$!
  echo "$pid" > "$PIDFILE"
  disown "$pid" 2>/dev/null || true

  sleep 0.3
  if ! kill -0 "$pid" 2>/dev/null; then
    echo "ERROR: serve.py exited immediately." >&2
    if [ "$NOLOG" -eq 0 ]; then
      echo "  See $LOGFILE" >&2
    fi
    rm -f "$PIDFILE"
    exit 1
  fi

  echo "topology viewer started"
  echo "pid     ${pid}  (${PIDFILE})"
  if [ "$NOLOG" -eq 1 ]; then
    echo "log     (none)"
  else
    echo "log     ${LOGFILE}"
  fi
  print_connect "$HOST" "$PORT"
}

ACTION="${1:-}"
if [ $# -gt 0 ]; then
  shift
fi

case "$ACTION" in
  -h|--help|"")
    usage
    if [ -z "$ACTION" ]; then
      exit 1
    fi
    exit 0
    ;;
  start|stop) ;;
  *)
    echo "ERROR: unknown command: ${ACTION}" >&2
    usage >&2
    exit 1
    ;;
esac

if [ "$ACTION" = "stop" ]; then
  if [ $# -gt 0 ]; then
    echo "ERROR: stop takes no options" >&2
    exit 1
  fi
  do_stop
  exit 0
fi

while [ $# -gt 0 ]; do
  case "$1" in
    -h|--help)
      usage
      exit 0
      ;;
    --nolog|--no-log|-q)
      NOLOG=1
      shift
      ;;
    --debug|-d)
      DEBUG=1
      shift
      ;;
    --host)
      HOST="$2"
      shift 2
      ;;
    --port)
      PORT="$2"
      shift 2
      ;;
    --)
      shift
      PASS+=("$@")
      break
      ;;
    *)
      PASS+=("$1")
      shift
      ;;
  esac
done

do_start
