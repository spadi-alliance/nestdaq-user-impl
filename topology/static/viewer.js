/* Layered topology viewer. Layout follows the pipeline; mesh is bundled
   when N*M is large. Click a box to see only that process's connections. */

const COL_W = 240;
const COL_GAP = 64;
const ROW_H = 48;
const BOX_W = 176;
const BOX_H = 28;
const LABEL_PAD_X = 16;
const LEFT = 20;
const TOP = 56;

let _labelCtx = null;
function labelWidth(s) {
  if (!_labelCtx) {
    const c = document.createElement("canvas");
    _labelCtx = c.getContext("2d");
  }
  _labelCtx.font = '12px "Segoe UI", "Helvetica Neue", Arial, sans-serif';
  return _labelCtx.measureText(String(s || "")).width;
}

function boxWidthFor(ids) {
  let w = BOX_W;
  for (const id of ids || []) {
    w = Math.max(w, Math.ceil(labelWidth(id) + LABEL_PAD_X));
  }
  return w;
}

let snap = null;
let focusId = null;
let focusMesh = null;
let pollGen = 0;
let showWarnings = false;

function meshKey(grp) {
  return `${grp.from_service}:${grp.from_channel}>${grp.to_service}:${grp.to_channel}`;
}

function $(id) { return document.getElementById(id); }

const MAIN_RANK = {
  Sampler: 0,
  AmQStrTdcSampler: 0,
  TFBFilePlayer: 0,
  STFBuilder: 1,
  TimeFrameBuilder: 2,
  LogicFilter: 3,
  LogicFilterGpu: 3,
  TimeFrameSlicerByLogicTiming: 4,
  FileSink: 5,
};

const SIDE_RANK = {
  Scaler: 0,
  DecSink: 1,
};

function isSideChannel(ch) {
  const c = String(ch || "").toLowerCase();
  return !!c && c !== "out";
}

function sideServiceNames(s) {
  const names = new Set(Object.keys(SIDE_RANK));
  for (const lk of (s && s.links) || []) {
    if (isSideChannel(lk.from_channel)) names.add(lk.to_service);
  }
  return names;
}

function isSide(name, sideSet) {
  return sideSet.has(name);
}

function stagesOf(snap, sideSet) {
  const names = (snap.services || []).filter((s) => !isSide(s.name, sideSet)).map((s) => s.name);
  const incoming = new Map(names.map((n) => [n, 0]));
  const outgoing = new Map(names.map((n) => [n, []]));
  for (const lk of snap.links || []) {
    if (isSide(lk.from_service, sideSet) || isSide(lk.to_service, sideSet)) continue;
    incoming.set(lk.to_service, (incoming.get(lk.to_service) || 0) + 1);
    (outgoing.get(lk.from_service) || []).push(lk.to_service);
  }
  const dist = new Map();
  const q = names.filter((n) => (incoming.get(n) || 0) === 0)
    .sort((a, b) => (MAIN_RANK[a] ?? 50) - (MAIN_RANK[b] ?? 50));
  q.forEach((n) => dist.set(n, 0));
  if (!q.length) names.forEach((n, i) => dist.set(n, i));
  const seen = new Set(q);
  while (q.length) {
    const u = q.shift();
    for (const v of outgoing.get(u) || []) {
      dist.set(v, Math.max(dist.get(v) || 0, (dist.get(u) || 0) + 1));
      if (!seen.has(v)) {
        seen.add(v);
        q.push(v);
      }
    }
  }
  names.forEach((n) => {
    if (!dist.has(n)) dist.set(n, MAIN_RANK[n] ?? 0);
  });
  return dist;
}

function instanceState(snap, id) {
  const inst = (snap.instances || []).find((x) => x.id === id);
  return inst && inst.state ? String(inst.state).trim() : "";
}

function stateClass(state) {
  const s = String(state || "").toUpperCase().replace(/[\s_-]/g, "");
  return {
    IDLE: "idle",
    INITIALIZINGDEVICE: "init-device",
    INITIALIZED: "device-ready",
    DEVICEREADY: "device-ready",
    INITIALIZINGTASK: "init-task",
    READY: "ready",
    RUNNING: "running",
    RESETTINGTASK: "reset-task",
    RESETTINGDEVICE: "reset-device",
    EXITING: "exiting",
    ERROR: "error",
  }[s] || "idle";
}

