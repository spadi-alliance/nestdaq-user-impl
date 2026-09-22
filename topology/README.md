# topology

Live view of nestdaq process connections. **Does not parse `topology.sh` / `run.sh`.**
After those scripts run, this tool reads the same Redis keys that `daq-webctl`
(`WebGui.cxx`) and `TopologyConfig.cxx` use, then draws the mesh.

nestdaq sources under `/home/online/nestdaq` are not modified.

## Why Redis, not the shell files

`topology.sh` and `param.sh` only write Redis. The real instance count is
assigned later when devices register

```
daq_service:<Service>:<Service>-N:presence
```

with a TTL (same scan `WebGui::PollState` uses). `autoSubChannel` then
creates one FairMQ sub-socket per **currently present** peer. Parsing the
shell would miss that, and would also keep drawing a previous run's
instance count.

## What is trusted vs ignored

| Layer | Redis | Role |
| --- | --- | --- |
| Live processes | DB0 `daq_service:<svc>:<id>:presence` | **Only** source of instance boxes |
| Logical topology | DB0 `daq_service:topology:endpoint:*` and `:link:*` | Service-level sockets / links from last `topology.sh` |
| Actual mesh | DB0 `…:channel:<name>:peer` of **present** instances | Connections after CONNECT |
| Params | DB2 `parameters:<id>` | Shown for present instances / service-level keys |

Leftovers are listed as **stale** and never become boxes or edges:

- `channel` / `socket` / `peer` of an instance with no presence
- peer entries pointing at a dead instance
- `parameters:Service-N` when `Service-N` is not present
- `service-instance-index` fields whose uuid is not in a live presence value

That is the “new config must not pick up the old run” rule. `topology.sh`
already deletes `daq_service:*` before rewriting; this viewer still filters
by presence so a half-cleared Redis cannot invent extra processes.

## How a large mesh is drawn

A full `N×M` crossing (LogicFilter×4 → TFS×4 = 16, or worse) is unreadable
if every line is always drawn.

- **1:1** (Sampler → STFBuilder): one line per pair, as in the sketch.
- **fan-in / fan-out**: converging / diverging lines.
- **mesh with ≤ 24 edges**: every line (TimeFrameBuilder → FileSink style).
- **mesh with > 24 edges**: a band labelled `N×M mesh · K links` plus
  `autoSubChannel` / routing. Check **draw every mesh line** to expand it.
- **click a box**: only that process’s edges stay visible, with subchannel
  indices and Redis params in the side panel.

Expected mesh (topology + current presence, `TopologyConfig` pairing) and
actual peer lists are compared. A mismatch is a warning (stale CONNECT
state or devices still negotiating).

## Run

```bash
# after Redis is up (run/init.sh) and topology.sh / param.sh / run-*.sh
python3 topology/serve.py
# http://127.0.0.1:8765/
```

```bash
python3 topology/serve.py --redis redis://127.0.0.1:6379
python3 topology/serve.py --dump          # JSON snapshot
python3 topology/serve.py --demo          # production-shaped demo, no Redis
```

The page polls `/api/snapshot` every second (same idea as `WebGui::PollState`,
without touching nestdaq or its websocket). If Redis has no topology and no
presence, a demo of `run_daid-daq-01` counts is shown so the layout can be
checked; a warning says it is not live.
