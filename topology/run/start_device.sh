#!/bin/bash
# Launch one FairMQ binary from the repo bin/ with daq_service / metrics / parameter_config.
#
#   ./start_device.sh AmQStrTdcSampler
#   ./start_device.sh FileSink --service-name DecSink
#
# Extra arguments (after the binary name) are passed AFTER the plugins so
# daq_service sees --service-name. There is no DecSink binary; DecSink is
# FileSink with --service-name DecSink (same as run_daid-daq-01).
# Default location is topology/run/; bin/ is at the repo root.

set -u

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
if [ -d "$SCRIPT_DIR/../../bin" ]; then
  ROOT_DIR=$(cd "$SCRIPT_DIR/../.." && pwd)
elif [ -d "$SCRIPT_DIR/../bin" ]; then
  ROOT_DIR=$(cd "$SCRIPT_DIR/.." && pwd)
else
  ROOT_DIR=$(cd "$SCRIPT_DIR/.." && pwd)
fi
cd "$SCRIPT_DIR"

if [ $# -lt 1 ]; then
  echo "Usage: $0 BINARY [args...]" >&2
  echo "  binaries in $ROOT_DIR/bin" >&2
  ls -1 "$ROOT_DIR/bin" 2>/dev/null | sed 's/^/    /' >&2
  sleep 30
  exit 1
fi

BIN_NAME=$1
shift

redis_server=127.0.0.1:6379
if [ -f "$SCRIPT_DIR/redis-server.txt" ]; then
  redis_server=$(cat "$SCRIPT_DIR/redis-server.txt" | sed 's/[[:blank:]]//g')
fi

if [[ "$BIN_NAME" =~ fairmq- ]]; then
  BIN=$BIN_NAME
else
  BIN=$ROOT_DIR/bin/$BIN_NAME
fi

if [ ! -x "$BIN" ]; then
  echo "ERROR: binary not found or not executable: $BIN" >&2
  echo "  name used in the shell must match a file under $ROOT_DIR/bin" >&2
  ls -1 "$ROOT_DIR/bin" 2>/dev/null | sed 's/^/    /' >&2
  sleep 60
  exit 1
fi

NESTDAQ_ROOT=${NESTDAQ:-$HOME/nestdaq}
PLUGIN_LIBDIR=$NESTDAQ_ROOT/lib64
if [ ! -d "$PLUGIN_LIBDIR" ] && [ -d "$NESTDAQ_ROOT/lib" ]; then
  PLUGIN_LIBDIR=$NESTDAQ_ROOT/lib
fi

echo "binary            = $BIN"
echo "extra args        = $*"
echo "PLUGIN_LIBDIR     = $PLUGIN_LIBDIR"
echo "registry          = tcp://$redis_server/0"

# Plugins first, then device-specific flags (--service-name DecSink, ...).
"$BIN" \
  -S "<${PLUGIN_LIBDIR}>" \
  -P daq_service \
  -P metrics \
  -P parameter_config \
  --registry-uri "tcp://${redis_server}/0" \
  --metrics-uri "tcp://${redis_server}/1" \
  --parameter-config-uri "tcp://${redis_server}/2" \
  --scaler-uri "tcp://${redis_server}/3" \
  --severity info \
  "$@"
status=$?
if [ "$status" -ne 0 ]; then
  echo
  echo "ERROR: $BIN_NAME exited status=$status"
  echo "window stays 60s so xterm is not blank (tmux remain-on-exit also keeps it)"
  sleep 60
fi
exit $status