function isLinkedState(state) {
  const s = String(state || "").toUpperCase().replace(/[\s_-]/g, "");
  return ["DEVICEREADY", "INITIALIZINGTASK", "READY", "RUNNING", "RESETTINGTASK"].includes(s);
}

function placeGroup(pos, svc, x, y, snap, boxW) {
  const ids = svc.instances || [];
  const w = boxW || boxWidthFor(ids);
  svc._x = x;
  svc._w = w;
  if (!ids.length) {
    svc._y0 = y;
    svc._y1 = y;
    return y;
  }
  svc._y0 = y;
  ids.forEach((id, i) => {
    pos.set(id, {
      x, y: y + i * ROW_H, w, h: BOX_H,
      service: svc.name,
      live: (svc.live_ids || svc.instances).includes(id),
      state: instanceState(snap, id),
    });
  });
  const bottom = y + Math.max(ids.length, 1) * ROW_H;
  svc._y1 = bottom;
  return bottom;
}

function layout(snap) {
  const sideSet = sideServiceNames(snap);
  const dist = stagesOf(snap, sideSet);
  const services = snap.services || [];
  const main = services.filter((s) => !isSide(s.name, sideSet))
    .sort((a, b) => (MAIN_RANK[a.name] ?? 50) - (MAIN_RANK[b.name] ?? 50)
      || a.name.localeCompare(b.name));
  const side = services.filter((s) => isSide(s.name, sideSet))
    .sort((a, b) => (SIDE_RANK[a.name] ?? 50) - (SIDE_RANK[b.name] ?? 50)
      || a.name.localeCompare(b.name));

  const cols = new Map();
  for (const svc of main) {
    const c = dist.get(svc.name) || 0;
    if (!cols.has(c)) cols.set(c, []);
    cols.get(c).push(svc);
  }
  const pos = new Map();
  let maxY = TOP;
  let maxX = 0;
  const colXs = new Map();
  let x = LEFT;
  for (const [c, svcs] of [...cols.entries()].sort((a, b) => a[0] - b[0])) {
    const boxW = Math.max(BOX_W, ...svcs.map((s) => boxWidthFor(s.instances)));
    colXs.set(c, x);
    let y = TOP;
    svcs.sort((a, b) => (MAIN_RANK[a.name] ?? 50) - (MAIN_RANK[b.name] ?? 50));
    for (const svc of svcs) {
      y = placeGroup(pos, svc, x, y, snap, boxW) + 24;
    }
    maxY = Math.max(maxY, y);
    maxX = Math.max(maxX, x + boxW);
    x += boxW + COL_GAP;
  }

  const nextYAt = new Map();
  for (const svc of side) {
    const srcLink = (snap.links || []).find((l) => l.to_service === svc.name);
    const srcSvc = srcLink && snap.services.find((s) => s.name === srcLink.from_service);
    const sx = (srcSvc && srcSvc._x != null) ? srcSvc._x : LEFT + COL_GAP;
    const boxW = boxWidthFor(svc.instances);
    const under = (srcSvc && srcSvc._y1 != null) ? srcSvc._y1 + ROW_H : TOP + ROW_H;
    const y = Math.max(under, nextYAt.get(sx) || 0);
    const bottom = placeGroup(pos, svc, sx, y, snap, boxW);
    nextYAt.set(sx, bottom + ROW_H);
    maxY = Math.max(maxY, bottom);
    maxX = Math.max(maxX, sx + boxW);
  }
  return { pos, maxX, maxY, colXs };
}

function laneOffset(i, n, halfWidth, gap) {
  if (n <= 1) return 0;
  const step0 = gap > 0 ? gap : 12;
  const step = (n - 1) * step0 > 2 * halfWidth ? (2 * halfWidth) / (n - 1) : step0;
  return (i - (n - 1) / 2) * step;
}

