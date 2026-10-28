#!/bin/sh
# Write redis-server.txt from topology/run/nestdaq.env, then start Redis + daq-webctl.
# Safe to call when Redis is already up.
HERE=$(CDPATH= cd "$(dirname "$0")" && pwd)
NESTDAQ_ENV_SH=$HERE/nestdaq_env.sh
# shellcheck disable=SC1090
. "$NESTDAQ_ENV_SH"
nestdaq_write_redis_txt

mod=$NESTDAQ/lib/redistimeseries.so
if [ ! -f "$mod" ]; then
  mod=$NESTDAQ/lib64/redistimeseries.so
fi
redis-server "$NESTDAQ/etc/redis.conf" --loadmodule "$mod"
RIHOST=0.0.0.0 redisinsight-linux64 &
daq-webctl >& /dev/null &
