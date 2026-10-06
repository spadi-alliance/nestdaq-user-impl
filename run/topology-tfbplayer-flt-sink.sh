#!/bin/bash
# Topology: TFBFilePlayer x1 -> LogicFilterGpu|LogicFilter x1 -> FileSink x1
#
# Usage:
#   ./topology-tfbplayer-flt-sink.sh
#   FILTER_DEVICE=LogicFilter ./topology-tfbplayer-flt-sink.sh
#   WITH_TRIGGER_VIEW=1 ./topology-tfbplayer-flt-sink.sh   # add the dqm monitor

cd "$(dirname "$0")"
export PATH="${NESTDAQ:-$HOME/nestdaq}/bin:$PATH"

redis_server=`cat redis-server.txt | sed 's/[[:blank:]]//g'`
server=redis://$redis_server/0

if ! redis-cli -u redis://$redis_server ping >/dev/null 2>&1; then
  echo "ERROR: Redis is not running at $redis_server (run ./init.sh)" >&2
  exit 1
fi
set -e

FILTER_DEVICE=${FILTER_DEVICE:-LogicFilterGpu}
case "$FILTER_DEVICE" in
  LogicFilter|LogicFilterGpu|LogicFilterSlice) ;;
  *)
    echo "ERROR: FILTER_DEVICE must be LogicFilter (CPU), LogicFilterGpu (GPU)" >&2
    echo "       or LogicFilterSlice (bit-sliced)" >&2
    exit 1
    ;;
esac

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

endpoint     TFBFilePlayer           out           type push method connect    autoSubChannel true

endpoint     ${FILTER_DEVICE}        in            type pull method bind
endpoint     ${FILTER_DEVICE}        out           type push method connect    autoSubChannel true

endpoint     FileSink                in            type pull  method bind

# The filter only sends on dqm when the channel exists, so registering it is
# what turns the monitor on. TriggerView subscribes, which lets it miss frames
# instead of holding the filter back.
if [ "${WITH_TRIGGER_VIEW:-0}" = "1" ]; then
  endpoint   ${FILTER_DEVICE}        dqm           type pub   method bind     enable-uds false
  endpoint   TriggerView             in            type sub   method connect
fi

echo "---------------------------------------------------------------------"
echo " config link"
echo "---------------------------------------------------------------------"

link    TFBFilePlayer           out                  ${FILTER_DEVICE}  in
link    ${FILTER_DEVICE}        out                  FileSink          in

if [ "${WITH_TRIGGER_VIEW:-0}" = "1" ]; then
  link  ${FILTER_DEVICE}        dqm                  TriggerView       in
fi
