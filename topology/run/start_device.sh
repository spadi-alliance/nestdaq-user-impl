#!/bin/bash
# Launch one FairMQ binary from NESTDAQ_BIN with daq_service / metrics / parameter_config.
#
#   ./start_device.sh AmQStrTdcSampler
#   ./start_device.sh FileSink --service-name DecSink
#
# Extra arguments (after the binary name) are passed AFTER the plugins so
# daq_service sees --service-name. There is no DecSink binary; DecSink is
# FileSink with --service-name DecSink (same as run_daid-daq-01).
# Default location is topology/run/. Executables come from NESTDAQ_BIN:
# this repo's bin/ unless BIN_DIR is set in nestdaq.env.

set -u

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
cd "$SCRIPT_DIR"
NESTDAQ_ENV_SH=$SCRIPT_DIR/nestdaq_env.sh
# shellcheck disable=SC1090
. "$NESTDAQ_ENV_SH"

if [ $# -lt 1 ]; then
  echo "Usage: $0 BINARY [args...]" >&2
  echo "  binaries in $NESTDAQ_BIN" >&2
  ls -1 "$NESTDAQ_BIN" 2>/dev/null | sed 's/^/    /' >&2
  sleep 30
  exit 1
fi

BIN_NAME=$1
shift

redis_server=$(nestdaq_redis_host)

if [[ "$BIN_NAME" =~ fairmq- ]]; then
  BIN=$BIN_NAME
else
  BIN=$NESTDAQ_BIN/$BIN_NAME
fi

if [ ! -x "$BIN" ]; then
  echo "ERROR: binary not found or not executable: $BIN" >&2
  echo "  name used in the shell must match a file under $NESTDAQ_BIN" >&2
  echo "  set BIN_DIR in topology/run/nestdaq.env when that directory is elsewhere" >&2
  ls -1 "$NESTDAQ_BIN" 2>/dev/null | sed 's/^/    /' >&2
  sleep 60
  exit 1
fi

PLUGIN_LIBDIR=$NESTDAQ/lib64
if [ ! -d "$PLUGIN_LIBDIR" ] && [ -d "$NESTDAQ/lib" ]; then
  PLUGIN_LIBDIR=$NESTDAQ/lib
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
