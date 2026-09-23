"""List / run / generate topology and run shell scripts under the repo."""

from __future__ import annotations

import os
import re
import subprocess
from datetime import datetime
from pathlib import Path
from typing import Any, Dict, List, Optional

HERE = Path(__file__).resolve().parent
REPO = HERE.parent
DEFAULT_SCRIPTS_DIR = "run"
PROTECTED = {
    "start_device.sh",
    "device_launch.sh",
    "init.sh",
    "start_from_edit.sh",
    "redis-server.txt",
}
SAFE_NAME = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]*$")
SAFE_IDENT = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")


def resolve_scripts_dir(rel: Optional[str]) -> Path:
    raw = (rel or DEFAULT_SCRIPTS_DIR).strip() or DEFAULT_SCRIPTS_DIR
    raw = raw.replace("\\", "/")
    if raw.startswith("/"):
        path = Path(raw).resolve()
    else:
        path = (REPO / raw).resolve()
    root = REPO.resolve()
    try:
        path.relative_to(root)
    except ValueError as exc:
        raise ValueError("directory must be under the repo") from exc
    if not path.is_dir():
        raise ValueError(f"not a directory: {raw}")
    return path


def rel_dir(path: Path) -> str:
    return str(path.relative_to(REPO.resolve()))


def classify(name: str) -> str:
    n = name.lower()
    if n.startswith("topology") or n.startswith("topo-"):
        return "topology"
    if n.startswith("param") or n.startswith("mq-param"):
        return "param"
    if n.startswith("run-") or n.startswith("run_"):
        return "run"
    return "other"


def list_script_dirs() -> List[str]:
    out = []
    root = REPO.resolve()
    for child in sorted(root.iterdir(), key=lambda p: p.name.lower()):
        if not child.is_dir() or child.name.startswith("."):
            continue
        if any(p.suffix == ".sh" and p.is_file() for p in child.iterdir()):
            out.append(child.name)
    if DEFAULT_SCRIPTS_DIR not in out and (root / DEFAULT_SCRIPTS_DIR).is_dir():
        out.insert(0, DEFAULT_SCRIPTS_DIR)
    return out


def list_scripts(rel: Optional[str]) -> Dict[str, Any]:
    path = resolve_scripts_dir(rel)
    rows = []
    for p in sorted(path.iterdir(), key=lambda x: x.name.lower()):
        if not p.is_file() or p.suffix != ".sh":
            continue
        rows.append({
            "name": p.name,
            "kind": classify(p.name),
            "size": p.stat().st_size,
        })
    groups = {"topology": [], "param": [], "run": [], "other": []}
    for row in rows:
        groups[row["kind"]].append(row)
    return {
        "ok": True,
        "dir": rel_dir(path),
        "default": DEFAULT_SCRIPTS_DIR,
        "dirs": list_script_dirs(),
        "scripts": rows,
        "groups": groups,
    }


def read_script(rel: Optional[str], name: str) -> Dict[str, Any]:
    path = _script_path(rel, name)
    text = path.read_text(encoding="utf-8", errors="replace")
    if len(text) > 200_000:
        text = text[:200_000] + "\n... truncated ...\n"
    return {
        "ok": True,
        "dir": rel_dir(path.parent),
        "name": path.name,
        "kind": classify(path.name),
        "text": text,
    }


def run_script(rel: Optional[str], name: str, timeout: int = 180) -> Dict[str, Any]:
    path = _script_path(rel, name)
    env = os.environ.copy()
    if not env.get("DISPLAY"):
        env["DISPLAY"] = "localhost:11.0"
    log = path.parent / f"_{path.stem}.last.log"
    try:
        proc = subprocess.run(
            ["bash", str(path)],
            cwd=str(path.parent),
            env=env,
            capture_output=True,
            text=True,
            timeout=max(5, min(int(timeout), 300)),
        )
    except subprocess.TimeoutExpired as exc:
        out = (exc.stdout or "") + (exc.stderr or "")
        log.write_text(out, encoding="utf-8")
        return {
            "ok": False,
            "dir": rel_dir(path.parent),
            "name": path.name,
            "error": f"timeout after {timeout}s",
            "output": out[-20000:],
            "log": log.name,
        }
    out = (proc.stdout or "") + (proc.stderr or "")
    log.write_text(out, encoding="utf-8")
    return {
        "ok": proc.returncode == 0,
        "dir": rel_dir(path.parent),
        "name": path.name,
        "exit": proc.returncode,
        "output": out[-20000:],
        "log": log.name,
    }


