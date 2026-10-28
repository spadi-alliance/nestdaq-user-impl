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
- **mesh at or below `box above`** (default 128): every line.
- **mesh with more links than that number**: a band. Click the band to draw
  every line and list them. Click the band again to collapse it.
- **click a box**: only that process’s edges stay visible, with subchannel
  indices and Redis params in the side panel.

Expected mesh (topology + current presence, `TopologyConfig` pairing) and
actual peer lists are compared. A mismatch is a warning (stale CONNECT
state or devices still negotiating).

## Run

Settings are `topology/run/nestdaq.env`.

- `NESTDAQ` is the nestdaq install. This checkout uses `$HOME/nestdaq`.
- `REDIS_SERVER` is `host:port`. This checkout uses `127.0.0.1:6379`.
- `BIN_DIR` applies only to the example launcher `topology/run/start_device.sh`.
  Leave it empty to use this repository's `bin/`.

```bash
./topology/server.sh start
./topology/server.sh stop
```

`start` reads `nestdaq.env` and writes `topology/run/redis-server.txt`.

- Redis and daq-webctl are already running: `topology/run/setup.sh` writes the
  address file, then the page starts.
- Redis does not answer, daq-webctl is not running, or only one of them is
  running: `topology/run/init.sh` writes the same file and starts each of
  Redis, RedisInsight, and daq-webctl that is not already running. Then the
  page starts.

Open one of the printed URLs. The same server serves `/guide`.

`stop` stops the viewer. Redis and the device processes keep running.
After changing `REDIS_SERVER`, stop and start again so the page reads the new address.

```bash
python3 topology/serve.py --redis redis://127.0.0.1:6379   # this process only
python3 topology/serve.py --dump                            # JSON snapshot
```

The page polls `/api/snapshot` every second (same idea as `WebGui::PollState`,
without touching nestdaq or its websocket). Empty Redis shows an empty graph
and a warning. A box appears when a running process registers in this Redis.
