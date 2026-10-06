#!/bin/bash
# Start TFBFilePlayer x1 -> LogicFilterGpu|LogicFilter x1 -> FileSink x1.
#
# FILTER_DEVICE selects the filter binary:
#   LogicFilter        CPU filter (default)
#   LogicFilterGpu     GPU filter
#   LogicFilterSlice   bit-sliced filter, trigger-backend cpu|gpu
#
# Usage:
#   ./run-tfbplayer-flt-sink.sh
#   FILTER_DEVICE=LogicFilter ./run-tfbplayer-flt-sink.sh
#   FILTER_DEVICE=LogicFilterSlice ./run-tfbplayer-flt-sink.sh
#   TFB_INFILE=/path/to/tf.dat ./run-tfbplayer-flt-sink.sh
#
# WITH_TRIGGER_VIEW=1 also registers the filter's dqm channel, points
# TriggerView at this filter's parameter hash, and starts it:
#   WITH_TRIGGER_VIEW=1 FILTER_DEVICE=LogicFilterSlice ./run-tfbplayer-flt-sink.sh

set -e

NESTDAQ=${NESTDAQ:-$HOME/nestdaq}
export PATH="$NESTDAQ/bin:$PATH"
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$NESTDAQ/lib:$NESTDAQ/lib64:$HOME/nestdaq/lib:$HOME/nestdaq/lib64

cd "$(dirname "$0")"
. ./device_launch.sh

#FILTER_DEVICE=${FILTER_DEVICE:-LogicFilterGpu}
FILTER_DEVICE=${FILTER_DEVICE:-LogicFilter}
case "$FILTER_DEVICE" in
  LogicFilter|LogicFilterGpu|LogicFilterSlice) ;;
  *)
    echo "ERROR: FILTER_DEVICE must be LogicFilter (CPU), LogicFilterGpu (GPU)" >&2
    echo "       or LogicFilterSlice (bit-sliced)" >&2
    exit 1
    ;;
esac
export FILTER_DEVICE

WITH_TRIGGER_VIEW=${WITH_TRIGGER_VIEW:-0}
export WITH_TRIGGER_VIEW
if [ "$WITH_TRIGGER_VIEW" = "1" ] && [ -z "${DISPLAY:-}" ]; then
  echo "ERROR: WITH_TRIGGER_VIEW=1 needs DISPLAY; TriggerView draws on a canvas" >&2
  exit 1
fi

redis_server=`cat redis-server.txt | sed 's/[[:blank:]]//g'`
PARAM_URI=redis://$redis_server/2
TOPO_URI=redis://$redis_server/0

fail=0

ensure_redis () {
  if redis-cli -u redis://$redis_server ping >/dev/null 2>&1; then
    echo "[1/6] Redis already running at $redis_server"
    return 0
  fi
  echo "[1/6] Redis is down. Starting ./init.sh ..."
  ./init.sh
  local i=0
  while [ $i -lt 30 ]; do
    if redis-cli -u redis://$redis_server ping >/dev/null 2>&1; then
      echo "[1/6] Redis is ready"
      return 0
    fi
    i=$((i + 1))
    sleep 0.2
  done
  echo "ERROR: Redis did not become ready after ./init.sh ($redis_server)" >&2
  exit 1
}

require_hfield () {
  local uri=$1 key=$2 field=$3
  local val
  val=`redis-cli -u "$uri" hget "$key" "$field"`
  if [ -z "$val" ]; then
    echo "  MISSING  $key.$field" >&2
    fail=1
  else
    echo "  OK       $key.$field = $val"
  fi
}

require_key () {
  local uri=$1 key=$2
  if [ "`redis-cli -u "$uri" exists "$key"`" != "1" ]; then
    echo "  MISSING  $key" >&2
    fail=1
  else
    echo "  OK       $key"
  fi
}