def write_edit_scripts(
    rel: Optional[str],
    stem: str,
    endpoints: List[Dict[str, Any]],
    links: List[Dict[str, str]],
    services: List[Dict[str, Any]],
) -> Dict[str, Any]:
    path = resolve_scripts_dir(rel)
    stem = _unique_stem(path, _safe_stem(stem))
    topo_name = f"topology-{stem}.sh"
    param_name = f"param-{stem}.sh"
    run_name = f"run-{stem}.sh"
    for name in (topo_name, param_name, run_name):
        if name in PROTECTED:
            raise ValueError(f"refusing to overwrite {name}")
    eps = _clean_endpoints(endpoints)
    lks = _clean_links(links)
    svcs = _clean_services(services)
    if not eps:
        raise ValueError("no endpoints to write")
    topo_text = _render_topology(stem, eps, lks, svcs)
    param_text = _render_param(stem, svcs)
    run_text = _render_run(stem, svcs)
    for name, text in ((topo_name, topo_text), (param_name, param_text), (run_name, run_text)):
        (path / name).write_text(text, encoding="utf-8")
        os.chmod(path / name, 0o755)
    return {
        "ok": True,
        "dir": rel_dir(path),
        "stem": stem,
        "files": [topo_name, param_name, run_name],
    }


def _script_path(rel: Optional[str], name: str) -> Path:
    name = (name or "").strip()
    if not SAFE_NAME.fullmatch(name) or not name.endswith(".sh"):
        raise ValueError("invalid script name")
    folder = resolve_scripts_dir(rel)
    path = (folder / name).resolve()
    try:
        path.relative_to(folder)
    except ValueError as exc:
        raise ValueError("script must be inside the selected directory") from exc
    if not path.is_file():
        raise ValueError(f"no such script: {name}")
    return path


def _safe_stem(stem: str) -> str:
    stem = (stem or "from-edit").strip()
    if not SAFE_NAME.fullmatch(stem):
        raise ValueError("stem must be letters, digits, . _ -")
    if len(stem) > 48:
        raise ValueError("stem too long")
    return stem


def _unique_stem(folder: Path, stem: str) -> str:
    stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    base = f"{stem}-{stamp}"
    candidate = base
    n = 2
    while (folder / f"topology-{candidate}.sh").exists() or (folder / f"run-{candidate}.sh").exists():
        candidate = f"{base}-{n}"
        n += 1
    return candidate


def _require_ident(value: str, kind: str) -> str:
    value = (value or "").strip()
    if not SAFE_IDENT.fullmatch(value):
        raise ValueError(f"unsafe {kind}: {value!r}")
    return value


