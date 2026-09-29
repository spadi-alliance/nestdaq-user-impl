"""Append live tmux pane text to one log file per process while enabled."""

from __future__ import annotations

import threading
import time
from pathlib import Path

from tmux_view import capture_instance

LOG_DIR = Path(__file__).resolve().parent / "run" / "logs"


class TmuxLog:
    def __init__(self, app) -> None:
        self.app = app
        self._stop = threading.Event()
        self._thread: threading.Thread | None = None
        self._prev: dict[str, str] = {}
        self._lock = threading.Lock()
        self._enabled = False

    def status(self) -> dict:
        with self._lock:
            enabled = self._enabled
        return {"ok": True, "enabled": enabled, "dir": str(LOG_DIR)}

    def set_enabled(self, enabled: bool) -> dict:
        with self._lock:
            self._enabled = bool(enabled)
            alive = self._thread is not None and self._thread.is_alive()
            if self._enabled and not alive:
                self._stop.clear()
                self._thread = threading.Thread(target=self._loop, name="tmux-log", daemon=True)
                self._thread.start()
            elif not self._enabled:
                self._stop.set()
        return self.status()

    def _loop(self) -> None:
        while not self._stop.is_set():
            try:
                self._once()
            except Exception:
                pass
            self._stop.wait(1.0)

    def _once(self) -> None:
        rt = self.app.rt
        if rt is None:
            return
        LOG_DIR.mkdir(parents=True, exist_ok=True)
        for inst in rt._read_live():
            iid = inst.get("id") or ""
            if not iid:
                continue
            try:
                account = rt.instance_account(iid)
                row = capture_instance(iid, inst.get("host") or "", inst.get("ip") or "", account)
            except Exception:
                continue
            if not row.get("found"):
                continue
            text = row.get("text") or ""
            extra = _new_lines(self._prev.get(iid, ""), text)
            self._prev[iid] = text
            if not extra:
                continue
            path = LOG_DIR / f"{iid}.log"
            with path.open("a", encoding="utf-8") as fh:
                fh.write(extra)


def _new_lines(prev: str, new: str) -> str:
    """Lines that appeared since the previous capture of the same pane."""
    new_lines = new.splitlines()
    if not new_lines:
        return ""
    prev_lines = prev.splitlines()
    if not prev_lines:
        return "\n".join(new_lines) + "\n"
    if new_lines == prev_lines:
        return ""
    overlap = 0
    limit = min(len(prev_lines), len(new_lines))
    for size in range(limit, 0, -1):
        if prev_lines[-size:] == new_lines[:size]:
            overlap = size
            break
    extra = new_lines[overlap:] if overlap else new_lines
    if not extra:
        return ""
    return "\n".join(extra) + "\n"
