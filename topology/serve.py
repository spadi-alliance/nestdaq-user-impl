#!/usr/bin/env python3
"""Live topology viewer. Reads Redis the same way daq-webctl does.

  python3 topology/serve.py
  python3 topology/serve.py --redis redis://127.0.0.1:6379 --port 8765
  python3 topology/serve.py --dump
"""

from __future__ import annotations

import argparse
import json
import os
import select
import socket
import subprocess
import sys
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse

HERE = Path(__file__).resolve().parent
STATIC = HERE / "static"
BIN = HERE.parent / "bin"
BIN_SKIP = {"sqlite3a", "tfdump", "fltcoin"}
DEFAULT_ENDPOINTS = {
    "AmQStrTdcSampler": [
        {"channel": "out", "type": "push", "method": "bind", "autoSubChannel": False},
    ],
    "Sampler": [
        {"channel": "out", "type": "push", "method": "bind", "autoSubChannel": False},
    ],
    "STFBuilder": [
        {"channel": "in", "type": "pull", "method": "connect", "autoSubChannel": False},
        {"channel": "out", "type": "push", "method": "connect", "autoSubChannel": True},
    ],
    "TimeFrameBuilder": [
        {"channel": "in", "type": "pull", "method": "bind", "autoSubChannel": False},
        {"channel": "out", "type": "push", "method": "connect", "autoSubChannel": True},
        {"channel": "decimator", "type": "push", "method": "connect", "autoSubChannel": False},
    ],
    "LogicFilter": [
        {"channel": "in", "type": "pull", "method": "bind", "autoSubChannel": False},
        {"channel": "out", "type": "push", "method": "connect", "autoSubChannel": True},
    ],
    "LogicFilterGpu": [
        {"channel": "in", "type": "pull", "method": "bind", "autoSubChannel": False},
        {"channel": "out", "type": "push", "method": "connect", "autoSubChannel": True},
    ],
    "FileSink": [
        {"channel": "in", "type": "pull", "method": "bind", "autoSubChannel": False},
    ],
    "Sink": [
        {"channel": "in", "type": "pull", "method": "bind", "autoSubChannel": False},
    ],
    "TFBFilePlayer": [
        {"channel": "out", "type": "push", "method": "bind", "autoSubChannel": True},
    ],
    "STFBFilePlayer": [
        {"channel": "out", "type": "push", "method": "bind", "autoSubChannel": True},
    ],
    "Scaler": [
        {"channel": "in", "type": "pull", "method": "connect", "autoSubChannel": False},
    ],
    "TimeFrameSlicerByLogicTiming": [
        {"channel": "in", "type": "pull", "method": "bind", "autoSubChannel": False},
        {"channel": "out", "type": "push", "method": "connect", "autoSubChannel": True},
    ],
}


def list_binaries() -> list:
    rows = []
    if BIN.is_dir():
        for p in sorted(BIN.iterdir(), key=lambda x: x.name.lower()):
            if not p.is_file() or p.name in BIN_SKIP:
                continue
            if not os.access(p, os.X_OK):
                continue
            rows.append({
                "name": p.name,
                "path": str(p),
                "defaults": DEFAULT_ENDPOINTS.get(p.name, [
                    {"channel": "in", "type": "pull", "method": "bind", "autoSubChannel": False},
                    {"channel": "out", "type": "push", "method": "connect", "autoSubChannel": False},
                ]),
            })
    return rows
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))

from redis_live import (  # noqa: E402
    RedisTopology,
    daq_incr_run,
    daq_publish,
    daq_set_run_info,
    daq_status_from,
    empty_params_snapshot,
    empty_snapshot,
)
from script_io import (  # noqa: E402
    DEFAULT_SCRIPTS_DIR,
    list_scripts,
    browse_script_dir,
    read_script,
    run_script,
    with_xterm_display,
    write_edit_scripts,
)
from tmux_log import TmuxLog  # noqa: E402
from tmux_view import capture_instance  # noqa: E402

XTERM_MODE_FILE = HERE / "xterm-mode"


def xterm_mode() -> str:
    try:
        raw = XTERM_MODE_FILE.read_text(encoding="utf-8").strip().lower()
    except OSError:
        return "xterm"
    if raw in {"viewer", "topology", "0", "off", "no"}:
        return "viewer"
    return "xterm"


def set_xterm_mode(mode: str) -> dict:
    chosen = "viewer" if str(mode).strip().lower() in {"viewer", "topology", "0", "off", "no"} else "xterm"
    XTERM_MODE_FILE.write_text(chosen + "\n", encoding="utf-8")
    return {"ok": True, "mode": chosen}


def _tcp_up(host: str, port: int) -> bool:
    sock = socket.socket()
    sock.settimeout(0.3)
    try:
        sock.connect((host, port))
        return True
    except OSError:
        return False
    finally:
        sock.close()


