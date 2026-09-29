"""Read-only tmux pane text for one FairMQ instance."""

from __future__ import annotations

import getpass
import json
import re
import socket
import subprocess
import time
from pathlib import Path

SAFE_ID = re.compile(r"^[A-Za-z0-9][A-Za-z0-9_.-]{0,80}$")
ANSI = re.compile(r"\x1b\[[0-9;?]*[A-Za-z]")
SSH_SPEC = re.compile(r"^[A-Za-z0-9_][A-Za-z0-9_.-]*@[A-Za-z0-9][A-Za-z0-9.-]*$")
PANE_TARGET = re.compile(r"^[A-Za-z0-9_.-]+:\d+\.\d+$")
_CACHE: dict = {}
_NEG: dict = {}

# Runs on the SSH target. Prints every pane's command line and text.
_REMOTE_DUMP = r"""
import json, subprocess
from pathlib import Path

def cmdline(pid):
    try:
        raw = Path("/proc/%s/cmdline" % pid).read_bytes()
    except OSError:
        return ""
    return raw.replace(b"\0", b" ").decode("utf-8", "replace")

def children(pid):
    out = []
    want = str(pid)
    try:
        names = list(Path("/proc").iterdir())
    except OSError:
        return out
    for entry in names:
        if not entry.name.isdigit():
            continue
        try:
            stat = (entry / "stat").read_text()
        except OSError:
            continue
        cut = stat.rfind(")")
        if cut < 0:
            continue
        fields = stat[cut + 2:].split()
        if len(fields) > 1 and fields[1] == want:
            out.append(int(entry.name))
    return out

def tree(pid):
    if pid <= 0:
        return ""
    chunks, stack, seen = [], [pid], set()
    while stack and len(seen) < 40:
        cur = stack.pop()
        if cur in seen:
            continue
        seen.add(cur)
        chunks.append(cmdline(cur))
        stack.extend(children(cur))
    return "\n".join(chunks)

proc = subprocess.run(
    ["tmux", "list-panes", "-a", "-F",
     "#{session_name}\t#{window_index}\t#{window_name}\t#{pane_index}\t#{pane_pid}"],
    capture_output=True, text=True,
)
rows = []
if proc.returncode == 0:
    for line in (proc.stdout or "").splitlines():
        parts = line.split("\t")
        if len(parts) < 5:
            continue
        session, widx, wname, pidx, pid = parts[:5]
        target = "%s:%s.%s" % (session, widx, pidx)
        cap = subprocess.run(
            ["tmux", "capture-pane", "-t", target, "-p", "-S", "-200"],
            capture_output=True, text=True,
        )
        rows.append({
            "session": session,
            "window": wname,
            "target": target,
            "cmd": tree(int(pid) if pid.isdigit() else 0),
            "text": cap.stdout if cap.returncode == 0 else "",
        })
print("BEGINJSON")
print(json.dumps(rows))
print("ENDJSON")
"""


def capture_instance(instance_id: str, host: str = "", ip: str = "", account: str = "") -> dict:
    instance_id = (instance_id or "").strip()
    if not SAFE_ID.fullmatch(instance_id):
        raise ValueError("bad instance id")
    now = time.time()
    cached = _CACHE.get(instance_id)
    if cached and now - cached["ts"] < 60:
        text = _capture_row(cached)
        if text is not None and (_has_id(text, instance_id) or cached.get("weak")):
            return _ok(instance_id, cached, text)
    neg = _NEG.get(instance_id)
    if neg and now - neg < 5 and not cached:
        return {
            "ok": True,
            "found": False,
            "id": instance_id,
            "error": "no tmux pane contains this instance",
        }
    spec = _ssh_target(host, ip, account)
    found = _pick(instance_id, spec)
    if not found:
        _NEG[instance_id] = now
        where = f" or on {spec}" if spec else ""
        return {
            "ok": True,
            "found": False,
            "id": instance_id,
            "host": spec,
            "error": f"no tmux pane contains this instance{where}",
        }
    _NEG.pop(instance_id, None)
    found["ts"] = now
    _CACHE[instance_id] = found
    return _ok(instance_id, found, found["text"])