function path(a, b, kind, lane) {
  const n = lane && lane.n > 0 ? lane.n : 1;
  const i = lane && lane.i >= 0 ? lane.i : 0;
  const below = b.y > a.y + a.h - 4 && Math.abs(a.x - b.x) < Math.max(a.w, b.w, BOX_W) * 0.6;
  if (below) {
    const x1 = a.x + a.w / 2 + laneOffset(i, n, (a.w - 16) / 2, 12);
    const x2 = b.x + b.w / 2 + laneOffset(i, n, (b.w - 16) / 2, 12);
    return `M ${x1} ${a.y + a.h} L ${x2} ${b.y}`;
  }
  const x1 = a.x + a.w;
  const y1 = a.y + a.h / 2;
  const x2 = b.x;
  const y2 = b.y + b.h / 2;
  if (kind === "mesh") {
    return `M ${x1} ${y1} L ${x2} ${y2}`;
  }
  if (Math.abs(y1 - y2) < 1.5) {
    return `M ${x1} ${y1} L ${x2} ${y2}`;
  }
  const gap = 18;
  const mx = (x2 > x1 + gap * 2)
    ? (x1 + (x2 - x1) / 2 + laneOffset(i, n, (x2 - x1) / 2 - gap, 12))
    : (x1 + gap);
  return `M ${x1} ${y1} L ${mx} ${y1} L ${mx} ${y2} L ${x2} ${y2}`;
}

function render(snap) {
  const svg = $("graph");
  const { pos, maxX, maxY } = layout(snap);
  const W = Math.max(maxX + 40, 720);
  const H = Math.max(maxY + 48, 360);
  const pane = $("graph-scroll");
  const avail = pane ? pane.clientWidth : 0;
  const scale = avail > 0 ? Math.min(1, avail / W) : 1;
  svg.setAttribute("viewBox", `0 0 ${W} ${H}`);
  svg.setAttribute("width", String(Math.round(W * scale)));
  svg.setAttribute("height", String(Math.round(H * scale)));
  svg.setAttribute("preserveAspectRatio", "xMinYMin meet");
  svg.style.width = Math.round(W * scale) + "px";
  svg.style.height = Math.round(H * scale) + "px";
  svg.innerHTML = "";

  const edges = snap.edges || [];
  const groups = snap.groups || [];

  const gBand = el(svg, "g");
  const gEdge = el(svg, "g");
  const gNode = el(svg, "g");
  const gAnno = el(svg, "g");

  const sortEdges = (list) => list.slice().sort((e1, e2) => {
    const dy = (pos.get(e1.from_id)?.y || 0) - (pos.get(e2.from_id)?.y || 0);
    if (dy) return dy;
    return (pos.get(e1.to_id)?.y || 0) - (pos.get(e2.to_id)?.y || 0);
  });
  const drawEdge = (e, grp, lane) => {
    const a = pos.get(e.from_id);
    const b = pos.get(e.to_id);
    if (!a || !b) return;
    const kind = (grp.kind === "mesh" || e.autoSubChannel) ? "mesh" : grp.kind;
    const linked = isLinkedState(a.state) && isLinkedState(b.state);
    const st = !linked ? " logical"
      : (stateClass(a.state) === "running" && stateClass(b.state) === "running"
        ? " running"
        : (stateClass(a.state) === "error" || stateClass(b.state) === "error" ? " error" : ""));
    const p = el(gEdge, "path", {
      d: path(a, b, kind, lane),
      class: "edge" + (e.autoSubChannel ? " auto" : "") + st,
    });
    p.dataset.from = e.from_id;
    p.dataset.to = e.to_id;
    p.dataset.mesh = meshKey(grp);
    p.dataset.info = `${e.from_id}.${e.from_channel}[${e.from_sub}] → ${e.to_id}.${e.to_channel}[${e.to_sub}]`;
  };

  const toggleMesh = (key) => {
    focusMesh = (focusMesh === key) ? null : key;
    if (focusMesh) focusId = null;
    render(snap);
  };

  for (const grp of groups) {
    const fromIds = (snap.services.find((s) => s.name === grp.from_service) || {}).instances || [];
    const toIds = (snap.services.find((s) => s.name === grp.to_service) || {}).instances || [];
    if (!fromIds.length || !toIds.length) continue;

    const ge = edges.filter((e) =>
      e.from_service === grp.from_service && e.to_service === grp.to_service
      && e.from_channel === grp.from_channel && e.to_channel === grp.to_channel
    );
    const bundle = grp.bundle;
    const key = meshKey(grp);
    const expanded = focusMesh === key;
    if (bundle && ge.length) {
      const a0 = pos.get(fromIds[0]);
      const a1 = pos.get(fromIds[fromIds.length - 1]);
      const b0 = pos.get(toIds[0]);
      const b1 = pos.get(toIds[toIds.length - 1]);
      if (a0 && b0) {
        const x = a0.x + a0.w + 8;
        const w = b0.x - x - 8;
        const y = Math.min(a0.y, b0.y) - 6;
        const h = Math.max(a1.y + BOX_H, b1.y + BOX_H) - y + 6;
        const r = el(gBand, "rect", {
          x, y, width: w, height: h, rx: 8,
          class: expanded ? "band band-open" : "band",
        });
        r.dataset.mesh = key;
        r.style.cursor = "pointer";
        r.addEventListener("click", () => toggleMesh(key));
        const lx = x + w / 2;
        const ly = Math.min(a0.y, b0.y) - 8;
        const caption = expanded
          ? `${grp.n_from}×${grp.n_to} · ${ge.length}  all`
          : `${grp.n_from}×${grp.n_to} · ${ge.length}`;
        const tl = text(gAnno, lx, ly - 12, caption, "band-label");
        tl.setAttribute("text-anchor", "middle");
        tl.style.cursor = "pointer";
        tl.addEventListener("click", () => toggleMesh(key));
        if (grp.routing) {
          const t2 = text(gAnno, lx, ly, String(grp.routing).replace(/\s*\([^)]*\)\s*/g, "").trim(), "kind-label");
          t2.setAttribute("text-anchor", "middle");
          t2.style.cursor = "pointer";
          t2.addEventListener("click", () => toggleMesh(key));
        }
      }
      const ordered = sortEdges(ge);
      if (expanded) {
        ordered.forEach((e, i) => drawEdge(e, grp, { i, n: ordered.length }));
      } else {
        for (const e of ordered) {
          if (focusId && (e.from_id === focusId || e.to_id === focusId)) {
            drawEdge(e, grp, { i: ordered.indexOf(e), n: ordered.length });
          }
        }
      }
      continue;
    }
    const ordered = sortEdges(ge);
    ordered.forEach((e, i) => drawEdge(e, grp, { i, n: ordered.length }));
  }

  for (const [id, p] of pos) {
    const g = el(gNode, "g", { class: "nodeg", transform: `translate(${p.x},${p.y})` });
    const scaler = p.service === "Scaler";
    const st = stateClass(p.state);
    const r = el(g, "rect", {
      width: p.w, height: p.h, rx: 2,
      class: "node" + (p.live ? "" : " dead") + (scaler ? " scaler" : "") + (st ? " " + st : ""),
    });
    r.dataset.id = id;
    r.dataset.service = p.service;
    const lb = text(g, 8, 18, id, "node-label" + (scaler ? " scaler" : ""));
    lb.setAttribute("data-id", id);
    const tip = document.createElementNS("http://www.w3.org/2000/svg", "title");
    tip.textContent = id;
    g.appendChild(tip);
    r.addEventListener("click", () => {
      focusId = (focusId === id) ? null : id;
      focusMesh = null;
      render(snap);
    });
  }

  applyFocus();
  $("stats").innerHTML = summaryHtml(snap);
  if (focusId) showDetail(focusId);
  else if (focusMesh) showMeshDetail(focusMesh);
  else showOverview();
}

