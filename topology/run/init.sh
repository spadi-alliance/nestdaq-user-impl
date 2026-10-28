#!/bin/sh
# Write redis-server.txt from topology/run/nestdaq.env, then start whichever
# of Redis, RedisInsight, and daq-webctl is not already running.
# ../server.sh start calls this unless Redis and daq-webctl are both up.
# When both are already running, server.sh start calls ./setup.sh instead.
HERE=$(CDPATH= cd "$(dirname "$0")" && pwd)
NESTDAQ_ENV_SH=$HERE/nestdaq_env.sh
# shellcheck disable=SC1090
. "$NESTDAQ_ENV_SH"
nestdaq_write_redis_txt

addr=$REDIS_SERVER
addr=${addr#redis://}
addr=${addr%%/*}

if redis-cli -u "redis://${addr}" ping >/dev/null 2>&1; then
  echo "Redis is already running at $addr"
else
  echo "Starting Redis at $addr"
  mod=$NESTDAQ/lib/redistimeseries.so
  if [ ! -f "$mod" ]; then
    mod=$NESTDAQ/lib64/redistimeseries.so
  fi
  redis-server "$NESTDAQ/etc/redis.conf" --loadmodule "$mod"
fi

# Linux keeps 15 characters of the process name: redisinsight-li.
if pgrep -x redisinsight-li >/dev/null 2>&1; then
  echo "RedisInsight is already running"
else
  echo "Starting RedisInsight"
  RIHOST=0.0.0.0 redisinsight-linux64 &
fi

if pgrep -x daq-webctl >/dev/null 2>&1; then
  echo "daq-webctl is already running"
else
  echo "Starting daq-webctl"
  daq-webctl >& /dev/null &
fi