def _ok(instance_id: str, row: dict, text: str) -> dict:
    lines = _strip(text).splitlines()
    if len(lines) > 80:
        lines = lines[-80:]
    return {
        "ok": True,
        "found": True,
        "id": instance_id,
        "session": row.get("session") or "",
        "window": row.get("window") or "",
        "host": row.get("ssh") or "",
        "match": row.get("match") or "",
        "text": "\n".join(lines),
    }


def _pick(instance_id: str, spec: str) -> dict | None:
    best = _find(instance_id)
    remote = _find_ssh(instance_id, spec)
    if remote and (not best or remote.get("score", 0) > best.get("score", 0)):
        return remote
    return best


def _find(instance_id: str) -> dict | None:
    panes = _panes()
    if panes is None:
        return None
    service, index = _split_id(instance_id)
    best = None
    best_score = 0
    for pane in panes:
        cmd = _tree_cmd(pane["pid"])
        score = 0
        match = ""
        text = ""
        if _has_id(cmd, instance_id):
            score = 20
            match = "process"
        else:
            text = _capture(pane["target"]) or ""
            own = len(re.findall(r"id = " + re.escape(instance_id) + r"(?![\w.-])", text))
            if own:
                score = 15 + min(own, 4)
                match = "log"
            elif _has_id(text, instance_id):
                score = 4
                match = "mention"
        if score == 0 and service and _has_id(cmd, service) and pane["window"].endswith(index):
            score = 3
            match = "window"
            text = text or (_capture(pane["target"]) or "")
        if score > best_score:
            best_score = score
            best = {
                "target": pane["target"],
                "session": pane["session"],
                "window": pane["window"],
                "ssh": "",
                "match": match,
                "text": text,
                "score": score,
                "weak": score < 10,
            }
    if best_score < 3:
        return None
    if not best["text"]:
        best["text"] = _capture(best["target"]) or ""
    return best


def _split_id(instance_id: str) -> tuple:
    if "-" not in instance_id:
        return instance_id, ""
    service, tail = instance_id.rsplit("-", 1)
    if not tail.isdigit():
        return instance_id, ""
    return service, tail


def _has_id(text: str, token: str) -> bool:
    if not text or not token:
        return False
    return re.search(r"(?<![\w.-])" + re.escape(token) + r"(?![\w.-])", text) is not None


def _panes() -> list | None:
    try:
        proc = subprocess.run(
            [
                "tmux", "list-panes", "-a", "-F",
                "#{session_name}\t#{window_index}\t#{window_name}\t#{pane_index}\t#{pane_pid}",
            ],
            capture_output=True,
            text=True,
            timeout=3,
        )
    except (OSError, subprocess.TimeoutExpired):
        return None
    if proc.returncode != 0:
        return []
    rows = []
    for line in (proc.stdout or "").splitlines():
        parts = line.split("\t")
        if len(parts) < 5:
            continue
        session, widx, wname, pidx, pid = parts[:5]
        rows.append({
            "session": session,
            "window": wname,
            "target": f"{session}:{widx}.{pidx}",
            "pid": int(pid) if pid.isdigit() else 0,
        })
    return rows


def _capture(target: str) -> str | None:
    try:
        proc = subprocess.run(
            ["tmux", "capture-pane", "-t", target, "-p", "-S", "-200"],
            capture_output=True,
            text=True,
            timeout=3,
        )
    except (OSError, subprocess.TimeoutExpired):
        return None
    if proc.returncode != 0:
        return None
    return proc.stdout or ""


def _strip(text: str) -> str:
    return ANSI.sub("", text or "").replace("\r", "")


def _ssh_target(host: str, ip: str, account: str = "") -> str:
    """SSH when the process is on another machine, or another account on this one."""
    place = (ip or "").strip() or (host or "").strip()
    user = (account or "").strip()
    me = getpass.getuser()
    if place and not _is_here(place):
        spec = f"{me}@{place}"
    elif user and user != me:
        spec = f"{user}@{socket.gethostname()}"
    else:
        return ""
    if not SSH_SPEC.fullmatch(spec):
        return ""
    return spec


def _is_here(place: str) -> bool:
    token = place.strip().lower().rstrip(".")
    if token in {"localhost", "127.0.0.1", "::1", "0.0.0.0"}:
        return True
    names = {socket.gethostname().lower()}
    try:
        names.add(socket.getfqdn().lower())
    except OSError:
        pass
    if token in names:
        return True
    ips = {"127.0.0.1", "::1"}
    try:
        proc = subprocess.run(["hostname", "-I"], capture_output=True, text=True, timeout=2)
        ips.update(part.strip().lower() for part in (proc.stdout or "").split() if part.strip())
    except (OSError, subprocess.TimeoutExpired):
        pass
    return token in ips


