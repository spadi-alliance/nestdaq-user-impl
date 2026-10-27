"""Live nestdaq topology from Redis.

Authority (same idea as daq-webctl WebGui / TopologyConfig):
  - Running processes: keys matching daq_service:<svc>:<id>:presence (TTL).
  - Logical topology:  daq_service:topology:endpoint:* and :link:*
  - Actual mesh:       :channel:<name>:peer lists of *present* instances only.

Leftover keys from a previous topology/run are classified as stale and
are never used to invent instances or connections.
"""

from __future__ import annotations

import json
import time
from collections import defaultdict
from pathlib import Path
from typing import Any, Dict, List, Optional, Set, Tuple

try:
    import redis
except ImportError:  # pragma: no cover
    redis = None


SEP = ":"
TOP = "daq_service"
PRESENCE = "presence"
HEALTH = "health"
STATE = "fair-mq-state"
UPDATED = "updatedTime"
CHANNEL = "channel"
PEER = "peer"
SOCKET = "socket"
INDEX = "service-instance-index"
TOPOLOGY = "topology"
ENDPOINT = "endpoint"
LINK = "link"

# Draw every crossing line at or below this; above it, use a mesh band.
# The topology page can override this per browser (input "box above").
MESH_LINE_LIMIT = 128

# Known application-level routing when autoSubChannel creates N destinations.
ROUTING = {
    "STFBuilder": "timeFrameId % N  (not counter RR)",
    "TimeFrameBuilder": "fDirection++ % N  (round-robin)",
    "LogicFilter": "fDirection++ % N  (round-robin)",
    "LogicFilterGpu": "fDirection++ % N  (round-robin)",
    "TimeFrameSlicerByLogicTiming": "fDirection++ % N  (round-robin)",
    "TFBFilePlayer": "fDirection++ % N  (round-robin)",
    "STFBFilePlayer": "fDirection++ % N  (round-robin)",
}


def scan_keys(r, match: str) -> List[str]:
    out: List[str] = []
    cursor = 0
    while True:
        cursor, keys = r.scan(cursor=cursor, match=match, count=200)
        out.extend(keys)
        if cursor == 0:
            break
    return out


def _truthy(value: Any) -> bool:
    return str(value).strip().lower() in {"1", "true", "yes", "on"}


def _inst_index(inst: str) -> int:
    if "-" in inst:
        tail = inst.rsplit("-", 1)[-1]
        if tail.isdigit():
            return int(tail)
    return 0


def _split_service_id(instance_id: str) -> Tuple[str, str]:
    if "-" not in instance_id:
        return "", ""
    service, tail = instance_id.rsplit("-", 1)
    if not service or not tail.isdigit():
        return "", ""
    return service, tail


def _sort_instances(names: List[str]) -> List[str]:
    return sorted(names, key=lambda n: (_inst_index(n), n))


def _param_kind(name: str) -> str:
    if "-" in name:
        tail = name.rsplit("-", 1)[-1]
        if tail.isdigit():
            return "instance"
    return "service"


def _param_sort_key(row: Dict[str, Any]) -> Tuple:
    name = row["name"]
    if row.get("kind") == "instance":
        svc = name.rsplit("-", 1)[0]
        return (svc, 1, _inst_index(name), name)
    return (name, 0, 0, name)


def _param_counts(registered: List[Dict[str, Any]]) -> Dict[str, int]:
    return {
        "keys": len(registered),
        "fields": sum(len(r.get("fields") or {}) for r in registered),
        "live": sum(1 for r in registered if r.get("live")),
        "leftover": sum(1 for r in registered if r.get("leftover")),
    }