function applyFocus() {
  document.querySelectorAll(".edge").forEach((p) => {
    if (focusMesh || !focusId) {
      p.classList.remove("dim");
      return;
    }
    const hit = p.dataset.from === focusId || p.dataset.to === focusId;
    p.classList.toggle("dim", !hit);
  });
  document.querySelectorAll(".node").forEach((n) => {
    const id = n.dataset.id;
    n.classList.toggle("focused", id === focusId);
    n.classList.toggle("dim", !!(!focusMesh && focusId && id !== focusId));
  });
}

function currentView() {
  const p = location.pathname.replace(/\/+$/, "") || "/";
  if (p === "/params") return "params";
  if (p === "/daq") return "daq";
  if (p === "/edit") return "edit";
  if (p === "/scripts") return "scripts";
  return "topology";
}

function isParamsView() {
  return currentView() === "params";
}

function applyView() {
  const view = currentView();
  const titles = {
    params: ["nestdaq params", "params <span>Redis DB2 · param.sh</span>"],
    daq: ["nestdaq daq", "daq <span>daq-webctl websocket</span>"],
    edit: ["nestdaq edit", "edit <span>write topology from bin/</span>"],
    scripts: ["nestdaq scripts", "scripts <span>param / topology / run</span>"],
    topology: ["nestdaq topology", "topology <span>live from Redis</span>"],
  };
  document.title = titles[view][0];
  $("page-title").innerHTML = titles[view][1];
  ["nav-topo", "topology", "nav-edit", "edit", "nav-scripts", "scripts", "nav-params", "params", "nav-daq", "daq"].forEach((id, i, a) => {
    if (i % 2) return;
    const n = $(id);
    if (n) n.classList.toggle("active", view === a[i + 1]);
  });
  $("stage").hidden = view !== "topology";
  $("view-params").hidden = view !== "params";
  const editView = $("view-edit");
  if (editView) editView.hidden = view !== "edit";
  const scriptsView = $("view-scripts");
  if (scriptsView) scriptsView.hidden = view !== "scripts";
  const daqView = $("view-daq");
  if (daqView) daqView.hidden = view !== "daq";
  if (view === "scripts" && typeof window.showScriptsView === "function") {
    window.showScriptsView();
  }
  $("toolbar-params").hidden = view !== "params";
  const tbTopo = $("toolbar-topo");
  if (tbTopo) tbTopo.hidden = view !== "topology";
  applyWarnlist();
}

