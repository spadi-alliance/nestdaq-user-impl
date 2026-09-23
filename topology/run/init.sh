#!/bin/sh
# Start Redis + daq-webctl. Safe to call when Redis is already up
# (redis-server daemonize will bind or report the existing instance).
NESTDAQ=${NESTDAQ:-$HOME/nestdaq}
export PATH="$NESTDAQ/bin:$PATH"

redis-server "$NESTDAQ/etc/redis.conf" --loadmodule "$NESTDAQ/lib/redistimeseries.so"
RIHOST=0.0.0.0 redisinsight-linux64 &
#daq-webctl >& $NESTDAQ/log/daq-webctl.log &
daq-webctl >& /dev/null &
