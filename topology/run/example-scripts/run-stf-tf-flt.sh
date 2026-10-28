#!/bin/bash
# Start AmQStrTdcSampler x20 → STFBuilder x20 → TimeFrameBuilder x3
#   → LogicFilter (or LogicFilterGpu) x3 → FileSink x3, and TFB.decimator → DecSink x3.
#
# bin/ name (first arg to start_device.sh) must exist:
#   AmQStrTdcSampler  STFBuilder  TimeFrameBuilder
#   LogicFilter | LogicFilterGpu   FileSink
# DecSink is not a file in bin/. It is:
#   ../start_device.sh FileSink --service-name DecSink
#   (same as run_daid-daq-01/run-stf-tf-flt.sh)
# One tmux session + one xterm per device type (same as run-tfbplayer-2sink.sh).
#
# Lives in topology/run/example-scripts/. NESTDAQ and Redis come from
# ../nestdaq.env.
#
# Sequence:
#   1. Redis ping; if down, ../init.sh
#   2. ./param-stf-tf-flt.sh
#   3. verify parameters
#   4. ./topology-stf-tf-flt.sh
#   5. verify topology
#   6. start processes
#
# Usage:
#   ./run-stf-tf-flt.sh
#   FILTER_DEVICE=LogicFilterGpu ./run-stf-tf-flt.sh

set -e

cd "$(dirname "$0")"
NESTDAQ_ENV_SH=$(CDPATH= cd "$(dirname "$0")/.." && pwd)/nestdaq_env.sh
# shellcheck disable=SC1090
. "$NESTDAQ_ENV_SH"
# shellcheck source=../../xterm_mode.sh
. ../../xterm_mode.sh

FILTER_DEVICE=${FILTER_DEVICE:-LogicFilter}

N_SAMPLER=${N_SAMPLER:-20}
N_STF=${N_STF:-20}
N_TFB=${N_TFB:-3}
N_FLT=${N_FLT:-3}
N_SINK=${N_SINK:-3}
N_DEC=${N_DEC:-3}

redis_server=$(nestdaq_redis_host)
PARAM_URI=redis://$redis_server/2
TOPO_URI=redis://$redis_server/0

fail=0

ensure_redis () {
  if redis-cli -u redis://$redis_server ping >/dev/null 2>&1; then
    echo "[1/6] Redis already running at $redis_server"
    return 0
  fi
  echo "[1/6] Redis is down. Starting ../init.sh ..."
  ../init.sh
  local i=0
  while [ $i -lt 30 ]; do
    if redis-cli -u redis://$redis_server ping >/dev/null 2>&1; then
      echo "[1/6] Redis is ready"
      return 0
    fi
    i=$((i + 1))
    sleep 0.2
  done
  echo "ERROR: Redis did not become ready after ../init.sh ($redis_server)" >&2
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
  require_hfield "$PARAM_URI" parameters:AmQStrTdcSampler-0 msiTcpIp
  require_hfield "$PARAM_URI" parameters:AmQStrTdcSampler-$((N_SAMPLER - 1)) msiTcpIp
  require_hfield "$PARAM_URI" parameters:STFBuilder-0 max-hbf
  require_hfield "$PARAM_URI" parameters:TimeFrameBuilder-0 decimation-factor
  require_hfield "$PARAM_URI" parameters:${FILTER_DEVICE}-0 trigger-expression
  require_hfield "$PARAM_URI" parameters:FileSink-0 prefix
  require_hfield "$PARAM_URI" parameters:DecSink-0 prefix
  if [ $fail -ne 0 ]; then
    echo "ERROR: parameter registration incomplete. Devices will not start." >&2
    exit 1
  fi
  echo "[3/6] Parameters complete"
}