def _clean_endpoints(rows: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
    out = []
    for ep in rows or []:
        svc = _require_ident(str(ep.get("service") or ""), "service")
        ch = _require_ident(str(ep.get("channel") or ""), "channel")
        typ = str(ep.get("type") or "push").strip().lower()
        method = str(ep.get("method") or "bind").strip().lower()
        if typ not in {"push", "pull", "pub", "sub"}:
            raise ValueError(f"bad type: {typ}")
        if method not in {"bind", "connect"}:
            raise ValueError(f"bad method: {method}")
        auto = ep.get("autoSubChannel")
        if isinstance(auto, str):
            auto = auto.strip().lower() in {"1", "true", "yes", "on"}
        out.append({
            "service": svc,
            "channel": ch,
            "type": typ,
            "method": method,
            "autoSubChannel": bool(auto),
        })
    return out


def _clean_links(rows: List[Dict[str, str]]) -> List[Dict[str, str]]:
    out = []
    for lk in rows or []:
        a = _require_ident(str(lk.get("from_service") or ""), "service")
        ac = _require_ident(str(lk.get("from_channel") or ""), "channel")
        b = _require_ident(str(lk.get("to_service") or ""), "service")
        bc = _require_ident(str(lk.get("to_channel") or ""), "channel")
        out.append({
            "from_service": a,
            "from_channel": ac,
            "to_service": b,
            "to_channel": bc,
        })
    return out


def _clean_services(rows: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
    out = []
    for svc in rows or []:
        name = _require_ident(str(svc.get("name") or ""), "service")
        binary = _require_ident(str(svc.get("binary") or name), "binary")
        try:
            n = int(svc.get("n") or 0)
        except (TypeError, ValueError):
            n = 0
        n = max(0, min(n, 200))
        if n < 1:
            continue
        out.append({"name": name, "binary": binary, "n": n})
    return out


def _render_topology(
    stem: str,
    endpoints: List[Dict[str, Any]],
    links: List[Dict[str, str]],
    services: List[Dict[str, Any]],
) -> str:
    lines = [
        "#!/bin/bash",
        f"# Generated from the edit page (stem={stem})",
        "# Writes Redis DB0 topology:endpoint / link / count.",
        "",
        'cd "$(dirname "$0")"',
        'export PATH="${NESTDAQ:-$HOME/nestdaq}/bin:$PATH"',
        "",
        "redis_server=`cat redis-server.txt | sed 's/[[:blank:]]//g'`",
        "server=redis://$redis_server/0",
        "",
        "if ! redis-cli -u redis://$redis_server ping >/dev/null 2>&1; then",
        '  echo "ERROR: Redis is not running at $redis_server (run ./init.sh)" >&2',
        "  exit 1",
        "fi",
        "set -e",
        "",
        "function endpoint () {",
        "  echo redis-cli -u $server hset daq_service:topology:endpoint:$1:$2 ${@:3}",
        "  redis-cli -u $server hset daq_service:topology:endpoint:$1:$2 ${@:3} || return 1",
        "}",
        "",
        "function link () {",
        "  echo redis-cli -u $server set daq_service:topology:link:$1:$2,$3:$4 none",
        "  redis-cli -u $server set daq_service:topology:link:$1:$2,$3:$4 none || return 1",
        "}",
        "",
        'echo "Clear DAQ service keys"',
        "keys=`redis-cli -u $server keys 'daq_service:*'`",
        'if [ -n "$keys" ]; then',
        '  echo "$keys" | xargs redis-cli -u $server del',
        "fi",
        "",
        'echo "---------------------------------------------------------------------"',
        'echo " config endpoint (socket)"',
        'echo "---------------------------------------------------------------------"',
        "",
    ]
    for ep in endpoints:
        auto = "true" if ep["autoSubChannel"] else "false"
        lines.append(
            f"endpoint     {ep['service']:<22} {ep['channel']:<12}"
            f" type {ep['type']:<5} method {ep['method']:<8} autoSubChannel {auto}"
        )
    lines += [
        "",
        'echo "---------------------------------------------------------------------"',
        'echo " config link"',
        'echo "---------------------------------------------------------------------"',
        "",
    ]
    for lk in links:
        lines.append(
            f"link    {lk['from_service']:<22} {lk['from_channel']:<18}"
            f" {lk['to_service']:<18} {lk['to_channel']}"
        )
    if services:
        lines += [
            "",
            'echo "---------------------------------------------------------------------"',
            'echo " config count"',
            'echo "---------------------------------------------------------------------"',
            "",
        ]
        for svc in services:
            lines.append(
                f'echo redis-cli -u $server set daq_service:topology:count:{svc["name"]} {svc["n"]}'
            )
            lines.append(
                f'redis-cli -u $server set daq_service:topology:count:{svc["name"]} {svc["n"]}'
            )
    lines.append("")
    return "\n".join(lines)


def _render_param(stem: str, services: List[Dict[str, Any]]) -> str:
    lines = [
        "#!/bin/bash",
        f"# Generated from the edit page (stem={stem})",
        "# Writes Redis DB2 parameters. AmQStrTdcSampler INIT TASK needs msiTcpIp + TdcType.",
        "",
        'cd "$(dirname "$0")"',
        'export PATH="${NESTDAQ:-$HOME/nestdaq}/bin:$PATH"',
        "",
        "redis_server=`cat redis-server.txt | sed 's/[[:blank:]]//g'`",
        "server=redis://$redis_server/2",
        "",
        "if ! redis-cli -u redis://$redis_server ping >/dev/null 2>&1; then",
        '  echo "ERROR: Redis is not running at $redis_server (run ./init.sh)" >&2',
        "  exit 1",
        "fi",
        "set -e",
        "",
        "SAMPLER_IP_BASE=${SAMPLER_IP_BASE:-192.168.2}",
        "SAMPLER_IP_START=${SAMPLER_IP_START:-101}",
        "TDC_TYPE=${TDC_TYPE:-5}",
        'TRIG_SIGNALS=${TRIG_SIGNALS:-\'(0xc0a802a9 0 0) (0xc0a802a9 1 0) (0xc0a802a9 2 0) (0xc0a802a9 3 0)\'}',
        "TRIG_EXPR=${TRIG_EXPR:-'RPN 0 1 & 2 & 3 &'}",
        "TRIG_WIDTH=${TRIG_WIDTH:-10}",
        "",
        "function param () {",
        "  echo redis-cli -u $server hset parameters:$1 ${@:2}",
        "  redis-cli -u $server hset parameters:$1 ${@:2} || return 1",
        "}",
        "",
        "redis-cli -u $server flushdb || exit 1",
        "",
    ]
    for svc in services:
        name = svc["name"]
        binary = svc["binary"]
        n = svc["n"]
        if name in {"AmQStrTdcSampler", "Sampler"} or binary in {"AmQStrTdcSampler", "Sampler"}:
            lines += [
                f"i=0",
                f'while [ "$i" -lt {n} ]; do',
                f'  ip="${{SAMPLER_IP_BASE}}.$((SAMPLER_IP_START + i))"',
                f'  param {name}-$i msiTcpIp "$ip" TdcType "$TDC_TYPE"',
                "  i=$((i + 1))",
                "done",
                "",
            ]
        elif name == "STFBuilder" or binary == "STFBuilder":
            lines += [
                f"i=0",
                f'while [ "$i" -lt {n} ]; do',
                f"  param {name}-$i max-hbf 4",
                "  i=$((i + 1))",
                "done",
                "",
            ]
        elif name == "TimeFrameBuilder" or binary == "TimeFrameBuilder":
            lines += [
                f"i=0",
                f'while [ "$i" -lt {n} ]; do',
                f"  param {name}-$i decimation-factor 1000 decimation-offset 0",
                "  i=$((i + 1))",
                "done",
                "",
            ]
        elif binary == "FileSink" or name in {"FileSink", "DecSink"}:
            sub = "dcmdata" if name == "DecSink" else "tdcdata"
            lines += [
                f"i=0",
                f'while [ "$i" -lt {n} ]; do',
                f'  dir=$(printf "%s/data/{sub}/%02d" "$PWD" "$i")',
                '  mkdir -p "$dir"',
                f'  param {name}-$i multipart true prefix "$dir" ext .dat openmode recreate',
                "  i=$((i + 1))",
                "done",
                "",
            ]
        elif name == "TimeFrameSlicerByLogicTiming" or binary == "TimeFrameSlicerByLogicTiming":
            lines += [
                f"i=0",
                f'while [ "$i" -lt {n} ]; do',
                f"  param {name}-$i time-offset-begin -250 time-offset-end 250",
                "  i=$((i + 1))",
                "done",
                "",
            ]
        elif binary in {"LogicFilter", "LogicFilterGpu"} or name in {"LogicFilter", "LogicFilterGpu"}:
            lines += [
                f"param {name} data-suppress true remove-hb false split 2 poll-timeout 1",
                f"i=0",
                f'while [ "$i" -lt {n} ]; do',
                f"  param {name}-$i data-suppress true remove-hb false split 2 poll-timeout 1",
                "  i=$((i + 1))",
                "done",
                "for key in "
                + " ".join([name] + [f"{name}-{i}" for i in range(n)])
                + "; do",
                '  echo redis-cli -u $server hset parameters:$key trigger-signals "${TRIG_SIGNALS}"',
                '  redis-cli -u $server hset parameters:$key trigger-signals "${TRIG_SIGNALS}"',
                '  echo redis-cli -u $server hset parameters:$key trigger-expression "${TRIG_EXPR}"',
                '  redis-cli -u $server hset parameters:$key trigger-expression "${TRIG_EXPR}"',
                '  echo redis-cli -u $server hset parameters:$key trigger-width "${TRIG_WIDTH}"',
                '  redis-cli -u $server hset parameters:$key trigger-width "${TRIG_WIDTH}"',
                "done",
                "",
            ]
        elif binary == "TFBFilePlayer" or name == "TFBFilePlayer":
            lines += [
                "TFB_INFILE=${TFB_INFILE:-$PWD/data/input/tfb.dat}",
                f"i=0",
                f'while [ "$i" -lt {n} ]; do',
                f'  param {name}-$i in-file "${{TFB_INFILE}}" wait 0 max-iterations 0 split 2 recbe false',
                "  i=$((i + 1))",
                "done",
                "",
            ]
    lines.append(f'echo "parameters registered for stem={stem}"')
    lines.append("")
    return "\n".join(lines)


def _render_run(stem: str, services: List[Dict[str, Any]]) -> str:
    lines = [
        "#!/bin/bash",
        f"# Generated from the edit page (stem={stem})",
        f"# Runs param-{stem}.sh, topology-{stem}.sh, then start_device.",
        "",
        "set -e",
        'cd "$(dirname "$0")"',
        "",
        "if [ -f ./device_launch.sh ]; then",
        "  . ./device_launch.sh",
        'elif [ -f "$(dirname "$0")/../run/device_launch.sh" ]; then',
        '  . "$(dirname "$0")/../run/device_launch.sh"',
        "else",
        '  echo "ERROR: device_launch.sh not found" >&2',
        "  exit 1",
        "fi",
        "",
        "redis_server=`cat redis-server.txt | sed 's/[[:blank:]]//g'`",
        "if ! redis-cli -u redis://$redis_server ping >/dev/null 2>&1; then",
        "  if [ -x ./init.sh ]; then",
        "    ./init.sh",
        "  else",
        '    echo "ERROR: Redis is not running at $redis_server" >&2',
        "    exit 1",
        "  fi",
        "fi",
        "",
        f'if [ -x "./param-{stem}.sh" ]; then',
        f'  echo "register parameters  ./param-{stem}.sh"',
        f"  ./param-{stem}.sh",
        "else",
        f'  echo "ERROR: ./param-{stem}.sh is missing (Sampler INIT TASK needs msiTcpIp/TdcType)" >&2',
        "  exit 1",
        "fi",
        "",
        f'if [ -x "./topology-{stem}.sh" ]; then',
        f'  echo "register topology  ./topology-{stem}.sh"',
        f"  ./topology-{stem}.sh",
        "else",
        f'  echo "ERROR: ./topology-{stem}.sh is missing" >&2',
        "  exit 1",
        "fi",
        "",
        'if [ -z "$DISPLAY" ]; then',
        '  echo "WARNING: DISPLAY is empty; tmux starts but xterm will not open" >&2',
        "fi",
        "",
    ]
    used = set()
    for svc in services:
        session = re.sub(r"[^A-Za-z0-9]", "", svc["name"]).lower()[:16] or "dev"
        base = session
        n = 2
        while session in used:
            session = f"{base}{n}"
            n += 1
        used.add(session)
        wprefix = svc["name"][:4]
        extra = ""
        if svc["name"] != svc["binary"]:
            extra = f' --service-name {svc["name"]}'
        lines.append(
            f'start_group {session} {svc["name"]} {svc["n"]} {wprefix} '
            f'{svc["binary"]}{extra}'
        )
    lines += [
        "",
        f'echo "started from run-{stem}.sh"',
        'echo "next/prev window: Ctrl-b n / Ctrl-b p"',
        "",
    ]
    return "\n".join(lines)