def default_uri() -> str:
    for cand in (
        HERE / "run" / "redis-server.txt",
        HERE.parent / "run" / "redis-server.txt",
    ):
        if cand.is_file():
            host = cand.read_text().strip().split()[0]
            if host:
                return f"redis://{host}"
    return "redis://127.0.0.1:6379"


class App:
    def __init__(self, uri: str, ctl_host: str = "127.0.0.1", ctl_port: int = 8080):
        self.uri = uri
        self.ctl_host = ctl_host
        self.ctl_port = ctl_port
        self.rt = None
        self.tmux_log = TmuxLog(self)
        try:
            self.rt = RedisTopology(uri)
        except Exception as exc:  # noqa: BLE001
            print(f"Redis client init failed: {exc}", file=sys.stderr)

    def snapshot(self) -> dict:
        if self.rt is None:
            return empty_snapshot(self.uri, ["Redis client unavailable"])
        try:
            self.rt.ping()
        except Exception as exc:  # noqa: BLE001
            return empty_snapshot(self.uri, [f"Redis ping failed ({exc})"])
        return self.rt.snapshot()

    def params(self) -> dict:
        if self.rt is None:
            return empty_params_snapshot(self.uri, ["Redis client unavailable"])
        try:
            self.rt.ping()
        except Exception as exc:  # noqa: BLE001
            return empty_params_snapshot(self.uri, [f"Redis ping failed ({exc})"])
        return self.rt.params_snapshot()

    def daq_status(self) -> dict:
        if self.rt is None:
            return {"ok": False, "error": "Redis client unavailable"}
        try:
            self.rt.ping()
            return daq_status_from(self.rt)
        except Exception as exc:  # noqa: BLE001
            return {"ok": False, "error": str(exc)}


