#!/bin/bash
# Start device groups from JSON written by the edit page:
#   {"services":[{"name":"STFBuilder","binary":"STFBuilder","n":20}, ...]}
#
#   ./start_from_edit.sh /path/to.json

set -e
cd "$(dirname "$0")"
# shellcheck source=device_launch.sh
. ./device_launch.sh

JSON=${1:?usage: start_from_edit.sh file.json}
if [ ! -f "$JSON" ]; then
  echo "ERROR: no json $JSON" >&2
  exit 1
fi

if [ -z "$DISPLAY" ]; then
  echo "WARNING: DISPLAY is empty; tmux starts but xterm will not open" >&2
fi

python3 - "$JSON" <<'PY' | while read -r session name binary n extra; do
import json, re, sys
d = json.load(open(sys.argv[1]))
for s in d.get("services") or []:
    name = str(s.get("name") or "").strip()
    binary = str(s.get("binary") or name).strip()
    try:
        n = int(s.get("n") or 0)
    except (TypeError, ValueError):
        n = 0
    if not name or not binary or n < 1:
        continue
    n = min(n, 200)
    session = re.sub(r"[^A-Za-z0-9]", "", name).lower()[:20] or "dev"
    extra = "--service-name" if name != binary else "-"
    print(session, name, binary, n, extra)
PY
  [ -z "$session" ] && continue
  wprefix=$(echo "$name" | cut -c1-4)
  if [ "$extra" = "--service-name" ]; then
    start_group "$session" "$name" "$n" "$wprefix" "$binary" --service-name "$name"
  else
    start_group "$session" "$name" "$n" "$wprefix" "$binary"
  fi
done

echo "started from $JSON"