verify_parameters () {
  echo "[3/6] Verify parameters (Redis DB 2)"
  fail=0
  require_hfield "$PARAM_URI" parameters:TFBFilePlayer-0 in-file
  require_hfield "$PARAM_URI" parameters:TFBFilePlayer-0 split
  require_hfield "$PARAM_URI" parameters:${FILTER_DEVICE}-0 trigger-expression
  require_hfield "$PARAM_URI" parameters:${FILTER_DEVICE}-0 trigger-signals
  require_hfield "$PARAM_URI" parameters:${FILTER_DEVICE}-0 trigger-width
  require_hfield "$PARAM_URI" parameters:FileSink-0 prefix
  require_hfield "$PARAM_URI" parameters:FileSink-0 ext
  if [ "$WITH_TRIGGER_VIEW" = "1" ]; then
    require_hfield "$PARAM_URI" parameters:TriggerView-0 trg-redis-key
  fi
  if [ $fail -ne 0 ]; then
    echo "ERROR: parameter registration incomplete. Devices will not start." >&2
    exit 1
  fi
  echo "[3/6] Parameters complete"
}

verify_topology () {
  echo "[5/6] Verify topology (Redis DB 0)"
  fail=0
  require_hfield "$TOPO_URI" daq_service:topology:endpoint:TFBFilePlayer:out autoSubChannel
  require_hfield "$TOPO_URI" daq_service:topology:endpoint:${FILTER_DEVICE}:in method
  require_hfield "$TOPO_URI" daq_service:topology:endpoint:${FILTER_DEVICE}:out autoSubChannel
  require_hfield "$TOPO_URI" daq_service:topology:endpoint:FileSink:in method
  require_key "$TOPO_URI" "daq_service:topology:link:TFBFilePlayer:out,${FILTER_DEVICE}:in"
  require_key "$TOPO_URI" "daq_service:topology:link:${FILTER_DEVICE}:out,FileSink:in"
  if [ "$WITH_TRIGGER_VIEW" = "1" ]; then
    require_hfield "$TOPO_URI" daq_service:topology:endpoint:${FILTER_DEVICE}:dqm method
    require_hfield "$TOPO_URI" daq_service:topology:endpoint:TriggerView:in method
    require_key "$TOPO_URI" "daq_service:topology:link:${FILTER_DEVICE}:dqm,TriggerView:in"
  fi
  if [ $fail -ne 0 ]; then
    echo "ERROR: topology registration incomplete. Devices will not start." >&2
    exit 1
  fi
  echo "[5/6] Topology complete"
}

ensure_redis

echo "[2/6] Register parameters"
./param-tfbplayer-flt-sink.sh

verify_parameters

echo "[4/6] Register topology"
./topology-tfbplayer-flt-sink.sh

verify_topology

echo "[6/6] Start processes (TFBFilePlayer x1, ${FILTER_DEVICE} x1, FileSink x1)"
if [ -z "$DISPLAY" ]; then
  echo "WARNING: DISPLAY is empty; tmux starts but xterm will not open" >&2
fi
start_group tfbplayer TFBFilePlayer 1 TFB TFBFilePlayer
start_group flt "$FILTER_DEVICE" 1 FLT "$FILTER_DEVICE"
start_group sink FileSink 1 Sink FileSink
if [ "$WITH_TRIGGER_VIEW" = "1" ]; then
  start_group trigview TriggerView 1 TrgView TriggerView
fi

echo "FILTER_DEVICE=${FILTER_DEVICE}"
if [ "$WITH_TRIGGER_VIEW" = "1" ]; then
  echo "tmux sessions: tfbplayer  flt  sink  trigview"
else
  echo "tmux sessions: tfbplayer  flt  sink"
fi
echo "next/prev window: Ctrl-b n / Ctrl-b p"
echo "output: data/fltcpu/ (LogicFilter), data/fltgpu/ (LogicFilterGpu),"
echo "        data/fltslice/ (LogicFilterSlice)"
