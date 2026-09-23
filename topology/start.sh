#!/bin/bash
# Start the topology viewer (http://127.0.0.1:8765/).
#
#   ./start.sh              background, append serve.log
#   ./start.sh --nolog      background, no log file
#   ./start.sh --debug      foreground, messages on this terminal (Ctrl-C)
#
# Extra arguments go to serve.py:
#   ./start.sh --redis redis://127.0.0.1:6379 --port 8765
#
# Stop a background instance:
#   kill "$(cat serve.pid)"
#   # or: fuser -k 8765/tcp

set -euo pipefail

HERE=$(cd "$(dirname "$0")" && pwd)
cd "$HERE"

HOST="${HOST:-127.0.0.1}"
PORT="${PORT:-8765}"
PIDFILE="${PIDFILE:-$HERE/serve.pid}"
LOGFILE="${LOGFILE:-$HERE/serve.log}"

NOLOG=0
DEBUG=0
PASS=()

usage () {
  sed -n '2,14p' "$0" | sed 's/^# \?//'
}

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

if ! command -v python3 >/dev/null 2>&1; then
  echo "ERROR: python3 not found" >&2
  exit 1
fi

if ! python3 -c "import redis" >/dev/null 2>&1; then
  echo "ERROR: Python package 'redis' is missing." >&2
  echo "  python3 -m pip install --user redis" >&2
  exit 1
fi

if [ -f "$PIDFILE" ]; then
  old=$(cat "$PIDFILE" 2>/dev/null || true)
  if [ -n "${old:-}" ] && kill -0 "$old" 2>/dev/null; then
    echo "already running  pid=$old  http://${HOST}:${PORT}/"
    if [ -f "$LOGFILE" ]; then
      echo "log  $LOGFILE"
    fi
    exit 0
  fi
  rm -f "$PIDFILE"
fi

if command -v ss >/dev/null 2>&1 && ss -ltn | awk '{print $4}' | grep -q ":${PORT}$"; then
  echo "ERROR: ${HOST}:${PORT} is already in use (pid file stale or another process)." >&2
  echo "  fuser -k ${PORT}/tcp" >&2
  echo "  $0 $*" >&2
  exit 1
fi

cmd=(python3 "$HERE/serve.py" --host "$HOST" --port "$PORT")
if [ ${#PASS[@]} -gt 0 ]; then
  cmd+=("${PASS[@]}")
fi

if [ "$DEBUG" -eq 1 ]; then
  echo "topology viewer  http://${HOST}:${PORT}/  (debug, foreground)"
  echo "Ctrl-C to stop"
  echo
  exec "${cmd[@]}"
fi

if [ "$NOLOG" -eq 1 ]; then
  nohup "${cmd[@]}" >/dev/null 2>&1 &
else
  nohup "${cmd[@]}" >>"$LOGFILE" 2>&1 &
fi
pid=$!
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

echo "topology viewer  http://${HOST}:${PORT}/"
echo "pid   $pid  ($PIDFILE)"
if [ "$NOLOG" -eq 1 ]; then
  echo "log   (none)"
else
  echo "log   $LOGFILE"
fi
echo "stop  kill $pid"