class RedisTopology:
    def __init__(self, uri: str = "redis://127.0.0.1:6379"):
        if redis is None:
            raise RuntimeError("python package 'redis' is required")
        base = uri.rstrip("/")
        if base.endswith("/0") or base.endswith("/1") or base.endswith("/2"):
            base = base.rsplit("/", 1)[0]
        self.uri = base
        self.r0 = redis.Redis.from_url(base + "/0", decode_responses=True)
        self.r1 = redis.Redis.from_url(base + "/1", decode_responses=True)
        self.r2 = redis.Redis.from_url(base + "/2", decode_responses=True)

    def ping(self) -> bool:
        return bool(self.r0.ping())

    def clear_except_run(self) -> Dict[str, Any]:
        """Flush DB0/1/2. Keep only run_info:run_number and latest_run_number."""
        keep = {}
        for name in ("run_number", "latest_run_number"):
            val = self.r0.get(f"{RUN_INFO}:{name}")
            if val is not None:
                keep[name] = val
        self.r0.flushdb()
        self.r1.flushdb()
        self.r2.flushdb()
        for name, val in keep.items():
            self.r0.set(f"{RUN_INFO}:{name}", val)
        return {"ok": True, "kept": keep}

    def snapshot(self) -> Dict[str, Any]:
        t0 = time.time()
        stale: List[Dict[str, str]] = []
        warnings: List[str] = []

        endpoints = self._read_endpoints()
        links = self._read_links()
        live = self._read_live()
        index_stale = self._read_index_stale(live)
        stale.extend(index_stale)

        runtime, rt_stale = self._read_runtime(live)
        stale.extend(rt_stale)

        params, param_stale = self._read_params(live)
        stale.extend(param_stale)

        live_services = {inst["service"] for inst in live}
        topo_services = {ep["service"] for ep in endpoints}
        if not live:
            warnings.append(
                "no live presence yet — run run.sh, then the mesh is drawn from Redis"
            )
        for svc in sorted(topo_services - live_services):
            warnings.append(f"topology has {svc} but no live presence")
        for svc in sorted(live_services - topo_services):
            warnings.append(f"live process {svc} has no topology endpoint")

        expected = expand_mesh(endpoints, links, live)
        actual = mesh_from_runtime(runtime, live, endpoints, links)
        edges = actual["edges"]
        source = "redis" if edges else "none"

        services = self._services(endpoints, links, live, runtime, params)
        groups = classify_groups(links, services, edges)

        snap = {
            "ok": True,
            "uri": self.uri,
            "ts": time.time(),
            "elapsed_ms": int((time.time() - t0) * 1000),
            "source": source,
            "services": services,
            "endpoints": endpoints,
            "links": links,
            "instances": live,
            "edges": edges,
            "expected_edges": expected["edges"],
            "actual_edges": actual["edges"],
            "groups": groups,
            "params": params,
            "stale": stale,
            "warnings": warnings,
            "counts": {
                "services": len(services),
                "instances": len(live),
                "logical_links": len(links),
                "mesh_edges": len(edges),
                "stale_keys": len(stale),
            },
        }
        return snap

    def _read_endpoints(self) -> List[Dict[str, Any]]:
        keys = scan_keys(self.r0, f"{TOP}:{TOPOLOGY}:{ENDPOINT}:*")
        out = []
        prefix = f"{TOP}:{TOPOLOGY}:{ENDPOINT}:"
        for key in keys:
            rest = key[len(prefix):]
            # service:channel  (service names have no ':')
            if ":" not in rest:
                continue
            service, channel = rest.split(":", 1)
            h = self.r0.hgetall(key) or {}
            out.append({
                "service": service,
                "channel": channel,
                "type": h.get("type", ""),
                "method": h.get("method", ""),
                "transport": h.get("transport", "zeromq"),
                "autoSubChannel": _truthy(h.get("autoSubChannel", "false")),
                "numSockets": int(h.get("numSockets") or 0),
                "waitForPeerConnection": _truthy(h.get("waitForPeerConnection", "true")),
                "raw": h,
            })
        out.sort(key=lambda e: (e["service"], e["channel"]))
        return out

    def _read_links(self) -> List[Dict[str, str]]:
        keys = scan_keys(self.r0, f"{TOP}:{TOPOLOGY}:{LINK}:*")
        out = []
        prefix = f"{TOP}:{TOPOLOGY}:{LINK}:"
        for key in keys:
            rest = key[len(prefix):]
            if "," not in rest:
                continue
            left, right = rest.split(",", 1)
            if ":" not in left or ":" not in right:
                continue
            s1, c1 = left.split(":", 1)
            s2, c2 = right.split(":", 1)
            out.append({
                "from_service": s1,
                "from_channel": c1,
                "to_service": s2,
                "to_channel": c2,
                "value": self.r0.get(key) or "",
            })
        out.sort(key=lambda x: (x["from_service"], x["to_service"], x["from_channel"]))
        return out

    def write_topology(
        self,
        endpoints: List[Dict[str, Any]],
        links: List[Dict[str, str]],
        services: Optional[List[Dict[str, Any]]] = None,
    ) -> Dict[str, Any]:
        """Replace only topology:endpoint/link/count. Presence stays."""
        old = scan_keys(self.r0, f"{TOP}:{TOPOLOGY}:{ENDPOINT}:*")
        old.extend(scan_keys(self.r0, f"{TOP}:{TOPOLOGY}:{LINK}:*"))
        old.extend(scan_keys(self.r0, f"{TOP}:{TOPOLOGY}:count:*"))
        if old:
            self.r0.delete(*old)
        n_ep = 0
        for ep in endpoints or []:
            svc = str(ep.get("service") or "").strip()
            ch = str(ep.get("channel") or "").strip()
            if not svc or not ch or ":" in svc:
                continue
            key = f"{TOP}:{TOPOLOGY}:{ENDPOINT}:{svc}:{ch}"
            self.r0.hset(key, "type", str(ep.get("type") or "push"))
            self.r0.hset(key, "method", str(ep.get("method") or "bind"))
            self.r0.hset(key, "autoSubChannel", "true" if _truthy(ep.get("autoSubChannel")) else "false")
            n_ep += 1
        n_lk = 0
        for lk in links or []:
            a = str(lk.get("from_service") or "").strip()
            ac = str(lk.get("from_channel") or "").strip()
            b = str(lk.get("to_service") or "").strip()
            bc = str(lk.get("to_channel") or "").strip()
            if not a or not ac or not b or not bc:
                continue
            self.r0.set(f"{TOP}:{TOPOLOGY}:{LINK}:{a}:{ac},{b}:{bc}", "none")
            n_lk += 1
        counts: Dict[str, int] = {}
        for svc in services or []:
            name = str(svc.get("name") or "").strip()
            if not name:
                continue
            try:
                n = int(svc.get("n") or 0)
            except (TypeError, ValueError):
                n = 0
            n = max(0, min(n, 200))
            self.r0.set(f"{TOP}:{TOPOLOGY}:count:{name}", str(n))
            counts[name] = n
        return {"ok": True, "endpoints": n_ep, "links": n_lk, "counts": counts}

    def read_counts(self) -> Dict[str, int]:
        out: Dict[str, int] = {}
        for key in scan_keys(self.r0, f"{TOP}:{TOPOLOGY}:count:*"):
            name = key.rsplit(":", 1)[-1]
            try:
                out[name] = int(self.r0.get(key) or 0)
            except (TypeError, ValueError):
                out[name] = 0
        return out

    def write_run_params(self, services: List[Dict[str, Any]], run_dir: str) -> int:
        """Minimal DB2 parameters so FileSink / TFB can start."""
        root = Path(run_dir)
        n_keys = 0
        for svc in services or []:
            name = str(svc.get("name") or "").strip()
            binary = str(svc.get("binary") or name).strip()
            try:
                n = int(svc.get("n") or 0)
            except (TypeError, ValueError):
                n = 0
            n = max(0, min(n, 200))
            for i in range(n):
                key = f"parameters:{name}-{i}"
                if name in {"AmQStrTdcSampler", "Sampler"} or binary in {"AmQStrTdcSampler", "Sampler"}:
                    self.r2.hset(key, "msiTcpIp", f"192.168.2.{101 + i}")
                    self.r2.hset(key, "TdcType", "5")
                if binary == "FileSink" or name in {"FileSink", "DecSink"}:
                    sub = "dcmdata" if name == "DecSink" else "tdcdata"
                    dest = root / "data" / sub / f"{i:02d}"
                    dest.mkdir(parents=True, exist_ok=True)
                    self.r2.hset(key, "multipart", "true")
                    self.r2.hset(key, "prefix", str(dest))
                    self.r2.hset(key, "ext", ".dat")
                    self.r2.hset(key, "openmode", "recreate")
                if name == "TimeFrameBuilder":
                    self.r2.hset(key, "decimation-factor", "1000")
                    self.r2.hset(key, "decimation-offset", "0")
                if name == "STFBuilder":
                    self.r2.hset(key, "max-hbf", "4")
                if name == "TimeFrameSlicerByLogicTiming":
                    self.r2.hset(key, "time-offset-begin", "-250")
                    self.r2.hset(key, "time-offset-end", "250")
                n_keys += 1
        return n_keys

    def _read_live(self) -> List[Dict[str, Any]]:
        keys = scan_keys(self.r0, f"{TOP}:*:*:{PRESENCE}")
        live = []
        for key in keys:
            parts = key.split(SEP)
            if len(parts) != 4 or parts[0] != TOP or parts[3] != PRESENCE:
                continue
            _, service, inst, _ = parts
            if service in {TOPOLOGY, INDEX, "run_info"}:
                continue
            uuid = self.r0.get(key) or ""
            ttl = self.r0.ttl(key)
            state = self.r0.get(f"{TOP}:{service}:{inst}:{STATE}") or ""
            updated = self.r0.get(f"{TOP}:{service}:{inst}:{UPDATED}") or ""
            health = self.r0.hgetall(f"{TOP}:{service}:{inst}:{HEALTH}") or {}
            live.append({
                "id": inst,
                "service": service,
                "index": _inst_index(inst),
                "uuid": uuid,
                "ttl": ttl,
                "state": state,
                "updated": updated,
                "host": health.get("hostname") or health.get("hostName") or "",
                "ip": health.get("hostIp") or health.get("ip") or "",
            })
        live.sort(key=lambda x: (x["service"], x["index"], x["id"]))
        return live

    def instance_location(self, instance_id: str) -> Dict[str, str]:
        """Host and IP from the process health hash. Empty when it has no presence."""
        service, tail = _split_service_id(instance_id)
        if not service or not tail:
            return {"host": "", "ip": ""}
        health = self.r0.hgetall(f"{TOP}:{service}:{instance_id}:{HEALTH}") or {}
        return {
            "host": health.get("hostname") or health.get("hostName") or "",
            "ip": health.get("hostIp") or health.get("ip") or "",
        }

    def instance_account(self, instance_id: str) -> str:
        """Unix account implied by a parameter path such as /home/nestdaq/data/00."""
        prefix = self.r2.hget(f"parameters:{instance_id}", "prefix") or ""
        parts = [p for p in str(prefix).split("/") if p]
        if len(parts) >= 2 and parts[0] == "home":
            return parts[1]
        return ""

    def _read_index_stale(self, live: List[Dict[str, Any]]) -> List[Dict[str, str]]:
        live_uuid = {x["uuid"] for x in live if x["uuid"]}
        stale = []
        for key in scan_keys(self.r0, f"{TOP}:{INDEX}:*"):
            h = self.r0.hgetall(key) or {}
            for index, uuid in h.items():
                if uuid not in live_uuid:
                    stale.append({
                        "key": key,
                        "field": index,
                        "reason": "service-instance-index uuid has no presence",
                    })
        return stale

    def _read_runtime(self, live: List[Dict[str, Any]]):
        present = {(x["service"], x["id"]) for x in live}
        present_ids = {x["id"] for x in live}
        runtime: Dict[str, Any] = defaultdict(lambda: {"channels": {}})
        stale: List[Dict[str, str]] = []

        for key in scan_keys(self.r0, f"{TOP}:*:*:{CHANNEL}:*"):
            parts = key.split(SEP)
            # daq_service : svc : id : channel : name
            # daq_service : svc : id : channel : name : peer
            if len(parts) < 5 or parts[0] != TOP or parts[3] != CHANNEL:
                continue
            service, inst = parts[1], parts[2]
            if (service, inst) not in present:
                stale.append({"key": key, "reason": "channel/peer of instance without presence"})
                continue
            ch_name = parts[4]
            inst_rt = runtime[inst]
            inst_rt["service"] = service
            inst_rt["id"] = inst
            ch = inst_rt["channels"].setdefault(ch_name, {
                "name": ch_name, "props": {}, "peers": [], "sockets": [],
            })
            if len(parts) == 5:
                props = self.r0.hgetall(key) or {}
                ch["props"] = props
                ch["autoSubChannel"] = _truthy(props.get("autoSubChannel", "false"))
                ch["numSockets"] = int(props.get("numSockets") or 0)
                ch["type"] = props.get("type", "")
                ch["method"] = props.get("method", "")
                ch["address"] = props.get("address", "")
            elif len(parts) == 6 and parts[5] == PEER:
                raw_peers = self.r0.lrange(key, 0, -1) or []
                kept = []
                for p in raw_peers:
                    parsed = _parse_channel_key(p)
                    if not parsed:
                        stale.append({"key": p, "reason": "unparseable peer entry"})
                        continue
                    if parsed["id"] not in present_ids:
                        stale.append({
                            "key": p,
                            "reason": "peer points at instance without presence",
                        })
                        continue
                    kept.append(parsed)
                ch["peers"] = kept

        for key in scan_keys(self.r0, f"{TOP}:*:*:{SOCKET}:*"):
            parts = key.split(SEP, 4)
            if len(parts) != 5 or parts[0] != TOP or parts[3] != SOCKET:
                continue
            service, inst, rest = parts[1], parts[2], parts[4]
            if (service, inst) not in present:
                stale.append({"key": key, "reason": "socket of instance without presence"})
                continue
            if not rest.startswith("chans."):
                continue
            bits = rest.split(".")
            if len(bits) < 3:
                continue
            ch_name = bits[1]
            try:
                sidx = int(bits[2])
            except ValueError:
                sidx = 0
            h = self.r0.hgetall(key) or {}
            inst_rt = runtime[inst]
            inst_rt["service"] = service
            inst_rt["id"] = inst
            ch = inst_rt["channels"].setdefault(ch_name, {
                "name": ch_name, "props": {}, "peers": [], "sockets": [],
            })
            ch.setdefault("sockets", []).append({
                "index": sidx,
                "address": h.get("address", ""),
                "method": h.get("method", ""),
            })
            if h.get("address") and not ch.get("address"):
                ch["address"] = h.get("address", "")
            if h.get("method") and not ch.get("method"):
                ch["method"] = h.get("method", "")

        return dict(runtime), stale

    def params_snapshot(self) -> Dict[str, Any]:
        """Every Redis DB2 parameters:* hash from the last param.sh (or leftover)."""
        t0 = time.time()
        live = self._read_live()
        registered = self._read_registered_params(live)
        warnings: List[str] = []
        if not registered:
            warnings.append("no parameters:* keys in Redis DB2 — run param.sh")
        return {
            "ok": True,
            "uri": self.uri,
            "ts": time.time(),
            "elapsed_ms": int((time.time() - t0) * 1000),
            "params": registered,
            "counts": _param_counts(registered),
            "warnings": warnings,
        }

    def _read_registered_params(self, live: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
        present_ids = {x["id"] for x in live}
        present_svc = {x["service"] for x in live}
        rows: List[Dict[str, Any]] = []
        for key in scan_keys(self.r2, "parameters:*"):
            name = key.split(":", 1)[1]
            h = self.r2.hgetall(key) or {}
            kind = _param_kind(name)
            leftover = kind == "instance" and name not in present_ids
            rows.append({
                "name": name,
                "key": key,
                "fields": dict(sorted(h.items())),
                "kind": kind,
                "live": name in present_ids or name in present_svc,
                "leftover": leftover,
            })
        rows.sort(key=_param_sort_key)
        return rows

    def _read_params(self, live: List[Dict[str, Any]]):
        registered = self._read_registered_params(live)
        params: Dict[str, Dict[str, str]] = {}
        stale = []
        for row in registered:
            if row["leftover"]:
                stale.append({
                    "key": row["key"],
                    "reason": "parameter instance has no presence (leftover after new config)",
                })
                continue
            params[row["name"]] = row["fields"]
        return params, stale

    def _services(self, endpoints, links, live, runtime, params):
        names: Set[str] = {x["service"] for x in live}

        by_svc: Dict[str, List[Dict[str, Any]]] = defaultdict(list)
        for inst in live:
            by_svc[inst["service"]].append(inst)

        eps_by_svc: Dict[str, List[Dict[str, Any]]] = defaultdict(list)
        for ep in endpoints:
            eps_by_svc[ep["service"]].append(ep)

        out = []
        for name in sorted(names):
            insts = by_svc.get(name, [])
            out.append({
                "name": name,
                "instances": [i["id"] for i in insts],
                "live_ids": [i["id"] for i in insts if not i.get("planned")],
                "n": len(insts),
                "endpoints": eps_by_svc.get(name, []),
                "routing": ROUTING.get(name, ""),
                "params": params.get(name, {}),
            })
        return out


def _parse_channel_key(key: str) -> Optional[Dict[str, str]]:
    parts = key.split(SEP)
    if len(parts) < 5 or parts[0] != TOP or parts[3] != CHANNEL:
        return None
    return {
        "service": parts[1],
        "id": parts[2],
        "channel": parts[4],
        "key": key,
    }


def _edge_keyset(edges: List[Dict[str, Any]]) -> Set[Tuple]:
    return {
        (e["from_id"], e["from_channel"], e["to_id"], e["to_channel"])
        for e in edges
    }


def _num_sockets(ep: Dict[str, Any], peer_n: int) -> int:
    if ep.get("autoSubChannel"):
        return max(peer_n, 1)
    n = int(ep.get("numSockets") or 0)
    return n if n > 0 else 1


def _find_ep(endpoints, service, channel) -> Dict[str, Any]:
    for ep in endpoints:
        if ep["service"] == service and ep["channel"] == channel:
            return ep
    return {"service": service, "channel": channel, "autoSubChannel": False,
            "type": "", "method": "", "numSockets": 0}


def expand_mesh(endpoints, links, live) -> Dict[str, Any]:
    """TopologyConfig::ResolveConnectAddress pairing, using live presence counts."""
    by_svc: Dict[str, List[str]] = defaultdict(list)
    for inst in live:
        by_svc[inst["service"]].append(inst["id"])
    for svc in by_svc:
        by_svc[svc] = _sort_instances(by_svc[svc])

    edges = []
    for lk in links:
        a_svc, a_ch = lk["from_service"], lk["from_channel"]
        b_svc, b_ch = lk["to_service"], lk["to_channel"]
        a_ids = by_svc.get(a_svc, [])
        b_ids = by_svc.get(b_svc, [])
        if not a_ids or not b_ids:
            continue
        ep_a = _find_ep(endpoints, a_svc, a_ch)
        ep_b = _find_ep(endpoints, b_svc, b_ch)
        n_a = _num_sockets(ep_a, len(b_ids))
        n_b = _num_sockets(ep_b, len(a_ids))
        auto = bool(ep_a.get("autoSubChannel") or ep_b.get("autoSubChannel"))
        routing = ""
        if auto and max(len(a_ids), len(b_ids)) > 1:
            routing = ROUTING.get(a_svc, "round-robin / FairMQ PUSH")

        for i, a_id in enumerate(a_ids):
            for j, b_id in enumerate(b_ids):
                connect = False
                sa = 0
                sb = 0
                if n_a <= 1 and n_b <= 1:
                    # 1:1 index pair, or fan-in/out when the other side has one instance
                    if i == j or len(a_ids) == 1 or len(b_ids) == 1:
                        connect = True
                elif n_a <= 1 and n_b > 1:
                    connect = True
                    sb = i
                elif n_a > 1 and n_b <= 1:
                    connect = True
                    sa = j
                else:
                    connect = True
                    sa = j
                    sb = i
                if not connect:
                    continue
                edges.append({
                    "from_id": a_id,
                    "from_service": a_svc,
                    "from_channel": a_ch,
                    "from_sub": sa,
                    "to_id": b_id,
                    "to_service": b_svc,
                    "to_channel": b_ch,
                    "to_sub": sb,
                    "autoSubChannel": auto,
                    "routing": routing,
                    "origin": "expected",
                })
    return {"edges": edges}


def _split_addrs(raw: str) -> List[str]:
    s = (raw or "").strip()
    if not s or s.lower() in {"unspecified", "none", "null"}:
        return []
    return [x.strip() for x in s.split(",") if x.strip()]


def _parse_ep_addr(addr: str) -> Dict[str, str]:
    out = {"raw": addr, "scheme": "", "host": "", "port": "", "path": ""}
    if "://" not in addr:
        return out
    scheme, rest = addr.split("://", 1)
    out["scheme"] = scheme.lower()
    if out["scheme"] in {"ipc", "inproc"}:
        out["path"] = rest
        return out
    if rest.startswith("["):
        end = rest.find("]")
        if end > 0:
            out["host"] = rest[1:end]
            tail = rest[end + 1:]
            if tail.startswith(":"):
                out["port"] = tail[1:]
            return out
    if ":" in rest:
        host, port = rest.rsplit(":", 1)
        out["host"] = host
        out["port"] = port
    else:
        out["host"] = rest
    return out


def _addrs_match(a: Dict[str, str], b: Dict[str, str]) -> bool:
    if not a.get("scheme") or a["scheme"] != b.get("scheme"):
        return False
    if a["scheme"] in {"ipc", "inproc"}:
        return bool(a.get("path")) and a["path"] == b.get("path")
    if not a.get("port") or a["port"] != b.get("port"):
        return False
    wild = {"", "*", "0.0.0.0", "::"}
    if a.get("host") in wild or b.get("host") in wild:
        return True
    return a.get("host") == b.get("host")


def _channel_addrs(ch: Dict[str, Any]) -> List[str]:
    addrs: List[str] = []
    for sock in ch.get("sockets") or []:
        addrs.extend(_split_addrs(sock.get("address") or ""))
    if not addrs:
        addrs = _split_addrs(ch.get("address") or "")
    return addrs


def mesh_from_runtime(runtime, live, endpoints, links) -> Dict[str, Any]:
    """Edges from Redis bind/connect addresses of present instances only.

    Peer lists are candidate names (every instance of the linked service).
    After CONNECT, socket/channel address is the real target. Match those.
    """
    if not runtime:
        return {"edges": []}
    topo_fwd = {
        (lk["from_service"], lk["from_channel"], lk["to_service"], lk["to_channel"])
        for lk in links
    }
    topo_rev = {(b, d, a, c) for (a, c, b, d) in topo_fwd}
    linked = topo_fwd | topo_rev

    binds: List[Dict[str, Any]] = []
    connects: List[Dict[str, Any]] = []
    for inst, rt in runtime.items():
        svc = rt.get("service")
        for ch_name, ch in rt.get("channels", {}).items():
            method = (ch.get("method") or (ch.get("props") or {}).get("method") or "").lower()
            auto = bool(ch.get("autoSubChannel"))
            for i, raw in enumerate(_channel_addrs(ch)):
                rec = {
                    "id": inst, "service": svc, "channel": ch_name,
                    "sub": i, "auto": auto, **_parse_ep_addr(raw),
                }
                if method == "bind":
                    binds.append(rec)
                elif method == "connect":
                    connects.append(rec)

    edges = []
    seen = set()
    for c in connects:
        for b in binds:
            pair = (c["service"], c["channel"], b["service"], b["channel"])
            if links and pair not in linked:
                continue
            if not _addrs_match(c, b):
                continue
            if (c["service"], c["channel"], b["service"], b["channel"]) in topo_fwd:
                src, dst, sa, sb = c, b, c["sub"], b["sub"]
            elif (b["service"], b["channel"], c["service"], c["channel"]) in topo_fwd:
                src, dst, sa, sb = b, c, b["sub"], c["sub"]
            else:
                src, dst, sa, sb = c, b, c["sub"], b["sub"]
            key = (src["id"], src["channel"], dst["id"], dst["channel"], sa, sb)
            if key in seen:
                continue
            seen.add(key)
            auto = bool(src.get("auto") or dst.get("auto")
                        or _find_ep(endpoints, src["service"], src["channel"]).get("autoSubChannel")
                        or _find_ep(endpoints, dst["service"], dst["channel"]).get("autoSubChannel"))
            routing = ROUTING.get(src["service"], "round-robin / FairMQ PUSH") if auto else ""
            edges.append({
                "from_id": src["id"],
                "from_service": src["service"],
                "from_channel": src["channel"],
                "from_sub": sa,
                "to_id": dst["id"],
                "to_service": dst["service"],
                "to_channel": dst["channel"],
                "to_sub": sb,
                "autoSubChannel": auto,
                "routing": routing,
                "origin": "redis",
            })
    return {"edges": edges}


def classify_groups(links, services, edges) -> List[Dict[str, Any]]:
    n_of = {s["name"]: s["n"] for s in services}
    groups = []
    for lk in links:
        a, b = lk["from_service"], lk["to_service"]
        ge = [e for e in edges
              if e["from_service"] == a and e["to_service"] == b
              and e["from_channel"] == lk["from_channel"]
              and e["to_channel"] == lk["to_channel"]]
        na, nb = n_of.get(a, 0), n_of.get(b, 0)
        kind = "none"
        if na and nb:
            if len(ge) == min(na, nb) and (na == nb or min(na, nb) == 1):
                kind = "1:1" if na == nb else ("fan-in" if na > nb else "fan-out")
            elif na > 1 and nb == 1:
                kind = "fan-in"
            elif na == 1 and nb > 1:
                kind = "fan-out"
            elif len(ge) >= na * nb and na > 1 and nb > 1:
                kind = "mesh"
            elif na > 1 and nb > 1:
                kind = "mesh"
            else:
                kind = "custom"
        auto = any(e["autoSubChannel"] for e in ge)
        if not auto:
            for s in services:
                for ep in s.get("endpoints") or []:
                    if ep["service"] == a and ep["channel"] == lk["from_channel"] and ep.get("autoSubChannel"):
                        auto = True
        groups.append({
            "from_service": a,
            "from_channel": lk["from_channel"],
            "to_service": b,
            "to_channel": lk["to_channel"],
            "kind": kind,
            "n_from": na,
            "n_to": nb,
            "n_edges": len(ge),
            "autoSubChannel": auto,
            "bundle": kind == "mesh" and len(ge) > MESH_LINE_LIMIT,
            "routing": ge[0]["routing"] if ge else "",
        })
    return groups


def empty_snapshot(uri: str = "", warnings: Optional[List[str]] = None) -> Dict[str, Any]:
    """Empty live graph when Redis is down. Does not invent processes."""
    return {
        "ok": False,
        "uri": uri,
        "ts": time.time(),
        "elapsed_ms": 0,
        "source": "none",
        "services": [],
        "endpoints": [],
        "links": [],
        "instances": [],
        "edges": [],
        "expected_edges": [],
        "actual_edges": [],
        "groups": [],
        "params": {},
        "stale": [],
        "warnings": list(warnings or []),
        "counts": {
            "services": 0,
            "instances": 0,
            "logical_links": 0,
            "mesh_edges": 0,
            "stale_keys": 0,
        },
    }


def empty_params_snapshot(uri: str = "", warnings: Optional[List[str]] = None) -> Dict[str, Any]:
    return {
        "ok": False,
        "uri": uri,
        "ts": time.time(),
        "elapsed_ms": 0,
        "params": [],
        "counts": _param_counts([]),
        "warnings": list(warnings or []),
    }


DAQCTL_CHANNEL = "daqctl"
RUN_INFO = "run_info"
KNOWN_DAQ_COMMANDS = {
    "BIND", "COMPLETE INIT", "CONNECT", "END", "INIT DEVICE", "INIT TASK",
    "RESET DEVICE", "RESET TASK", "RUN", "STOP", "exit", "quit", "reset", "start",
}
KNOWN_RUN_INFO = {"run_number", "wait-device-ready", "wait-ready"}


def _daq_services(live: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
    by: Dict[str, List[Dict[str, Any]]] = defaultdict(list)
    for inst in live:
        by[inst["service"]].append(inst)
    out = []
    for svc, rows in sorted(by.items()):
        counts: Dict[str, int] = defaultdict(int)
        insts = []
        for row in rows:
            st = row.get("state") or "Undefined"
            counts[st] += 1
            insts.append({
                "service": row["service"],
                "instance": row["id"],
                "state": st,
                "date": row.get("updated") or "",
            })
        out.append({
            "service": svc,
            "n_instances": len(rows),
            "date": max((r.get("updated") or "") for r in rows),
            "counts": [{"name": k, "value": v} for k, v in counts.items()],
            "instances": insts,
        })
    return out


# RedisTopology DAQ helpers (same path as WebGui::RedisPublishDaqCommand)
def daq_status_from(rt: "RedisTopology") -> Dict[str, Any]:
    live = rt._read_live()
    return {
        "ok": True,
        "via": "redis",
        "channel": DAQCTL_CHANNEL,
        "run_number": rt.r0.get(f"{RUN_INFO}:run_number") or "",
        "latest_run_number": rt.r0.get(f"{RUN_INFO}:latest_run_number") or "",
        "wait_device_ready": _truthy(rt.r0.get(f"{RUN_INFO}:wait-device-ready") or "true"),
        "wait_ready": _truthy(rt.r0.get(f"{RUN_INFO}:wait-ready") or "true"),
        "services": _daq_services(live),
        "service_list_changed": "true",
        "instance_list_changed": "true",
    }


def daq_set_run_info(rt: "RedisTopology", name: str, value: str) -> str:
    if name not in KNOWN_RUN_INFO:
        raise ValueError(f"unknown run_info field: {name}")
    rt.r0.set(f"{RUN_INFO}:{name}", value)
    return rt.r0.get(f"{RUN_INFO}:{name}") or ""


def daq_incr_run(rt: "RedisTopology") -> int:
    return int(rt.r0.incr(f"{RUN_INFO}:run_number"))


def daq_publish(rt: "RedisTopology", value: str, services: List[str], instances: List[str]) -> Dict[str, Any]:
    cmd = str(value or "").strip()
    if cmd not in KNOWN_DAQ_COMMANDS:
        raise ValueError(f"unknown command: {cmd}")
    svcs = services or ["all"]
    insts = instances or ["all"]
    if cmd == "RUN":
        cur = rt.r0.get(f"{RUN_INFO}:run_number")
        if cur:
            rt.r0.set(f"{RUN_INFO}:latest_run_number", cur)
    payload = {
        "command": "change_state",
        "value": cmd,
        "services": svcs,
        "instances": insts,
    }
    n = rt.r0.publish(DAQCTL_CHANNEL, json.dumps(payload))
    return {"ok": True, "subscribers": int(n), "payload": payload}


def dumps(snap: Dict[str, Any]) -> str:
    return json.dumps(snap, indent=2, sort_keys=False)
