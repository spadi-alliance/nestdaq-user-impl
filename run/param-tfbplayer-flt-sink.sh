#!/bin/bash
# Parameters: TFBFilePlayer-0 -> LogicFilterGpu-0 or LogicFilter-0 -> FileSink-0
#
# Usage:
#   ./param-tfbplayer-flt-sink.sh
#   FILTER_DEVICE=LogicFilter ./param-tfbplayer-flt-sink.sh
#   TFB_INFILE=/path/to/tf.dat ./param-tfbplayer-flt-sink.sh
#   WITH_TRIGGER_VIEW=1 ./param-tfbplayer-flt-sink.sh   # also register TriggerView

cd "$(dirname "$0")"
export PATH="${NESTDAQ:-$HOME/nestdaq}/bin:$PATH"

redis_server=`cat redis-server.txt | sed 's/[[:blank:]]//g'`
server=redis://$redis_server/2

if ! redis-cli -u redis://$redis_server ping >/dev/null 2>&1; then
  echo "ERROR: Redis is not running at $redis_server (run ./init.sh)" >&2
  exit 1
fi
set -e

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

TFB_INFILE=${TFB_INFILE:-$PWD/data/input/run000400.dat}
case "$FILTER_DEVICE" in
  LogicFilter)      SINK_PREFIX=${SINK_PREFIX:-$PWD/data/fltcpu} ;;
  LogicFilterSlice) SINK_PREFIX=${SINK_PREFIX:-$PWD/data/fltslice} ;;
  *)                SINK_PREFIX=${SINK_PREFIX:-$PWD/data/fltgpu} ;;
esac

function param () {
  echo redis-cli -u $server hset parameters:$1 ${@:2}
  redis-cli -u $server hset parameters:$1 ${@:2} || return 1
}

if [ ! -f "$TFB_INFILE" ]; then
  echo "ERROR: input file not found: $TFB_INFILE" >&2
  exit 1
fi
mkdir -p "$SINK_PREFIX"

redis-cli -u $server flushdb || exit 1

param TFBFilePlayer-0 \
  in-file "${TFB_INFILE}" \
  wait 0 \
  max-iterations 0 \
  split 2 \
  recbe false

param FileSink-0 \
  multipart true \
  prefix "${SINK_PREFIX}" \
  ext .dat \
  openmode recreate

param ${FILTER_DEVICE}-0 \
  data-suppress true \
  remove-hb false \
  split 2 \
  poll-timeout 1

# LogicFilter trigger (see LogicFilter.md)
# This parser reads each parenthesis as (module channel offset).
# Signal ids are the parenthesis order, starting at 0.
# The 4-field form "(id module channel offset)" would take id as the
# module and drop the real offset, so the leading id is omitted here.
#
#   0  0xc0a802a9 ch  8 offset   0
#   1  0xc0a802a9 ch 10 offset   0
#   2  0xc0a802aa ch 16 offset -12
#   ...
#  13  0xc0a802aa ch 28 offset -12
# Expression: (0 AND 1) AND (2 OR 3 OR ... OR 13)
# trigger-width: coincidence window in 4 ns bins
TRIG_SIGNALS='(0xc0a802a9 8 0) (0xc0a802a9 10 0) (0xc0a802aa 16 -12) (0xc0a802aa 17 -12) (0xc0a802aa 18 -12) (0xc0a802aa 19 -12) (0xc0a802aa 20 -12) (0xc0a802aa 21 -12) (0xc0a802aa 22 -12) (0xc0a802aa 23 -12) (0xc0a802aa 24 -12) (0xc0a802aa 25 -12) (0xc0a802aa 27 -12) (0xc0a802aa 28 -12)'
TRIG_EXPR='(0 & 1) & (2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13)'
TRIG_WIDTH='10'
# LogicFilterSlice only: cpu or gpu
TRIG_BACKEND=${TRIG_BACKEND:-cpu}

for key in ${FILTER_DEVICE} ${FILTER_DEVICE}-0
do
  echo redis-cli -u $server hset parameters:$key trigger-signals "${TRIG_SIGNALS}"
  redis-cli -u $server hset parameters:$key trigger-signals "${TRIG_SIGNALS}" || exit 1
  echo redis-cli -u $server hset parameters:$key trigger-expression "${TRIG_EXPR}"
  redis-cli -u $server hset parameters:$key trigger-expression "${TRIG_EXPR}" || exit 1
  echo redis-cli -u $server hset parameters:$key trigger-width "${TRIG_WIDTH}"
  redis-cli -u $server hset parameters:$key trigger-width "${TRIG_WIDTH}" || exit 1
  if [ "$FILTER_DEVICE" = "LogicFilterSlice" ]; then
    echo redis-cli -u $server hset parameters:$key trigger-backend "${TRIG_BACKEND}"
    redis-cli -u $server hset parameters:$key trigger-backend "${TRIG_BACKEND}" || exit 1
  fi
done

# TriggerView reads the signal list itself rather than taking it from the
# stream, and its default key names LogicFilter, so it has to be pointed at
# whichever filter is actually running.
if [ "${WITH_TRIGGER_VIEW:-0}" = "1" ]; then
  for key in TriggerView TriggerView-0
  do
    param $key \
      trg-redis-uri "redis://$redis_server/2" \
      trg-redis-key "parameters:${FILTER_DEVICE}-0" \
      max-iterations 0
  done
  echo "---- TriggerView-0 ----"
  redis-cli -u $server hgetall parameters:TriggerView-0
fi

echo "---- ${FILTER_DEVICE} ----"
redis-cli -u $server hgetall parameters:${FILTER_DEVICE}
echo "---- ${FILTER_DEVICE}-0 ----"
redis-cli -u $server hgetall parameters:${FILTER_DEVICE}-0
echo "---- TFBFilePlayer-0 ----"
redis-cli -u $server hgetall parameters:TFBFilePlayer-0
echo "---- FileSink-0 ----"
redis-cli -u $server hgetall parameters:FileSink-0
echo "FILTER_DEVICE=${FILTER_DEVICE}"
echo "TFB_INFILE=${TFB_INFILE}"
echo "SINK_PREFIX=${SINK_PREFIX}"
