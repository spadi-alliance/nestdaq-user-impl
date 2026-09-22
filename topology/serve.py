#!/usr/bin/env python3
"""Live topology viewer. Reads Redis the same way daq-webctl does.

  python3 topology/serve.py
  python3 topology/serve.py --redis redis://127.0.0.1:6379 --port 8765
  python3 topology/serve.py --dump
  python3 topology/serve.py --demo
"""

from __future__ import annotations

import argparse
import json
import sys
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlparse

HERE = Path(__file__).resolve().parent
STATIC = HERE / "static"
DOCS = STATIC / "docs"
DOC_FILES = {
    "technical-note-en.pdf": (
        DOCS / "technical-note-en.pdf",
        "application/pdf",
        "nestdaq-topology-technical-note-en.pdf",
    ),
    "technical-note-ko.pdf": (
        DOCS / "technical-note-ko.pdf",
        "application/pdf",
        "nestdaq-topology-technical-note-ko.pdf",
    ),
    "technical-note-en.tex": (
        HERE / "docs" / "technical-note-en.tex",
        "application/x-tex; charset=utf-8",
        "nestdaq-topology-technical-note-en.tex",
    ),
}
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))

from redis_live import RedisTopology, demo_params_snapshot, demo_snapshot  # noqa: E402


def default_uri() -> str:
    for cand in (
        HERE.parent / "run" / "redis-server.txt",
        HERE.parent / "run_daid-daq-01" / "redis-server.txt",
    ):
        if cand.is_file():
            host = cand.read_text().strip().split()[0]
            if host:
                return f"redis://{host}"
    return "redis://127.0.0.1:6379"


class App:
    def __init__(self, uri: str, demo: bool):
        self.uri = uri
        self.force_demo = demo
        self.rt = None
        if not demo:
            try:
                self.rt = RedisTopology(uri)
            except Exception as exc:  # noqa: BLE001
                print(f"Redis client init failed: {exc}", file=sys.stderr)

    def snapshot(self) -> dict:
        if self.force_demo:
            return demo_snapshot()
        if self.rt is None:
            snap = demo_snapshot()
            snap["warnings"].append("Redis client unavailable; showing demo")
            return snap
        try:
            self.rt.ping()
        except Exception as exc:  # noqa: BLE001
            snap = demo_snapshot()
            snap["warnings"].append(f"Redis ping failed ({exc}); showing demo")
            return snap
        snap = self.rt.snapshot()
        if snap["counts"]["instances"] == 0 and snap["counts"]["logical_links"] == 0:
            demo = demo_snapshot()
            demo["warnings"].append(
                "Redis is up but has no topology/presence keys. "
                "Run topology.sh / param.sh / run-*.sh, then reload. "
                "Showing production-shaped demo until then."
            )
            demo["uri"] = snap["uri"]
            return demo
        return snap

    def params(self) -> dict:
        if self.force_demo:
            return demo_params_snapshot()
        if self.rt is None:
            snap = demo_params_snapshot()
            snap["warnings"].append("Redis client unavailable; showing demo")
            return snap
        try:
            self.rt.ping()
        except Exception as exc:  # noqa: BLE001
            snap = demo_params_snapshot()
            snap["warnings"].append(f"Redis ping failed ({exc}); showing demo")
            return snap
        return self.rt.params_snapshot()


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
            if path in ("/", "/index.html", "/params"):
                return self._file(STATIC / "index.html", "text/html; charset=utf-8")
            if path == "/viewer.js":
                return self._file(STATIC / "viewer.js", "application/javascript")
            if path == "/viewer.css":
                return self._file(STATIC / "viewer.css", "text/css")
            if path.startswith("/docs/"):
                name = Path(path).name
                spec = DOC_FILES.get(name)
                if spec:
                    fpath, ctype, download = spec
                    return self._file(
                        fpath,
                        ctype,
                        extra={"Content-Disposition": f'attachment; filename="{download}"'},
                    )
            if path == "/api/snapshot":
                q = urlparse(self.path).query
                force_demo = "demo=1" in q
                body = json.dumps(demo_snapshot() if force_demo else app.snapshot()).encode("utf-8")
                return self._send(200, body, "application/json")
            if path == "/api/params":
                q = urlparse(self.path).query
                force_demo = "demo=1" in q
                body = json.dumps(
                    demo_params_snapshot() if force_demo else app.params()
                ).encode("utf-8")
                return self._send(200, body, "application/json")
            self._send(404, b"not found", "text/plain")

        def _file(self, path: Path, ctype: str, extra=None):
            if not path.is_file():
                return self._send(404, b"missing static file", "text/plain")
            return self._send(200, path.read_bytes(), ctype, extra=extra)

    return Handler


def main(argv=None) -> int:
    p = argparse.ArgumentParser(description="Live nestdaq topology from Redis")
    p.add_argument("--redis", default=default_uri(), help="redis://host:port")
    p.add_argument("--port", type=int, default=8765)
    p.add_argument("--host", default="127.0.0.1")
    p.add_argument("--dump", action="store_true", help="print snapshot JSON and exit")
    p.add_argument("--demo", action="store_true", help="force demo snapshot")
    args = p.parse_args(argv)

    app = App(args.redis, args.demo)
    if args.dump:
        print(json.dumps(app.snapshot(), indent=2))
        return 0

    httpd = ThreadingHTTPServer((args.host, args.port), make_handler(app))
    print(f"topology viewer  http://{args.host}:{args.port}/", flush=True)
    print(f"redis            {args.redis}", flush=True)
    print("polls /api/snapshot every 1s; presence TTL is the live instance set", flush=True)
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nstop")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
