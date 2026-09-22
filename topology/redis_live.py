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

# Draw every crossing line below this; above it, use a mesh band.
MESH_LINE_LIMIT = 32

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
        source = "actual" if actual["edges"] else "expected"
        edges = actual["edges"] if actual["edges"] else expected["edges"]

        if actual["edges"] and expected["edges"]:
            a = _edge_keyset(actual["edges"])
            e = _edge_keyset(expected["edges"])
            if a != e:
                warnings.append(
                    f"actual peer mesh ({len(a)}) differs from topology+presence "
                    f"expectation ({len(e)}); leftover connect state or devices "
                    f"still negotiating"
                )

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
                "name": ch_name, "props": {}, "peers": [],
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
            parts = key.split(SEP)
            if len(parts) < 5 or parts[0] != TOP:
                continue
            service, inst = parts[1], parts[2]
            if (service, inst) not in present:
                stale.append({"key": key, "reason": "socket of instance without presence"})
                continue

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
            "demo": False,
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
        names: Set[str] = set()
        names.update(ep["service"] for ep in endpoints)
        for lk in links:
            names.add(lk["from_service"])
            names.add(lk["to_service"])
        names.update(x["service"] for x in live)

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


def mesh_from_runtime(runtime, live, endpoints, links) -> Dict[str, Any]:
    """Use peer lists written after CONNECT. Drop peers without presence."""
    if not runtime:
        return {"edges": []}
    link_set = {
        (lk["from_service"], lk["from_channel"], lk["to_service"], lk["to_channel"])
        for lk in links
    }
    link_set |= {(b, d, a, c) for (a, c, b, d) in list(link_set)}

    inst_svc = {x["id"]: x["service"] for x in live}
    edges = []
    seen = set()
    for inst, rt in runtime.items():
        svc = rt.get("service") or inst_svc.get(inst)
        for ch_name, ch in rt.get("channels", {}).items():
            auto = bool(ch.get("autoSubChannel"))
            for idx, peer in enumerate(ch.get("peers") or []):
                pair = (svc, ch_name, peer["service"], peer["channel"])
                if links and pair not in link_set:
                    continue
                # Orient along the topology.sh link order when known.
                forward = (svc, ch_name, peer["service"], peer["channel"])
                reverse = (peer["service"], peer["channel"], svc, ch_name)
                if forward in {(lk["from_service"], lk["from_channel"],
                                lk["to_service"], lk["to_channel"]) for lk in links}:
                    src_id, src_svc, src_ch = inst, svc, ch_name
                    dst_id, dst_svc, dst_ch = peer["id"], peer["service"], peer["channel"]
                    sa, sb = idx, 0
                elif reverse in {(lk["from_service"], lk["from_channel"],
                                  lk["to_service"], lk["to_channel"]) for lk in links}:
                    src_id, src_svc, src_ch = peer["id"], peer["service"], peer["channel"]
                    dst_id, dst_svc, dst_ch = inst, svc, ch_name
                    sa, sb = 0, idx
                else:
                    src_id, src_svc, src_ch = inst, svc, ch_name
                    dst_id, dst_svc, dst_ch = peer["id"], peer["service"], peer["channel"]
                    sa, sb = idx, 0
                key = (src_id, src_ch, dst_id, dst_ch)
                if key in seen:
                    continue
                seen.add(key)
                routing = ""
                if auto:
                    routing = ROUTING.get(src_svc, "round-robin / FairMQ PUSH")
                edges.append({
                    "from_id": src_id,
                    "from_service": src_svc,
                    "from_channel": src_ch,
                    "from_sub": sa,
                    "to_id": dst_id,
                    "to_service": dst_svc,
                    "to_channel": dst_ch,
                    "to_sub": sb,
                    "autoSubChannel": auto,
                    "routing": routing,
                    "origin": "actual",
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


def demo_snapshot() -> Dict[str, Any]:
    """In-memory snapshot matching run_daid-daq-01 production counts.

    Used when Redis is empty so the viewer can still show the mesh policy.
    """
    spec = [
        ("AmQStrTdcSampler", 6, [("out", "push", "bind", False)]),
        ("STFBuilder", 6, [
            ("in", "pull", "connect", False),
            ("out", "push", "connect", True),
        ]),
        ("TimeFrameBuilder", 1, [
            ("in", "pull", "bind", False),
            ("out", "push", "connect", True),
            ("decimator", "push", "connect", False),
        ]),
        ("LogicFilter", 4, [
            ("in", "pull", "bind", False),
            ("out", "push", "connect", True),
        ]),
        ("TimeFrameSlicerByLogicTiming", 4, [
            ("in", "pull", "bind", False),
            ("out", "push", "connect", True),
        ]),
        ("FileSink", 1, [("in", "pull", "bind", False)]),
        ("DecSink", 1, [("in", "pull", "bind", False)]),
    ]
    endpoints = []
    live = []
    services = []
    for name, n, eps in spec:
        for ch, typ, method, auto in eps:
            endpoints.append({
                "service": name, "channel": ch, "type": typ, "method": method,
                "transport": "zeromq", "autoSubChannel": auto, "numSockets": 0,
                "waitForPeerConnection": True, "raw": {},
            })
        ids = [f"{name}-{i}" for i in range(n)]
        for i, inst in enumerate(ids):
            live.append({
                "id": inst, "service": name, "index": i, "uuid": f"demo-{inst}",
                "ttl": 5, "state": "RUNNING", "updated": "", "host": "demo", "ip": "",
            })
        services.append({
            "name": name, "instances": ids, "n": n,
            "endpoints": [e for e in endpoints if e["service"] == name],
            "routing": ROUTING.get(name, ""), "params": {},
        })
    links = [
        {"from_service": "AmQStrTdcSampler", "from_channel": "out",
         "to_service": "STFBuilder", "to_channel": "in", "value": "none"},
        {"from_service": "STFBuilder", "from_channel": "out",
         "to_service": "TimeFrameBuilder", "to_channel": "in", "value": "none"},
        {"from_service": "TimeFrameBuilder", "from_channel": "out",
         "to_service": "LogicFilter", "to_channel": "in", "value": "none"},
        {"from_service": "TimeFrameBuilder", "from_channel": "decimator",
         "to_service": "DecSink", "to_channel": "in", "value": "none"},
        {"from_service": "LogicFilter", "from_channel": "out",
         "to_service": "TimeFrameSlicerByLogicTiming", "to_channel": "in",
         "value": "none"},
        {"from_service": "TimeFrameSlicerByLogicTiming", "from_channel": "out",
         "to_service": "FileSink", "to_channel": "in", "value": "none"},
    ]
    expected = expand_mesh(endpoints, links, live)
    groups = classify_groups(links, services, expected["edges"])
    return {
        "ok": True,
        "uri": "demo://stf-tf-flt-tfs",
        "ts": time.time(),
        "elapsed_ms": 0,
        "source": "expected",
        "demo": True,
        "services": services,
        "endpoints": endpoints,
        "links": links,
        "instances": live,
        "edges": expected["edges"],
        "expected_edges": expected["edges"],
        "actual_edges": [],
        "groups": groups,
        "params": {},
        "stale": [],
        "warnings": ["demo snapshot (Redis empty or --demo); not live presence"],
        "counts": {
            "services": len(services),
            "instances": len(live),
            "logical_links": len(links),
            "mesh_edges": len(expected["edges"]),
            "stale_keys": 0,
        },
    }


def demo_params_snapshot() -> Dict[str, Any]:
    """Shape of run/param.sh so the params page is not blank under --demo."""
    rows = [
        {
            "name": "TFBFilePlayer-0",
            "key": "parameters:TFBFilePlayer-0",
            "kind": "instance",
            "live": True,
            "leftover": False,
            "fields": {
                "in-file": "/demo/tfb.dat",
                "wait": "0",
                "max-iterations": "0",
                "split": "2",
                "recbe": "false",
            },
        },
        {
            "name": "LogicFilterGpu",
            "key": "parameters:LogicFilterGpu",
            "kind": "service",
            "live": True,
            "leftover": False,
            "fields": {
                "trigger-signals": "(0xc0a802a9 0 0) (0xc0a802a9 1 0) (0xc0a802a9 2 0) (0xc0a802a9 3 0)",
                "trigger-expression": "RPN 0 1 & 2 & 3 &",
                "trigger-width": "10",
            },
        },
        {
            "name": "LogicFilterGpu-0",
            "key": "parameters:LogicFilterGpu-0",
            "kind": "instance",
            "live": True,
            "leftover": False,
            "fields": {
                "data-suppress": "true",
                "remove-hb": "false",
                "split": "2",
                "poll-timeout": "1",
                "trigger-signals": "(0xc0a802a9 0 0) (0xc0a802a9 1 0) (0xc0a802a9 2 0) (0xc0a802a9 3 0)",
                "trigger-expression": "RPN 0 1 & 2 & 3 &",
                "trigger-width": "10",
            },
        },
        {
            "name": "FileSink-0",
            "key": "parameters:FileSink-0",
            "kind": "instance",
            "live": True,
            "leftover": False,
            "fields": {
                "multipart": "true",
                "prefix": "data/fltgpu/tf",
                "ext": ".dat",
            },
        },
    ]
    return {
        "ok": True,
        "uri": "demo://param.sh",
        "ts": time.time(),
        "elapsed_ms": 0,
        "demo": True,
        "params": rows,
        "counts": _param_counts(rows),
        "warnings": ["demo parameters (Redis empty or --demo); not live param.sh"],
    }


def dumps(snap: Dict[str, Any]) -> str:
    return json.dumps(snap, indent=2, sort_keys=False)