verify_topology () {
  echo "[5/6] Verify topology (Redis DB 0)"
  fail=0
  require_hfield "$TOPO_URI" daq_service:topology:endpoint:AmQStrTdcSampler:out type
  require_hfield "$TOPO_URI" daq_service:topology:endpoint:STFBuilder:out autoSubChannel
  require_hfield "$TOPO_URI" daq_service:topology:endpoint:TimeFrameBuilder:out autoSubChannel
  require_hfield "$TOPO_URI" daq_service:topology:endpoint:TimeFrameBuilder:decimator type
  require_hfield "$TOPO_URI" daq_service:topology:endpoint:${FILTER_DEVICE}:out autoSubChannel
  require_hfield "$TOPO_URI" daq_service:topology:endpoint:FileSink:in method
  require_hfield "$TOPO_URI" daq_service:topology:endpoint:DecSink:in method
  require_key "$TOPO_URI" "daq_service:topology:link:AmQStrTdcSampler:out,STFBuilder:in"
  require_key "$TOPO_URI" "daq_service:topology:link:STFBuilder:out,TimeFrameBuilder:in"
  require_key "$TOPO_URI" "daq_service:topology:link:TimeFrameBuilder:out,${FILTER_DEVICE}:in"
  require_key "$TOPO_URI" "daq_service:topology:link:TimeFrameBuilder:decimator,DecSink:in"
  require_key "$TOPO_URI" "daq_service:topology:link:${FILTER_DEVICE}:out,FileSink:in"
  if [ $fail -ne 0 ]; then
    echo "ERROR: topology registration incomplete. Devices will not start." >&2
    exit 1
  fi
  echo "[5/6] Topology complete"
}

XTERM_IDX=0
# Remote X11 here is 1280x720 and often only has 6x13 (not 8x16).
# Size cols/rows from the real glyph so 3x2 windows fill the screen
# with a 4px gap. Assuming 8x16 left a ~120px hole between windows.
XTERM_NCOL=3
XTERM_NROW=2
XTERM_GAP=4
XTERM_CHROME_W=12
XTERM_CHROME_H=36
XTERM_FN=6x13
XTERM_CELL_W=6
XTERM_CELL_H=13

xterm_pick_font () {
  local cand
  for cand in 8x16 7x14 6x13 6x12 fixed; do
    if xlsfonts -fn "$cand" 2>/dev/null | grep -q .; then
      XTERM_FN=$cand
      if [[ $cand =~ ^([0-9]+)x([0-9]+)$ ]]; then
        XTERM_CELL_W=${BASH_REMATCH[1]}
        XTERM_CELL_H=${BASH_REMATCH[2]}
      fi
      return
    fi
  done
}