function applyWarnlist() {
  const box = $("warnlist");
  if (box) box.hidden = !showWarnings;
}

function showView(name, push) {
  const dest = name === "params" ? "/params" : name === "daq" ? "/daq" : name === "edit" ? "/edit" : name === "scripts" ? "/scripts" : "/";
  if (push && location.pathname.replace(/\/+$/, "") !== dest.replace(/\/+$/, "")) {
    history.pushState({ view: name }, "", dest);
  }
  applyView();
  poll();
}

function paramsSummaryHtml(data) {
  const c = data.counts || {};
  const pills = [
    `<span class="pill info">${c.keys || 0} keys</span>`,
    `<span class="pill info">${c.fields || 0} fields</span>`,
    `<span class="pill info">${c.live || 0} with presence</span>`,
  ];
  if (c.leftover) pills.push(`<span class="pill stale">${c.leftover} leftover (no presence)</span>`);
  if ((data.warnings || []).length) pills.push(`<span class="pill warn">${data.warnings.length} warnings</span>`);
  if (data.ok) pills.push(`<span class="pill ok">redis live</span>`);
  return pills.join(" ");
}

function renderParams(data) {
  const root = $("params-root");
  const q = (($("params-filter") && $("params-filter").value) || "").trim().toLowerCase();
  const rows = (data.params || []).filter((p) => {
    if (!q) return true;
    if (p.name.toLowerCase().includes(q) || p.key.toLowerCase().includes(q)) return true;
    return Object.entries(p.fields || {}).some(([k, v]) =>
      k.toLowerCase().includes(q) || String(v).toLowerCase().includes(q));
  });
  $("stats").innerHTML = paramsSummaryHtml(data);
  if (!rows.length) {
    root.innerHTML = `<p class="param-empty">${
      (data.params || []).length ? "no keys match the filter" : "no parameters:* in Redis DB2 — run param.sh"
    }</p>`;
    return;
  }
  root.innerHTML = rows.map((p) => {
    const fieldRows = Object.keys(p.fields || {}).map((k) =>
      `<tr><th>${esc(k)}</th><td>${esc(p.fields[k])}</td></tr>`
    ).join("") || `<tr><td colspan="2">empty hash</td></tr>`;
    const pills = [
      `<span class="pill info">${esc(p.kind)}</span>`,
      p.live ? `<span class="pill ok">presence</span>` : `<span class="pill info">no presence</span>`,
    ];
    if (p.leftover) pills.push(`<span class="pill stale">leftover</span>`);
    return `<section class="param-card${p.leftover ? " leftover" : ""}">
      <div class="param-head">
        <h2>${esc(p.key)}</h2>
        <div>${pills.join(" ")}</div>
      </div>
      <table>${fieldRows}</table>
    </section>`;
  }).join("");
}

