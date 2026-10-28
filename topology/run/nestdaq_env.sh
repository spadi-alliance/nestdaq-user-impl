# Source this file. Do not execute it.
# Lives in topology/run/, next to nestdaq.env.
#   NESTDAQ_ENV_SH=/path/to/topology/run/nestdaq_env.sh
#   . "$NESTDAQ_ENV_SH"
# Bash can omit NESTDAQ_ENV_SH; dash must set it to this file first.

if [ -n "${BASH_SOURCE:-}" ]; then
  _nest_self=$BASH_SOURCE
else
  _nest_self=${NESTDAQ_ENV_SH:-}
fi
if [ -z "${_nest_self}" ]; then
  echo "nestdaq_env.sh: set NESTDAQ_ENV_SH to this file before sourcing" >&2
  return 1 2>/dev/null || exit 1
fi
_nest_self_dir=$(CDPATH= cd "$(dirname "$_nest_self")" && pwd)
NESTDAQ_REPO=$(CDPATH= cd "$_nest_self_dir/../.." && pwd)
if [ -f "$_nest_self_dir/nestdaq.env" ]; then
  # shellcheck disable=SC1091
  . "$_nest_self_dir/nestdaq.env"
fi
: "${NESTDAQ:=$HOME/nestdaq}"
: "${REDIS_SERVER:=127.0.0.1:6379}"
export NESTDAQ REDIS_SERVER NESTDAQ_REPO
export PATH="$NESTDAQ/bin:${PATH:-}"
case ":${LD_LIBRARY_PATH:-}:" in
  *":$NESTDAQ/lib:"*) ;;
  *)
    LD_LIBRARY_PATH="$NESTDAQ/lib:$NESTDAQ/lib64${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    export LD_LIBRARY_PATH
    ;;
esac

nestdaq_write_redis_txt() {
  _dest=$NESTDAQ_REPO/topology/run/redis-server.txt
  mkdir -p "$(dirname "$_dest")"
  printf '%s\n' "$REDIS_SERVER" > "$_dest"
  echo "redis-server.txt  $REDIS_SERVER"
}

nestdaq_redis_host() {
  _file=$NESTDAQ_REPO/topology/run/redis-server.txt
  if [ -f "$_file" ]; then
    sed 's/[[:blank:]]//g' "$_file"
    return 0
  fi
  printf '%s\n' "$REDIS_SERVER"
}