def _capture_row(row: dict) -> str | None:
    spec = row.get("ssh") or ""
    if spec:
        return _capture_ssh(spec, row.get("target") or "")
    return _capture(row.get("target") or "")


def _capture_ssh(spec: str, target: str) -> str | None:
    if not SSH_SPEC.fullmatch(spec) or not PANE_TARGET.fullmatch(target):
        return None
    try:
        proc = subprocess.run(
            [
                "ssh", "-T", "-o", "BatchMode=yes", "-o", "ConnectTimeout=5", spec,
                "tmux", "capture-pane", "-t", target, "-p", "-S", "-200",
            ],
            capture_output=True,
            text=True,
            timeout=8,
        )
    except (OSError, subprocess.TimeoutExpired):
        return None
    if proc.returncode != 0:
        return None
    return proc.stdout or ""


def _dump_ssh(spec: str) -> list:
    try:
        proc = subprocess.run(
            ["ssh", "-T", "-o", "BatchMode=yes", "-o", "ConnectTimeout=5", spec, "python3", "-"],
            input=_REMOTE_DUMP,
            capture_output=True,
            text=True,
            timeout=12,
        )
    except (OSError, subprocess.TimeoutExpired):
        return []
    if proc.returncode != 0:
        return []
    raw = proc.stdout or ""
    start = raw.find("BEGINJSON")
    end = raw.rfind("ENDJSON")
    if start < 0 or end < start:
        return []
    body = raw[start + len("BEGINJSON"):end].strip()
    try:
        data = json.loads(body)
    except json.JSONDecodeError:
        return []
    return data if isinstance(data, list) else []


def _find_ssh(instance_id: str, spec: str) -> dict | None:
    if not spec:
        return None
    service, index = _split_id(instance_id)
    best = None
    best_score = 0
    for spec in (spec,):
        for pane in _dump_ssh(spec):
            if not isinstance(pane, dict):
                continue
            target = str(pane.get("target") or "")
            if not PANE_TARGET.fullmatch(target):
                continue
            cmd = str(pane.get("cmd") or "")
            text = str(pane.get("text") or "")
            window = str(pane.get("window") or "")
            score = 0
            match = ""
            if _has_id(cmd, instance_id):
                score = 20
                match = "process"
            else:
                own = len(re.findall(r"id = " + re.escape(instance_id) + r"(?![\w.-])", text))
                if own:
                    score = 15 + min(own, 4)
                    match = "log"
                elif _has_id(text, instance_id):
                    score = 4
                    match = "mention"
            if score == 0 and service and _has_id(cmd, service) and window.endswith(index):
                score = 3
                match = "window"
            if score <= best_score:
                continue
            best_score = score
            best = {
                "target": target,
                "session": str(pane.get("session") or ""),
                "window": window,
                "ssh": spec,
                "match": match,
                "text": text,
                "score": score,
                "weak": score < 10,
            }
    if best_score < 3:
        return None
    return best


def _tree_cmd(pid: int) -> str:
    if pid <= 0:
        return ""
    chunks = []
    stack = [pid]
    seen = set()
    while stack and len(seen) < 40:
        cur = stack.pop()
        if cur in seen:
            continue
        seen.add(cur)
        chunks.append(_cmdline(cur))
        stack.extend(_children(cur))
    return "\n".join(chunks)


def _cmdline(pid: int) -> str:
    try:
        raw = Path(f"/proc/{pid}/cmdline").read_bytes()
    except OSError:
        return ""
    return raw.replace(b"\0", b" ").decode("utf-8", "replace")


def _children(pid: int) -> list:
    out = []
    proc = Path("/proc")
    try:
        names = list(proc.iterdir())
    except OSError:
        return out
    want = str(pid)
    for entry in names:
        if not entry.name.isdigit():
            continue
        try:
            stat = (entry / "stat").read_text()
        except OSError:
            continue
        cut = stat.rfind(")")
        if cut < 0:
            continue
        fields = stat[cut + 2 :].split()
        if len(fields) > 1 and fields[1] == want:
            out.append(int(entry.name))
    return out