xterm_geometry () {
  local idx=$1
  local screen_w=1280 screen_h=720
  local dim
  dim=$(xdpyinfo 2>/dev/null | awk '/dimensions:/ {print $2; exit}')
  if [ -n "$dim" ]; then
    screen_w=${dim%x*}
    screen_h=${dim#*x}
  fi
  local slot_w=$(( (screen_w - XTERM_GAP * (XTERM_NCOL + 1)) / XTERM_NCOL ))
  local slot_h=$(( (screen_h - XTERM_GAP * (XTERM_NROW + 1)) / XTERM_NROW ))
  local cols=$(( (slot_w - XTERM_CHROME_W) / XTERM_CELL_W ))
  local rows=$(( (slot_h - XTERM_CHROME_H) / XTERM_CELL_H ))
  [ "$cols" -lt 40 ] && cols=40
  [ "$rows" -lt 10 ] && rows=10
  local col=$((idx % XTERM_NCOL))
  local row=$(( (idx / XTERM_NCOL) % XTERM_NROW ))
  local x=$((XTERM_GAP + col * (slot_w + XTERM_GAP)))
  local y=$((XTERM_GAP + row * (slot_h + XTERM_GAP)))
  echo "${cols}x${rows}+${x}+${y}"
}

open_xterm () {
  local title=$1 session=$2
  if ! want_xterm; then
    echo "xterm off: $title  tmux session $session  (view in topology)"
    return 0
  fi
  if [ -z "$DISPLAY" ] || ! command -v xterm >/dev/null 2>&1; then
    return
  fi
  if [ "$XTERM_IDX" -eq 0 ]; then
    xterm_pick_font
    echo "xterm font $XTERM_FN  cell ${XTERM_CELL_W}x${XTERM_CELL_H}  gap ${XTERM_GAP}px"
  fi
  local geom
  geom=$(xterm_geometry "$XTERM_IDX")
  XTERM_IDX=$((XTERM_IDX + 1))
  echo "xterm $title  $geom"
  xterm -fn "$XTERM_FN" -b 1 -geometry "$geom" -T "$title" -e tmux a -t "$session" &
}

# bin/ file name must match the first argument to start_device.sh
# service name is the binary name, except DecSink = FileSink --service-name DecSink
require_binary () {
  local name=$1
  local path="../../../bin/$name"
  if [ ! -x "$path" ]; then
    echo "ERROR: $path is missing. Shell name must match bin/." >&2
    ls -1 ../../../bin >&2
    exit 1
  fi
  echo "  binary OK  $path"
}

kill_tmux_group () {
  local g=$1
  tmux list-sessions -F '#{session_name} #{session_group}' 2>/dev/null | while read -r name grp; do
    if [ "$name" = "$g" ] || [ "$grp" = "$g" ]; then
      tmux kill-session -t "$name" 2>/dev/null || true
    fi
  done
}

start_group () {
  local session=$1
  local title=$2
  local n=$3
  local wprefix=$4
  shift 4
  local bin=$1
  require_binary "$bin"
  echo "start_group $title  n=$n  bin=$bin  args=$*"
  kill_tmux_group "$session"
  tmux new-session -d -s "$session" -n _boot -- sleep 1
  tmux set-option -t "$session" remain-on-exit on
  local i
  for i in $(seq 0 $((n - 1))); do
    echo "start device $title $i  ../start_device.sh $*"
    tmux new-window -d -n "${wprefix}${i}" -t "$session" -- ../start_device.sh "$@"
    sleep 0.15
  done
  tmux kill-window -t "${session}:_boot" 2>/dev/null || tmux kill-window -t "${session}:0" 2>/dev/null || true
  open_xterm "$title" "$session"
}

mkdir -p data/tdcdata data/dcmdata

ensure_redis

echo "[2/6] Register parameters"
FILTER_DEVICE=$FILTER_DEVICE \
N_SAMPLER=$N_SAMPLER N_STF=$N_STF N_TFB=$N_TFB N_FLT=$N_FLT N_SINK=$N_SINK N_DEC=$N_DEC \
  ./param-stf-tf-flt.sh

verify_parameters

echo "[4/6] Register topology"
FILTER_DEVICE=$FILTER_DEVICE ./topology-stf-tf-flt.sh

verify_topology

echo "[6/6] Start processes (Sampler $N_SAMPLER  STF $N_STF  TFB $N_TFB  ${FILTER_DEVICE} $N_FLT  Sink $N_SINK  Dec $N_DEC)"
if [ -z "$DISPLAY" ]; then
  echo "WARNING: DISPLAY is empty; tmux starts but xterm will not open" >&2
fi
start_group sampler AmQStrTdcSampler "$N_SAMPLER" S AmQStrTdcSampler
start_group stfb STFBuilder "$N_STF" STF STFBuilder
start_group tfb TimeFrameBuilder "$N_TFB" TFB TimeFrameBuilder
start_group flt "$FILTER_DEVICE" "$N_FLT" FLT "$FILTER_DEVICE"
start_group sink FileSink "$N_SINK" Sink FileSink
start_group decsink DecSink "$N_DEC" Dec FileSink --service-name DecSink

echo "tmux sessions: sampler  stfb  tfb  flt  sink  decsink"
echo "next/prev window: Ctrl-b n / Ctrl-b p"
echo "output: data/tdcdata/NN  and  data/dcmdata/NN"
echo "viewer: after presence appears, http://127.0.0.1:8765/"
