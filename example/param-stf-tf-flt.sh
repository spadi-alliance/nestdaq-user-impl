#!/bin/bash
# Parameters for AmQStrTdcSampler x20 → STF x20 → TFB x3 → LogicFilter(Gpu) x3 → FileSink x3
# and TFB.decimator → DecSink x3.
#
# Sampler IPs default to 192.168.2.101 .. 120 (override with SAMPLER_IP_BASE / START).
#
# Lives in example/. Redis address is example/redis-server.txt.
# If Redis is down, start it with ../run/init.sh.
#
# Usage:
#   ./param-stf-tf-flt.sh
#   FILTER_DEVICE=LogicFilterGpu ./param-stf-tf-flt.sh

cd "$(dirname "$0")"
export PATH="${NESTDAQ:-$HOME/nestdaq}/bin:$PATH"

redis_server=`cat redis-server.txt | sed 's/[[:blank:]]//g'`
server=redis://$redis_server/2

if ! redis-cli -u redis://$redis_server ping >/dev/null 2>&1; then
  echo "ERROR: Redis is not running at $redis_server (run ../run/init.sh)" >&2
  exit 1
fi
set -e

FILTER_DEVICE=${FILTER_DEVICE:-LogicFilter}

N_SAMPLER=${N_SAMPLER:-20}
N_STF=${N_STF:-20}
N_TFB=${N_TFB:-3}
N_FLT=${N_FLT:-3}
N_SINK=${N_SINK:-3}
N_DEC=${N_DEC:-3}

SAMPLER_IP_BASE=${SAMPLER_IP_BASE:-192.168.2}
SAMPLER_IP_START=${SAMPLER_IP_START:-101}
TDC_TYPE=${TDC_TYPE:-5}

function param () {
  echo redis-cli -u $server hset parameters:$1 ${@:2}
  redis-cli -u $server hset parameters:$1 ${@:2} || return 1
}

redis-cli -u $server flushdb || exit 1

# TdcType: HR-TDC pairing 5, LR-TDC pairing 6 (see run_daid-daq-01/mq-param.sh)
i=0
while [ "$i" -lt "$N_SAMPLER" ]; do
  ip="${SAMPLER_IP_BASE}.$((SAMPLER_IP_START + i))"
  param AmQStrTdcSampler-$i msiTcpIp "$ip" TdcType "$TDC_TYPE"
  i=$((i + 1))
done

i=0
while [ "$i" -lt "$N_STF" ]; do
  param STFBuilder-$i max-hbf 4
  i=$((i + 1))
done

i=0
while [ "$i" -lt "$N_TFB" ]; do
  param TimeFrameBuilder-$i decimation-factor 1000 decimation-offset 0
  i=$((i + 1))
done

i=0
while [ "$i" -lt "$N_SINK" ]; do
  dir=$(printf "%s/data/tdcdata/%02d" "$PWD" "$i")
  mkdir -p "$dir"
  param FileSink-$i multipart true prefix "$dir" ext .dat openmode recreate
  i=$((i + 1))
done

i=0
while [ "$i" -lt "$N_DEC" ]; do
  dir=$(printf "%s/data/dcmdata/%02d" "$PWD" "$i")
  mkdir -p "$dir"
  param DecSink-$i multipart true prefix "$dir" ext .dat openmode recreate
  i=$((i + 1))
done

TRIG_SIGNALS='(0xc0a802a9 0 0) (0xc0a802a9 1 0) (0xc0a802a9 2 0) (0xc0a802a9 3 0)'
TRIG_EXPR='RPN 0 1 & 2 & 3 &'
TRIG_WIDTH='10'

param ${FILTER_DEVICE} \
  data-suppress true \
  remove-hb false \
  split 2 \
  poll-timeout 1

i=0
while [ "$i" -lt "$N_FLT" ]; do
  param ${FILTER_DEVICE}-$i \
    data-suppress true \
    remove-hb false \
    split 2 \
    poll-timeout 1
  i=$((i + 1))
done

echo redis-cli -u $server hset parameters:${FILTER_DEVICE} trigger-signals "${TRIG_SIGNALS}"
redis-cli -u $server hset parameters:${FILTER_DEVICE} trigger-signals "${TRIG_SIGNALS}" || exit 1
echo redis-cli -u $server hset parameters:${FILTER_DEVICE} trigger-expression "${TRIG_EXPR}"
redis-cli -u $server hset parameters:${FILTER_DEVICE} trigger-expression "${TRIG_EXPR}" || exit 1
echo redis-cli -u $server hset parameters:${FILTER_DEVICE} trigger-width "${TRIG_WIDTH}"
redis-cli -u $server hset parameters:${FILTER_DEVICE} trigger-width "${TRIG_WIDTH}" || exit 1

i=0
while [ "$i" -lt "$N_FLT" ]; do
  echo redis-cli -u $server hset parameters:${FILTER_DEVICE}-$i trigger-signals "${TRIG_SIGNALS}"
  redis-cli -u $server hset parameters:${FILTER_DEVICE}-$i trigger-signals "${TRIG_SIGNALS}" || exit 1
  echo redis-cli -u $server hset parameters:${FILTER_DEVICE}-$i trigger-expression "${TRIG_EXPR}"
  redis-cli -u $server hset parameters:${FILTER_DEVICE}-$i trigger-expression "${TRIG_EXPR}" || exit 1
  echo redis-cli -u $server hset parameters:${FILTER_DEVICE}-$i trigger-width "${TRIG_WIDTH}"
  redis-cli -u $server hset parameters:${FILTER_DEVICE}-$i trigger-width "${TRIG_WIDTH}" || exit 1
  i=$((i + 1))
done

echo "parameters registered: Sampler $N_SAMPLER  STF $N_STF  TFB $N_TFB  ${FILTER_DEVICE} $N_FLT  Sink $N_SINK  Dec $N_DEC"
