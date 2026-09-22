/* Layered topology viewer. Layout follows the pipeline; mesh is bundled
   when N*M is large. Click a box to see only that process's connections. */

const COL_W = 300;
const ROW_H = 40;
const BOX_W = 200;
const BOX_H = 28;
const LEFT = 40;
const TOP = 56;

let snap = null;
let focusId = null;
let showAllMesh = false;
let showExpected = false;
let forceDemo = false;
let pollGen = 0;

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

function isSide(name) {
  return Object.prototype.hasOwnProperty.call(SIDE_RANK, name);
}

function stagesOf(snap) {
  const names = snap.services.filter((s) => !isSide(s.name)).map((s) => s.name);
  const incoming = new Map(names.map((n) => [n, 0]));
  const outgoing = new Map(names.map((n) => [n, []]));
  for (const lk of snap.links) {
    if (isSide(lk.from_service) || isSide(lk.to_service)) continue;
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

function placeGroup(pos, svc, x, y) {
  const ids = svc.instances.length ? svc.instances : [`${svc.name} (no process)`];
  svc._x = x;
  svc._y0 = y;
  ids.forEach((id, i) => {
    pos.set(id, {
      x, y: y + i * ROW_H, w: BOX_W, h: BOX_H,
      service: svc.name,
      live: (svc.live_ids || svc.instances).includes(id),
    });
  });
  const bottom = y + Math.max(ids.length, 1) * ROW_H;
  svc._y1 = bottom;
  return bottom;
}

function layout(snap) {
  const dist = stagesOf(snap);
  const main = snap.services.filter((s) => !isSide(s.name))
    .sort((a, b) => (MAIN_RANK[a.name] ?? 50) - (MAIN_RANK[b.name] ?? 50)
      || a.name.localeCompare(b.name));
  const side = snap.services.filter((s) => isSide(s.name))
    .sort((a, b) => (SIDE_RANK[a.name] ?? 50) - (SIDE_RANK[b.name] ?? 50));

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
  for (const [c, svcs] of [...cols.entries()].sort((a, b) => a[0] - b[0])) {
    const x = LEFT + c * COL_W;
    colXs.set(c, x);
    let y = TOP;
    svcs.sort((a, b) => (MAIN_RANK[a.name] ?? 50) - (MAIN_RANK[b.name] ?? 50));
    for (const svc of svcs) {
      y = placeGroup(pos, svc, x, y) + 24;
    }
    maxY = Math.max(maxY, y);
    maxX = Math.max(maxX, x + BOX_W);
  }

  let sideY = maxY + 28;
  for (const svc of side) {
    const srcLink = (snap.links || []).find((l) => l.to_service === svc.name);
    const srcSvc = srcLink && snap.services.find((s) => s.name === srcLink.from_service);
    const x = (srcSvc && srcSvc._x != null) ? srcSvc._x : (LEFT + COL_W);
    const bottom = placeGroup(pos, svc, x, sideY);
    sideY = bottom + 24;
    maxY = Math.max(maxY, sideY);
    maxX = Math.max(maxX, x + BOX_W);
  }
  return { pos, maxX, maxY, colXs };
}

function path(a, b, kind) {
  const x1 = a.x + a.w;
  const y1 = a.y + a.h / 2;
  const x2 = b.x;
  const y2 = b.y + b.h / 2;
  // autoSubChannel mesh: straight crossings so N×M is visible
  if (kind === "mesh") {
    return `M ${x1} ${y1} L ${x2} ${y2}`;
  }
  if (Math.abs(y1 - y2) < 1.5) {
    return `M ${x1} ${y1} L ${x2} ${y2}`;
  }
  // 1:1 / fan: ㄴ-shaped orthogonal
  const gap = 18;
  const mx = (x2 > x1 + gap * 2) ? (x1 + (x2 - x1) / 2) : (x1 + gap);
  return `M ${x1} ${y1} L ${mx} ${y1} L ${mx} ${y2} L ${x2} ${y2}`;
}

function render(snap) {
  const svg = $("graph");
  const { pos, maxX, maxY } = layout(snap);
  const W = Math.max(maxX + 360, 900);
  const H = Math.max(maxY + 80, 400);
  svg.setAttribute("viewBox", `0 0 ${W} ${H}`);
  svg.setAttribute("width", "100%");
  svg.setAttribute("preserveAspectRatio", "xMinYMin meet");
  svg.style.height = Math.min(H, 720) + "px";
  svg.innerHTML = "";

  const useExpected = showExpected && snap.expected_edges && snap.expected_edges.length;
  const edges = useExpected ? snap.expected_edges : snap.edges;
  const groups = snap.groups || [];

  const gBand = el(svg, "g");
  const gEdge = el(svg, "g");
  const gNode = el(svg, "g");
  const gAnno = el(svg, "g");

  for (const grp of groups) {
    const fromIds = (snap.services.find((s) => s.name === grp.from_service) || {}).instances || [];
    const toIds = (snap.services.find((s) => s.name === grp.to_service) || {}).instances || [];
    const fromPlace = fromIds.length ? fromIds : [`${grp.from_service} (no process)`];
    const toPlace = toIds.length ? toIds : [`${grp.to_service} (no process)`];
    const auto = grp.autoSubChannel || (snap.endpoints || []).some((e) =>
      e.service === grp.from_service && e.channel === grp.from_channel && e.autoSubChannel);
    const p0 = pos.get(fromPlace[0]);
    if (p0 && auto) {
      const t = text(gAnno, p0.x, p0.y - 14,
        `${grp.from_service}.${grp.from_channel} → autoSubChannel true`,
        "anno");
      t.setAttribute("text-anchor", "start");
    }

    const bundle = grp.bundle && !showAllMesh;
    if (bundle) {
      const a0 = pos.get(fromPlace[0]);
      const a1 = pos.get(fromPlace[fromPlace.length - 1]);
      const b0 = pos.get(toPlace[0]);
      const b1 = pos.get(toPlace[toPlace.length - 1]);
      if (a0 && b0) {
        const x = a0.x + BOX_W + 8;
        const w = b0.x - x - 8;
        const y = Math.min(a0.y, b0.y) - 6;
        const h = Math.max(a1.y + BOX_H, b1.y + BOX_H) - y + 6;
        const r = el(gBand, "rect", {
          x, y, width: w, height: h, rx: 8, class: "band",
        });
        r.dataset.group = `${grp.from_service}:${grp.to_service}`;
        const label = `${grp.n_from}×${grp.n_to} mesh · ${grp.n_edges} links`;
        const tl = text(gAnno, x + w / 2, y + h / 2 - 4, label, "band-label");
        tl.setAttribute("text-anchor", "middle");
        if (grp.routing) {
          const t2 = text(gAnno, x + w / 2, y + h / 2 + 12, grp.routing, "kind-label");
          t2.setAttribute("text-anchor", "middle");
        }
      }
      continue;
    }

    const ge = edges.filter((e) =>
      e.from_service === grp.from_service && e.to_service === grp.to_service
      && e.from_channel === grp.from_channel && e.to_channel === grp.to_channel
    );
    if (!ge.length) {
      const a = pos.get(fromPlace[0]);
      const b = pos.get(toPlace[0]);
      if (a && b) {
        const p = el(gEdge, "path", { d: path(a, b, grp.kind), class: "edge logical" });
        p.dataset.from = fromPlace[0];
        p.dataset.to = toPlace[0];
        p.dataset.info = `${grp.from_service}.${grp.from_channel} → ${grp.to_service}.${grp.to_channel} (configured, no presence)`;
      }
      continue;
    }
    for (const e of ge) {
      const a = pos.get(e.from_id);
      const b = pos.get(e.to_id);
      if (!a || !b) continue;
      const kind = (grp.kind === "mesh" || e.autoSubChannel) ? "mesh" : grp.kind;
      const p = el(gEdge, "path", { d: path(a, b, kind), class: "edge" + (e.autoSubChannel ? " auto" : "") });
      p.dataset.from = e.from_id;
      p.dataset.to = e.to_id;
      p.dataset.info = `${e.from_id}.${e.from_channel}[${e.from_sub}] → ${e.to_id}.${e.to_channel}[${e.to_sub}]`;
    }
  }

  for (const [id, p] of pos) {
    const g = el(gNode, "g", { class: "nodeg", transform: `translate(${p.x},${p.y})` });
    const scaler = p.service === "Scaler";
    const r = el(g, "rect", {
      width: p.w, height: p.h, rx: 2,
      class: "node" + (p.live ? "" : " dead") + (scaler ? " scaler" : ""),
    });
    r.dataset.id = id;
    const lb = text(g, 8, 18, id, "node-label" + (scaler ? " scaler" : ""));
    lb.setAttribute("data-id", id);
    r.addEventListener("click", () => {
      focusId = (focusId === id) ? null : id;
      applyFocus();
      showDetail(id);
    });
  }

  applyFocus();
  $("stats").innerHTML = summaryHtml(snap);
  if (!focusId) showOverview();
}

function applyFocus() {
  document.querySelectorAll(".edge").forEach((p) => {
    if (!focusId) {
      p.classList.remove("dim");
      return;
    }
    const hit = p.dataset.from === focusId || p.dataset.to === focusId;
    p.classList.toggle("dim", !hit);
  });
  document.querySelectorAll(".node").forEach((n) => {
    const id = n.dataset.id;
    n.classList.toggle("focused", id === focusId);
    n.classList.toggle("dim", !!(focusId && id !== focusId));
  });
}

function isParamsView() {
  return location.pathname.replace(/\/+$/, "") === "/params";
}

function applyView() {
  const params = isParamsView();
  document.title = params ? "nestdaq params" : "nestdaq topology";
  $("page-title").innerHTML = params
    ? "params <span>Redis DB2 · param.sh</span>"
    : "topology <span>live from Redis</span>";
  $("nav-topo").classList.toggle("active", !params);
  $("nav-params").classList.toggle("active", params);
  $("stage").hidden = params;
  $("view-params").hidden = !params;
  $("toolbar-topo").hidden = params;
  $("toolbar-params").hidden = !params;
}

function showView(name, push) {
  const params = name === "params";
  const dest = params ? "/params" : "/";
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
  if (data.demo) pills.push(`<span class="pill warn">demo</span>`);
  if ((data.warnings || []).length) pills.push(`<span class="pill warn">${data.warnings.length} warnings</span>`);
  if (!data.demo && data.ok) pills.push(`<span class="pill ok">redis live</span>`);
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
  if (snap.demo) pills.push(`<span class="pill warn">demo</span>`);
  if ((snap.stale || []).length) pills.push(`<span class="pill stale">${snap.stale.length} stale keys ignored</span>`);
  if ((snap.warnings || []).length) pills.push(`<span class="pill warn">${snap.warnings.length} warnings</span>`);
  if (!snap.demo && snap.ok) pills.push(`<span class="pill ok">redis live</span>`);
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
    ${(snap.warnings || []).length ? `<h2>warnings</h2><ul>${snap.warnings.map((w) => `<li>${esc(w)}</li>`).join("")}</ul>` : ""}
    ${(snap.stale || []).length ? `<h2>stale (ignored)</h2><table>${staleRows}</table>` : ""}
    <p class="kind-label">click a process to see params and its mesh only</p>
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
  const wantParams = isParamsView();
  try {
    if (wantParams) {
      const r = await fetch(forceDemo ? "/api/params?demo=1" : "/api/params", { cache: "no-store" });
      const data = await r.json();
      if (gen !== pollGen || !isParamsView()) return;
      $("clock").textContent = new Date(data.ts * 1000).toLocaleTimeString();
      renderParams(data);
      $("warnlist").innerHTML = (data.warnings || []).map((w) => `<li>${esc(w)}</li>`).join("");
      return;
    }
    const r = await fetch(forceDemo ? "/api/snapshot?demo=1" : "/api/snapshot", { cache: "no-store" });
    const data = await r.json();
    if (gen !== pollGen || isParamsView()) return;
    snap = data;
    $("clock").textContent = new Date(snap.ts * 1000).toLocaleTimeString();
    render(snap);
    $("warnlist").innerHTML = (snap.warnings || []).map((w) => `<li>${esc(w)}</li>`).join("");
  } catch (err) {
    if (gen !== pollGen) return;
    $("clock").textContent = "redis/viewer error";
    console.error(err);
  }
}

window.addEventListener("DOMContentLoaded", () => {
  applyView();
  $("tog-mesh").addEventListener("change", (ev) => {
    showAllMesh = ev.target.checked;
    if (snap) render(snap);
  });
  $("tog-expected").addEventListener("change", (ev) => {
    showExpected = ev.target.checked;
    if (snap) render(snap);
  });
  $("tog-demo").addEventListener("change", (ev) => {
    forceDemo = ev.target.checked;
    poll();
  });
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
  window.addEventListener("popstate", () => {
    applyView();
    poll();
  });
  poll();
  setInterval(poll, 1000);
});
