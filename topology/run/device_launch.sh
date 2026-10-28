# Shared tmux + xterm launch for one device type.
# Sourced by start_from_edit.sh (and can be sourced by run-*.sh).
# Default cwd is topology/run/. Executables come from NESTDAQ_BIN.

# shellcheck source=../xterm_mode.sh
_launch_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
. "$_launch_dir/../xterm_mode.sh"
if [ -z "${NESTDAQ_BIN:-}" ]; then
  NESTDAQ_ENV_SH=$_launch_dir/nestdaq_env.sh
  # shellcheck disable=SC1090
  . "$NESTDAQ_ENV_SH"
fi

XTERM_IDX=${XTERM_IDX:-0}
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

require_binary () {
  local name=$1
  local path="$NESTDAQ_BIN/$name"
  if [ ! -x "$path" ]; then
    echo "ERROR: $path is missing." >&2
    echo "  set BIN_DIR in topology/run/nestdaq.env when executables are not in this repo's bin/" >&2
    ls -1 "$NESTDAQ_BIN" >&2
    return 1
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
  require_binary "$bin" || return 1
  echo "start_group $title  n=$n  bin=$bin  args=$*"
  kill_tmux_group "$session"
  tmux new-session -d -s "$session" -n _boot -- sleep 1
  tmux set-option -t "$session" remain-on-exit on
  local i
  for i in $(seq 0 $((n - 1))); do
    echo "start device $title $i  $_launch_dir/start_device.sh $*"
    tmux new-window -d -n "${wprefix}${i}" -t "$session" -- "$_launch_dir/start_device.sh" "$@"
    sleep 0.15
  done
  tmux kill-window -t "${session}:_boot" 2>/dev/null || tmux kill-window -t "${session}:0" 2>/dev/null || true
  open_xterm "$title" "$session"
}