def make_handler(app: App):
    class Handler(BaseHTTPRequestHandler):
        def log_message(self, fmt, *args):
            sys.stderr.write("%s - %s\n" % (self.address_string(), fmt % args))

        def _send(self, code: int, body: bytes, ctype: str, extra=None):
            self.send_response(code)
            self.send_header("Content-Type", ctype)
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-store")
            if extra:
                for k, v in extra.items():
                    self.send_header(k, v)
            self.end_headers()
            self.wfile.write(body)

        def do_GET(self):
            path = urlparse(self.path).path
            if path in ("/", "/index.html", "/params", "/daq", "/edit", "/scripts"):
                return self._file(STATIC / "index.html", "text/html; charset=utf-8")
            if path == "/viewer.js":
                return self._file(STATIC / "viewer.js", "application/javascript")
            if path == "/editor.js":
                return self._file(STATIC / "editor.js", "application/javascript")
            if path == "/scripts.js":
                return self._file(STATIC / "scripts.js", "application/javascript")
            if path == "/ctl.js":
                return self._file(STATIC / "ctl.js", "application/javascript")
            if path == "/viewer.css":
                return self._file(STATIC / "viewer.css", "text/css")
            if path == "/slides-viewer.tex":
                return self._file(
                    HERE / "docs" / "slides-viewer.tex",
                    "application/x-tex; charset=utf-8",
                    extra={"Content-Disposition": 'attachment; filename="slides-viewer.tex"'},
                )
            if path == "/api/snapshot":
                body = json.dumps(app.snapshot()).encode("utf-8")
                return self._send(200, body, "application/json")
            if path == "/api/params":
                body = json.dumps(app.params()).encode("utf-8")
                return self._send(200, body, "application/json")
            if path == "/api/binaries":
                return self._send(200, json.dumps({"ok": True, "binaries": list_binaries()}).encode(), "application/json")
            if path == "/api/topology":
                snap = app.snapshot()
                counts = {}
                if app.rt is not None:
                    try:
                        counts = app.rt.read_counts()
                    except Exception:
                        counts = {}
                return self._send(200, json.dumps({
                    "ok": snap.get("ok", False),
                    "endpoints": snap.get("endpoints") or [],
                    "links": snap.get("links") or [],
                    "counts": counts,
                }).encode(), "application/json")
            if path == "/api/scripts/dirs":
                q = parse_qs(urlparse(self.path).query)
                d = (q.get("dir") or [DEFAULT_SCRIPTS_DIR])[0]
                try:
                    return self._send(200, json.dumps(browse_script_dir(d)).encode(), "application/json")
                except Exception as exc:  # noqa: BLE001
                    return self._send(400, json.dumps({"ok": False, "error": str(exc)}).encode(), "application/json")
            if path == "/api/scripts":
                q = parse_qs(urlparse(self.path).query)
                d = (q.get("dir") or [DEFAULT_SCRIPTS_DIR])[0]
                try:
                    return self._send(200, json.dumps(list_scripts(d)).encode(), "application/json")
                except Exception as exc:  # noqa: BLE001
                    return self._send(400, json.dumps({"ok": False, "error": str(exc)}).encode(), "application/json")
            if path == "/api/scripts/text":
                q = parse_qs(urlparse(self.path).query)
                d = (q.get("dir") or [DEFAULT_SCRIPTS_DIR])[0]
                name = (q.get("name") or [""])[0]
                try:
                    return self._send(200, json.dumps(read_script(d, name)).encode(), "application/json")
                except Exception as exc:  # noqa: BLE001
                    return self._send(400, json.dumps({"ok": False, "error": str(exc)}).encode(), "application/json")
            if path == "/api/xterm-mode":
                return self._send(200, json.dumps({"ok": True, "mode": xterm_mode()}).encode(), "application/json")
            if path == "/api/tmux/log":
                return self._send(200, json.dumps(app.tmux_log.status()).encode(), "application/json")
            if path == "/api/tmux":
                q = parse_qs(urlparse(self.path).query)
                inst = (q.get("id") or [""])[0]
                host, ip, account = "", "", ""
                if app.rt is not None:
                    try:
                        loc = app.rt.instance_location(inst)
                        host, ip = loc.get("host") or "", loc.get("ip") or ""
                        account = app.rt.instance_account(inst)
                    except Exception:
                        host, ip, account = "", "", ""
                try:
                    return self._send(200, json.dumps(capture_instance(inst, host, ip, account)).encode(), "application/json")
                except Exception as exc:  # noqa: BLE001
                    return self._send(400, json.dumps({"ok": False, "error": str(exc)}).encode(), "application/json")
            if path == "/api/daq/status":
                return self._send(200, json.dumps(app.daq_status()).encode("utf-8"), "application/json")
            if path == "/api/daq-status":
                return self._send(200, json.dumps(app.daq_status()).encode("utf-8"), "application/json")
            if path == "/api/daq-ws":
                return self._tunnel_ws(app.ctl_host, app.ctl_port)
            self._send(404, b"not found", "text/plain")

        def do_POST(self):
            path = urlparse(self.path).path
            try:
                payload = self._read_json()
            except Exception as exc:  # noqa: BLE001
                return self._send(400, json.dumps({"ok": False, "error": str(exc)}).encode(), "application/json")
            if path == "/api/xterm-mode":
                return self._send(200, json.dumps(set_xterm_mode(payload.get("mode") or "")).encode(), "application/json")
            if path == "/api/tmux/log":
                enabled = bool(payload.get("enabled"))
                return self._send(200, json.dumps(app.tmux_log.set_enabled(enabled)).encode(), "application/json")
            if path == "/api/scripts/run":
                try:
                    out = run_script(
                        payload.get("dir") or DEFAULT_SCRIPTS_DIR,
                        payload.get("name") or "",
                    )
                    return self._send(200, json.dumps(out).encode(), "application/json")
                except Exception as exc:  # noqa: BLE001
                    return self._send(400, json.dumps({"ok": False, "error": str(exc)}).encode(), "application/json")
            if path == "/api/scripts/write":
                try:
                    out = write_edit_scripts(
                        payload.get("dir") or DEFAULT_SCRIPTS_DIR,
                        payload.get("stem") or "from-edit",
                        payload.get("endpoints") or [],
                        payload.get("links") or [],
                        payload.get("services") or [],
                    )
                    return self._send(200, json.dumps(out).encode(), "application/json")
                except Exception as exc:  # noqa: BLE001
                    return self._send(400, json.dumps({"ok": False, "error": str(exc)}).encode(), "application/json")
            if app.rt is None:
                return self._send(503, json.dumps({"ok": False, "error": "Redis client unavailable"}).encode(), "application/json")
            try:
                if path == "/api/daq/command":
                    out = daq_publish(
                        app.rt,
                        payload.get("value") or payload.get("command") or "",
                        list(payload.get("services") or ["all"]),
                        list(payload.get("instances") or ["all"]),
                    )
                    return self._send(200, json.dumps(out).encode(), "application/json")
                if path == "/api/daq/run":
                    action = payload.get("action") or "set"
                    if action == "incr":
                        val = daq_incr_run(app.rt)
                    else:
                        val = daq_set_run_info(app.rt, "run_number", str(payload.get("value") or ""))
                    return self._send(200, json.dumps({"ok": True, "run_number": val}).encode(), "application/json")
                if path == "/api/daq/flag":
                    name = payload.get("name") or ""
                    val = daq_set_run_info(app.rt, name, str(payload.get("value") or "").lower())
                    return self._send(200, json.dumps({"ok": True, "name": name, "value": val}).encode(), "application/json")
                if path == "/api/topology":
                    out = app.rt.write_topology(
                        payload.get("endpoints") or [],
                        payload.get("links") or [],
                        payload.get("services") or [],
                    )
                    return self._send(200, json.dumps(out).encode(), "application/json")
                if path == "/api/redis-clear":
                    out = app.rt.clear_except_run()
                    return self._send(200, json.dumps(out).encode(), "application/json")
                if path == "/api/run-start":
                    services = payload.get("services") or []
                    app.rt.write_topology(
                        payload.get("endpoints") or [],
                        payload.get("links") or [],
                        services,
                    )
                    run_dir = HERE / "run"
                    n_params = app.rt.write_run_params(services, str(run_dir))
                    json_path = run_dir / "_edit_run.json"
                    json_path.write_text(json.dumps({"services": services}), encoding="utf-8")
                    env = with_xterm_display(os.environ.copy())
                    log = run_dir / "_edit_run.log"
                    with log.open("ab") as fh:
                        subprocess.Popen(
                            [str(run_dir / "start_from_edit.sh"), str(json_path)],
                            cwd=str(run_dir),
                            env=env,
                            stdout=fh,
                            stderr=subprocess.STDOUT,
                            start_new_session=True,
                        )
                    n = sum(max(0, int(s.get("n") or 0)) for s in services if s.get("name"))
                    return self._send(200, json.dumps({
                        "ok": True,
                        "started": n,
                        "params": n_params,
                    }).encode(), "application/json")
            except Exception as exc:  # noqa: BLE001
                return self._send(400, json.dumps({"ok": False, "error": str(exc)}).encode(), "application/json")
            self._send(404, b"not found", "text/plain")

        def _read_json(self):
            n = int(self.headers.get("Content-Length") or 0)
            raw = self.rfile.read(n) if n else b"{}"
            return json.loads(raw.decode("utf-8") or "{}")

        def _tunnel_ws(self, host: str, port: int):
            try:
                dest = socket.create_connection((host, port), timeout=3)
            except OSError:
                return self._send(
                    502,
                    b"daq-webctl is not running",
                    "text/plain",
                )
            lines = ["GET /daq-webctl HTTP/1.1"]
            for key, val in self.headers.items():
                if key.lower() == "host":
                    val = f"{host}:{port}"
                lines.append(f"{key}: {val}")
            dest.sendall(("\r\n".join(lines) + "\r\n\r\n").encode("iso-8859-1"))
            client = self.connection
            self.close_connection = True
            try:
                while True:
                    ready, _, _ = select.select([client, dest], [], [], 60)
                    if not ready:
                        continue
                    for src, dst in ((client, dest), (dest, client)):
                        if src not in ready:
                            continue
                        chunk = src.recv(16384)
                        if not chunk:
                            return
                        dst.sendall(chunk)
            except OSError:
                return
            finally:
                try:
                    dest.close()
                except OSError:
                    pass

        def _file(self, path: Path, ctype: str, extra=None):
            if not path.is_file():
                return self._send(404, b"missing static file", "text/plain")
            return self._send(200, path.read_bytes(), ctype, extra=extra)

    return Handler