function summaryHtml(snap) {
  const c = snap.counts || {};
  const pills = [
    `<span class="pill info">${c.instances || 0} processes</span>`,
    `<span class="pill info">${c.mesh_edges || 0} mesh links</span>`,
    `<span class="pill info">source ${snap.source}</span>`,
  ];
  if ((snap.stale || []).length) pills.push(`<span class="pill stale">${snap.stale.length} stale keys ignored</span>`);
  if ((snap.warnings || []).length) pills.push(`<span class="pill warn">${snap.warnings.length} warnings</span>`);
  if (snap.ok) pills.push(`<span class="pill ok">redis live</span>`);
  return pills.join(" ");
}

function showOverview() {
  const box = $("detail");
  if (!snap) { box.hidden = true; return; }
  box.hidden = false;
  const staleRows = (snap.stale || []).slice(0, 20).map((s) =>
    `<tr><td colspan="2">${esc(s.key)}</td></tr><tr><td></td><td>${esc(s.reason)}</td></tr>`
  ).join("");
  const grpRows = (snap.groups || []).map((g) =>
    `<tr><td>${esc(g.from_service)}.${esc(g.from_channel)} → ${esc(g.to_service)}.${esc(g.to_channel)}</td>
     <td>${g.kind} · ${g.n_edges} · ${g.autoSubChannel ? "autoSubChannel" : ""}</td></tr>`
  ).join("");
  box.innerHTML = `
    <h2>topology</h2>
    <div>${esc(snap.uri)}</div>
    <table>${grpRows}</table>
    ${showWarnings && (snap.warnings || []).length ? `<h2>warnings</h2><ul>${snap.warnings.map((w) => `<li>${esc(w)}</li>`).join("")}</ul>` : ""}
    ${(snap.stale || []).length ? `<h2>stale (ignored)</h2><table>${staleRows}</table>` : ""}
    <ul class="state-legend">
      <li><i class="idle"></i>idle</li>
      <li><i class="init-device"></i>connect</li>
      <li><i class="device-ready"></i>device ready</li>
      <li><i class="init-task"></i>init task</li>
      <li><i class="ready"></i>ready</li>
      <li><i class="running"></i>running</li>
      <li><i class="reset-task"></i>reset task</li>
      <li><i class="reset-device"></i>reset device</li>
      <li><i class="error"></i>error</li>
    </ul>
  `;
}

function showMeshDetail(key) {
  const box = $("detail");
  if (!snap || !key) { box.hidden = true; return; }
  box.hidden = false;
  const grp = (snap.groups || []).find((g) => meshKey(g) === key);
  const edges = (snap.edges || []).filter((e) => meshKey({
    from_service: e.from_service,
    from_channel: e.from_channel,
    to_service: e.to_service,
    to_channel: e.to_channel,
  }) === key);
  const rows = edges.map((e) =>
    `<tr><td colspan="2">${esc(e.from_id)}.${esc(e.from_channel)}[${e.from_sub}] → ${esc(e.to_id)}.${esc(e.to_channel)}[${e.to_sub}]</td></tr>`
  ).join("");
  box.innerHTML = `
    <h2>mesh ${esc(grp ? `${grp.from_service} → ${grp.to_service}` : key)}</h2>
    <table>
      <tr><th>edges</th><td>${edges.length}</td></tr>
      <tr><th>size</th><td>${grp ? `${grp.n_from}×${grp.n_to}` : ""}</td></tr>
      <tr><th>routing</th><td>${esc(grp && grp.routing || "")}</td></tr>
    </table>
    <h2>all mesh lines (${edges.length})</h2>
    <table>${rows || "<tr><td>none</td></tr>"}</table>
    <p class="kind-label">click the mesh band again to collapse</p>
  `;
}

