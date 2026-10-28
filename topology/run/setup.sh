#!/bin/sh
# Confirm the Redis address for the web monitor.
# Reads topology/run/nestdaq.env and writes topology/run/redis-server.txt.
# Does not start Redis, RedisInsight, or daq-webctl.
# ../server.sh start calls this when Redis and daq-webctl are already running.
# When either is not running, server.sh start calls ./init.sh instead.
HERE=$(CDPATH= cd "$(dirname "$0")" && pwd)
NESTDAQ_ENV_SH=$HERE/nestdaq_env.sh
# shellcheck disable=SC1090
. "$NESTDAQ_ENV_SH"
nestdaq_write_redis_txt