def main(argv=None) -> int:
    p = argparse.ArgumentParser(description="Live nestdaq topology from Redis")
    p.add_argument("--redis", default=default_uri(), help="redis://host:port")
    p.add_argument("--port", type=int, default=8765)
    p.add_argument("--host", default="0.0.0.0")
    p.add_argument("--dump", action="store_true", help="print snapshot JSON and exit")
    p.add_argument("--ctl-host", default="127.0.0.1", help="daq-webctl host")
    p.add_argument("--ctl-port", type=int, default=8080, help="daq-webctl port")
    args = p.parse_args(argv)

    app = App(args.redis, args.ctl_host, args.ctl_port)
    if args.dump:
        print(json.dumps(app.snapshot(), indent=2))
        return 0

    ThreadingHTTPServer.allow_reuse_address = True
    try:
        httpd = ThreadingHTTPServer((args.host, args.port), make_handler(app))
    except OSError as exc:
        print(
            f"ERROR: {args.host}:{args.port} is already in use.\n"
            f"  {exc}\n"
            f"  Stop the old viewer, then retry:\n"
            f"    fuser -k {args.port}/tcp\n"
            f"    python3 serve.py",
            file=sys.stderr,
        )
        return 1
    print(f"topology viewer  http://{args.host}:{args.port}/", flush=True)
    print(f"redis            {args.redis}", flush=True)
    print("daq commands     Redis PUBLISH daqctl (same as WebGui, no 8080)", flush=True)
    print("polls /api/snapshot every 1s; presence TTL is the live instance set", flush=True)
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nstop")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