function showDetail(id) {
  const box = $("detail");
  box.hidden = false;
  const inst = (snap.instances || []).find((x) => x.id === id);
  const svc = (snap.services || []).find((s) => (s.instances || []).includes(id) || s.name === id);
  const edges = (snap.edges || []).filter((e) => e.from_id === id || e.to_id === id);
  const params = Object.assign({}, (svc && svc.params) || {}, (snap.params || {})[id] || {});
  const paramRows = Object.keys(params).map((k) =>
    `<tr><th>${esc(k)}</th><td>${esc(params[k])}</td></tr>`
  ).join("");
  const edgeRows = edges.map((e) =>
    `<tr><td colspan="2">${esc(e.from_id)}.${esc(e.from_channel)}[${e.from_sub}] → ${esc(e.to_id)}.${esc(e.to_channel)}[${e.to_sub}]</td></tr>`
  ).join("");
  box.innerHTML = `
    <h2>${esc(id)}</h2>
    <table>
      <tr><th>service</th><td>${esc(inst ? inst.service : (svc && svc.name) || "")}</td></tr>
      <tr><th>state</th><td>${esc(inst ? inst.state : "no presence")}</td></tr>
      <tr><th>ttl</th><td>${inst ? inst.ttl : ""}</td></tr>
      <tr><th>host</th><td>${esc(inst ? (inst.host + " " + inst.ip) : "")}</td></tr>
      <tr><th>routing</th><td>${esc(svc && svc.routing || "")}</td></tr>
    </table>
    <h2>this process mesh (${edges.length})</h2>
    <table>${edgeRows || "<tr><td>none</td></tr>"}</table>
    <h2>params</h2>
    <table>${paramRows || "<tr><td>none</td></tr>"}</table>
    <p class="kind-label">click again to clear focus</p>
  `;
}

function el(parent, tag, attrs) {
  const n = document.createElementNS("http://www.w3.org/2000/svg", tag);
  if (attrs) {
    for (const [k, v] of Object.entries(attrs)) n.setAttribute(k, v);
  }
  parent.appendChild(n);
  return n;
}

function text(parent, x, y, value, cls) {
  const n = el(parent, "text", { x, y, class: cls || "" });
  n.textContent = value;
  return n;
}

function esc(s) {
  return String(s == null ? "" : s)
    .replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
}

async function poll() {
  const gen = ++pollGen;
  const view = currentView();
  const wantParams = view === "params";
  if (view === "daq") {
    if (DaqCtl.lastSummary) renderDaqTables(DaqCtl.lastSummary);
    return;
  }
  if (view === "edit" || view === "scripts") return;
  try {
    if (wantParams) {
      const r = await fetch("/api/params", { cache: "no-store" });
      const data = await r.json();
      if (gen !== pollGen || !isParamsView()) return;
      $("clock").textContent = new Date(data.ts * 1000).toLocaleTimeString();
      renderParams(data);
      $("warnlist").innerHTML = (data.warnings || []).map((w) => `<li>${esc(w)}</li>`).join("");
      applyWarnlist();
      return;
    }
    const r = await fetch("/api/snapshot", { cache: "no-store" });
    const data = await r.json();
    if (gen !== pollGen || currentView() !== "topology") return;
    snap = data;
    $("clock").textContent = new Date(snap.ts * 1000).toLocaleTimeString();
    render(snap);
    $("warnlist").innerHTML = (snap.warnings || []).map((w) => `<li>${esc(w)}</li>`).join("");
    applyWarnlist();
  } catch (err) {
    if (gen !== pollGen) return;
    $("clock").textContent = "redis/viewer error";
    console.error(err);
  }
}

window.addEventListener("DOMContentLoaded", () => {
  applyView();
  const togWarn = $("tog-warnings");
  if (togWarn) {
    togWarn.addEventListener("change", (ev) => {
      showWarnings = ev.target.checked;
      applyWarnlist();
      if (snap && currentView() === "topology" && !focusId && !focusMesh) showOverview();
    });
  }
  $("params-filter").addEventListener("input", () => {
    if (isParamsView()) poll();
  });
  $("nav-params").addEventListener("click", (ev) => {
    ev.preventDefault();
    showView("params", true);
  });
  $("nav-topo").addEventListener("click", (ev) => {
    ev.preventDefault();
    showView("topology", true);
  });
  const navEdit = $("nav-edit");
  if (navEdit) {
    navEdit.addEventListener("click", (ev) => {
      ev.preventDefault();
      showView("edit", true);
    });
  }
  const navScripts = $("nav-scripts");
  if (navScripts) {
    navScripts.addEventListener("click", (ev) => {
      ev.preventDefault();
      showView("scripts", true);
    });
  }
  const navDaq = $("nav-daq");
  if (navDaq) {
    navDaq.addEventListener("click", (ev) => {
      ev.preventDefault();
      showView("daq", true);
    });
  }
  window.addEventListener("popstate", () => {
    applyView();
    poll();
  });
  poll();
  setInterval(poll, 1000);
  try {
    if (typeof initDaqCtl === "function") initDaqCtl();
  } catch (err) {
    console.error(err);
  }
});
