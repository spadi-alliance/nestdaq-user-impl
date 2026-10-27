#!/bin/bash
# Topology (same pairing as run_daid-daq-01/topology-stf-tf-flt.sh):
#   AmQStrTdcSampler x20 --1:1--> STFBuilder x20
#     STF.out autoSubChannel true --> TimeFrameBuilder x3
#     TFB.out autoSubChannel true --> LogicFilter(Gpu) x3 --> FileSink x3
#     TFB.decimator (no autoSub) --> DecSink x3  (1:1 each)
#
# Lives in example/. Redis address is example/redis-server.txt.
# If Redis is down, start it with ../run/init.sh.
#
# Usage:
#   ./topology-stf-tf-flt.sh
#   FILTER_DEVICE=LogicFilterGpu ./topology-stf-tf-flt.sh

cd "$(dirname "$0")"
export PATH="${NESTDAQ:-$HOME/nestdaq}/bin:$PATH"

redis_server=`cat redis-server.txt | sed 's/[[:blank:]]//g'`
server=redis://$redis_server/0

if ! redis-cli -u redis://$redis_server ping >/dev/null 2>&1; then
  echo "ERROR: Redis is not running at $redis_server (run ../run/init.sh)" >&2
  exit 1
fi
set -e

FILTER_DEVICE=${FILTER_DEVICE:-LogicFilter}

function endpoint () {
  echo redis-cli -u $server hset daq_service:topology:endpoint:$1:$2 ${@:3}
  redis-cli -u $server hset daq_service:topology:endpoint:$1:$2 ${@:3} || return 1
}

function link () {
  echo redis-cli -u $server set daq_service:topology:link:$1:$2,$3:$4 none
  redis-cli -u $server set daq_service:topology:link:$1:$2,$3:$4 none || return 1
}

echo "Clear DAQ service keys"
keys=`redis-cli -u $server keys 'daq_service:*'`
if [ -n "$keys" ]; then
  echo "$keys" | xargs redis-cli -u $server del
fi

echo "---------------------------------------------------------------------"
echo " config endpoint (socket)  FILTER_DEVICE=${FILTER_DEVICE}"
echo "---------------------------------------------------------------------"

endpoint     AmQStrTdcSampler        out           type push  method bind       autoSubChannel false

endpoint     STFBuilder              in            type pull  method connect    autoSubChannel false
endpoint     STFBuilder              out           type push  method connect    autoSubChannel true

endpoint     TimeFrameBuilder        in            type pull  method bind
endpoint     TimeFrameBuilder        out           type push  method connect    autoSubChannel true
endpoint     TimeFrameBuilder        decimator     type push  method connect

endpoint     ${FILTER_DEVICE}        in            type pull  method bind
endpoint     ${FILTER_DEVICE}        out           type push  method connect    autoSubChannel true

endpoint     FileSink                in            type pull  method bind
# DecSink is the FileSink binary with --service-name DecSink (no DecSink file in bin/)
endpoint     DecSink                 in            type pull  method bind

echo "---------------------------------------------------------------------"
echo " config link"
echo "---------------------------------------------------------------------"

link    AmQStrTdcSampler        out                  STFBuilder        in
link    STFBuilder              out                  TimeFrameBuilder  in
link    TimeFrameBuilder        out                  ${FILTER_DEVICE}  in
link    TimeFrameBuilder        decimator            DecSink           in
link    ${FILTER_DEVICE}        out                  FileSink          in
