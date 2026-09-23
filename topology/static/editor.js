/* Topology editor: binaries in bin/ → endpoints + links → Redis DB0 topology:* only. */

let editBinaries = [];
let editServices = [];
let editLinks = [];

function editStatus(msg, ok) {
  const el = document.getElementById("edit-status");
  if (!el) return;
  el.textContent = msg || "";
  el.style.color = ok === false ? "#b91c1c" : "";
}

function defaultEndpoints(binName) {
  const row = editBinaries.find((b) => b.name === binName);
  const eps = (row && row.defaults) || [
    { channel: "in", type: "pull", method: "bind", autoSubChannel: false },
    { channel: "out", type: "push", method: "connect", autoSubChannel: false },
  ];
  return eps.map((e) => ({
    channel: e.channel,
    type: e.type || "push",
    method: e.method || "bind",
    autoSubChannel: !!e.autoSubChannel,
  }));
}

function addService(binName, serviceName) {
  const name = (serviceName || binName || "").trim();
  if (!name) return;
  if (editServices.some((s) => s.name === name)) {
    editStatus(`${name} is already in the list`, false);
    return;
  }
  editServices.push({
    name,
    binary: binName || name,
    n: 1,
    endpoints: defaultEndpoints(binName || name),
  });
  renderEditor();
}

function removeService(idx) {
  const name = editServices[idx] && editServices[idx].name;
  editServices.splice(idx, 1);
  editLinks = editLinks.filter((l) => l.from_service !== name && l.to_service !== name);
  renderEditor();
}

function endpointChoices() {
  const out = [];
  for (const svc of editServices) {
    for (const ep of svc.endpoints) {
      if (ep.channel) out.push(`${svc.name}.${ep.channel}`);
    }
  }
  return out;
}

function renderEditor() {
  const bins = document.getElementById("edit-bin-list");
  const svcs = document.getElementById("edit-svc-root");
  const links = document.getElementById("edit-link-root");
  if (!bins || !svcs || !links) return;

  bins.innerHTML = editBinaries.map((b) =>
    `<li><button type="button" class="edit-bin" data-bin="${esc(b.name)}">${esc(b.name)}</button></li>`
  ).join("") || "<li>no executables in bin/</li>";
  bins.querySelectorAll(".edit-bin").forEach((btn) => {
    btn.addEventListener("click", () => addService(btn.dataset.bin));
  });

  svcs.innerHTML = editServices.map((svc, i) => {
    const eps = svc.endpoints.map((ep, j) => `
      <tr>
        <td><input data-s="${i}" data-e="${j}" data-f="channel" value="${esc(ep.channel)}"></td>
        <td>
          <select data-s="${i}" data-e="${j}" data-f="type">
            ${["push", "pull", "pub", "sub"].map((t) =>
              `<option${ep.type === t ? " selected" : ""}>${t}</option>`).join("")}
          </select>
        </td>
        <td>
          <select data-s="${i}" data-e="${j}" data-f="method">
            ${["bind", "connect"].map((t) =>
              `<option${ep.method === t ? " selected" : ""}>${t}</option>`).join("")}
          </select>
        </td>
        <td><label><input type="checkbox" data-s="${i}" data-e="${j}" data-f="autoSubChannel"${ep.autoSubChannel ? " checked" : ""}> autoSub</label></td>
        <td><button type="button" data-rm-ep="${i}:${j}">x</button></td>
      </tr>`).join("");
    return `
      <article class="edit-card">
        <header>
          <label>service <input data-svc="${i}" data-f="name" value="${esc(svc.name)}"></label>
          <label>n <input type="number" min="1" max="200" data-svc="${i}" data-f="n" value="${esc(svc.n || 1)}"></label>
          <label>binary <input data-svc="${i}" data-f="binary" value="${esc(svc.binary)}" readonly></label>
          <button type="button" data-add-ep="${i}">+ endpoint</button>
          <button type="button" data-rm-svc="${i}">remove</button>
        </header>
        <table>
          <thead><tr><th>channel</th><th>type</th><th>method</th><th></th><th></th></tr></thead>
          <tbody>${eps || "<tr><td colspan='5'>no endpoints</td></tr>"}</tbody>
        </table>
      </article>`;
  }).join("") || "<p class='edit-hint'>add a binary from the left</p>";

  svcs.querySelectorAll("[data-svc]").forEach((el) => {
    el.addEventListener("change", () => {
      const s = editServices[+el.dataset.svc];
      if (!s) return;
      if (el.dataset.f === "name") {
        const prev = s.name;
        const next = el.value.trim();
        if (!next) return;
        s.name = next;
        editLinks.forEach((l) => {
          if (l.from_service === prev) l.from_service = next;
          if (l.to_service === prev) l.to_service = next;
        });
      }
      if (el.dataset.f === "n") {
        const n = parseInt(el.value, 10);
        s.n = Number.isFinite(n) ? Math.max(1, Math.min(200, n)) : 1;
      }
    });
  });
  svcs.querySelectorAll("[data-s][data-e]").forEach((el) => {
    const apply = () => {
      const ep = editServices[+el.dataset.s] && editServices[+el.dataset.s].endpoints[+el.dataset.e];
      if (!ep) return;
      if (el.dataset.f === "autoSubChannel") ep.autoSubChannel = el.checked;
      else ep[el.dataset.f] = el.value;
    };
    el.addEventListener("change", apply);
    el.addEventListener("input", apply);
  });
  svcs.querySelectorAll("[data-add-ep]").forEach((btn) => {
    btn.addEventListener("click", () => {
      const s = editServices[+btn.dataset.addEp];
      if (s) s.endpoints.push({ channel: "out", type: "push", method: "connect", autoSubChannel: false });
      renderEditor();
    });
  });
  svcs.querySelectorAll("[data-rm-svc]").forEach((btn) => {
    btn.addEventListener("click", () => removeService(+btn.dataset.rmSvc));
  });
  svcs.querySelectorAll("[data-rm-ep]").forEach((btn) => {
    btn.addEventListener("click", () => {
      const [si, ei] = btn.dataset.rmEp.split(":").map(Number);
      const s = editServices[si];
      if (s) s.endpoints.splice(ei, 1);
      renderEditor();
    });
  });

  const choices = endpointChoices();
  const opt = (cur) => choices.map((c) =>
    `<option value="${esc(c)}"${c === cur ? " selected" : ""}>${esc(c)}</option>`).join("");
  links.innerHTML = editLinks.map((lk, i) => {
    const from = `${lk.from_service}.${lk.from_channel}`;
    const to = `${lk.to_service}.${lk.to_channel}`;
    return `<div class="edit-link-row">
      <select data-lk="${i}" data-f="from">${opt(from)}</select>
      <span>→</span>
      <select data-lk="${i}" data-f="to">${opt(to)}</select>
      <button type="button" data-rm-lk="${i}">x</button>
    </div>`;
  }).join("") || "<p class='edit-hint'>add a link after two endpoints exist</p>";
  links.querySelectorAll("[data-lk]").forEach((el) => {
    el.addEventListener("change", () => {
      const lk = editLinks[+el.dataset.lk];
      if (!lk) return;
      const [svc, ch] = el.value.split(".");
      if (el.dataset.f === "from") {
        lk.from_service = svc;
        lk.from_channel = ch;
      } else {
        lk.to_service = svc;
        lk.to_channel = ch;
      }
    });
  });
  links.querySelectorAll("[data-rm-lk]").forEach((btn) => {
    btn.addEventListener("click", () => {
      editLinks.splice(+btn.dataset.rmLk, 1);
      renderEditor();
    });
  });
}

function collectPayload() {
  const endpoints = [];
  for (const svc of editServices) {
    for (const ep of svc.endpoints) {
      if (!svc.name || !ep.channel) continue;
      endpoints.push({
        service: svc.name,
        channel: ep.channel,
        type: ep.type,
        method: ep.method,
        autoSubChannel: !!ep.autoSubChannel,
      });
    }
  }
  return {
    endpoints,
    links: editLinks.filter((l) => l.from_service && l.to_service),
    services: editServices.map((s) => ({
      name: s.name,
      binary: s.binary,
      n: Math.max(1, Math.min(200, parseInt(s.n, 10) || 1)),
    })),
  };
}

function applyLoaded(data) {
  const bySvc = new Map();
  for (const ep of data.endpoints || []) {
    if (!bySvc.has(ep.service)) {
      const n = (data.counts && data.counts[ep.service]) || 1;
      bySvc.set(ep.service, { name: ep.service, binary: ep.service, n, endpoints: [] });
    }
    bySvc.get(ep.service).endpoints.push({
      channel: ep.channel,
      type: ep.type || "push",
      method: ep.method || "bind",
      autoSubChannel: !!ep.autoSubChannel,
    });
  }
  for (const svc of bySvc.values()) {
    const hit = editBinaries.find((b) => b.name === svc.name);
    if (hit) svc.binary = hit.name;
    else if (svc.name === "DecSink") svc.binary = "FileSink";
  }
  editServices = [...bySvc.values()];
  editLinks = (data.links || []).map((l) => ({
    from_service: l.from_service,
    from_channel: l.from_channel,
    to_service: l.to_service,
    to_channel: l.to_channel,
  }));
  renderEditor();
}

async function loadEditorBins() {
  const r = await fetch("/api/binaries", { cache: "no-store" });
  const data = await r.json();
  editBinaries = data.binaries || [];
}

async function loadEditorFromRedis() {
  const r = await fetch("/api/topology", { cache: "no-store" });
  const data = await r.json();
  applyLoaded(data);
  editStatus(`loaded ${editServices.length} services, ${(data.links || []).length} links`, true);
}

async function writeEditorToRedis() {
  const payload = collectPayload();
  if (!payload.endpoints.length) {
    editStatus("nothing to write", false);
    return;
  }
  const r = await fetch("/api/topology", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(payload),
  });
  const data = await r.json();
  if (!data.ok) {
    editStatus(data.error || "write failed", false);
    return;
  }
  editStatus(`wrote ${data.endpoints} endpoints, ${data.links} links, counts saved`, true);
}

async function writeEditorScripts() {
  const payload = collectPayload();
  if (!payload.endpoints.length || !payload.links.length) {
    editStatus("need endpoints and at least one link", false);
    return;
  }
  const dirEl = document.getElementById("edit-script-dir");
  const stemEl = document.getElementById("edit-script-stem");
  const dir = ((dirEl && dirEl.value) || (window.currentScriptsDir && window.currentScriptsDir()) || "run").trim() || "run";
  const stem = ((stemEl && stemEl.value) || "from-edit").trim() || "from-edit";
  if (!window.confirm(`Write topology/param/run-${stem}-<time>.sh into ${dir}/ ?`)) return;
  const r = await fetch("/api/scripts/write", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({
      dir,
      stem,
      endpoints: payload.endpoints,
      links: payload.links,
      services: payload.services,
    }),
  });
  const data = await r.json();
  if (!data.ok) {
    editStatus(data.error || "write scripts failed", false);
    return;
  }
  editStatus(`wrote ${(data.files || []).join(", ")} in ${data.dir}/`, true);
}

async function startEditorRun() {
  const payload = collectPayload();
  if (!payload.services.length) {
    editStatus("add a service and set n first", false);
    return;
  }
  editStatus("starting…", true);
  const r = await fetch("/api/run-start", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(payload),
  });
  const data = await r.json();
  if (!data.ok) {
    editStatus(data.error || "start failed", false);
    return;
  }
  editStatus(`wrote topology, started ${data.started} processes`, true);
}

async function initEditor() {
  const addLink = document.getElementById("edit-add-link");
  if (!addLink) return;
  await loadEditorBins();
  document.getElementById("edit-load").addEventListener("click", () => {
    loadEditorFromRedis().catch((err) => editStatus(String(err), false));
  });
  document.getElementById("edit-write").addEventListener("click", () => {
    writeEditorToRedis().catch((err) => editStatus(String(err), false));
  });
  const startBtn = document.getElementById("edit-start");
  if (startBtn) {
    startBtn.addEventListener("click", () => {
      startEditorRun().catch((err) => editStatus(String(err), false));
    });
  }
  const clearForm = document.getElementById("edit-clear-form");
  if (clearForm) {
    clearForm.addEventListener("click", () => {
      editServices = [];
      editLinks = [];
      renderEditor();
      editStatus("edit form cleared (Redis unchanged)", true);
    });
  }
  const writeScripts = document.getElementById("edit-write-scripts");
  if (writeScripts) {
    writeScripts.addEventListener("click", () => {
      writeEditorScripts().catch((err) => editStatus(String(err), false));
    });
  }
  const editDir = document.getElementById("edit-script-dir");
  if (editDir) {
    editDir.addEventListener("input", () => { editDir.dataset.touched = "1"; });
  }
  const clearRedis = document.getElementById("edit-clear-redis");
  if (clearRedis) {
    clearRedis.addEventListener("click", () => {
      if (!window.confirm("Clear Redis DB0/1/2 except run number?")) return;
      fetch("/api/redis-clear", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: "{}",
      }).then((r) => r.json()).then((data) => {
        if (!data.ok) {
          editStatus(data.error || "clear failed", false);
          return;
        }
        editServices = [];
        editLinks = [];
        renderEditor();
        const run = (data.kept && data.kept.run_number) || "";
        editStatus(`Redis cleared, run # kept ${run}`, true);
      }).catch((err) => editStatus(String(err), false));
    });
  }
  addLink.addEventListener("click", () => {
    const ch = endpointChoices();
    if (ch.length < 2) {
      editStatus("need at least two endpoints", false);
      return;
    }
    const [a, ac] = ch[0].split(".");
    const [b, bc] = ch[1].split(".");
    editLinks.push({ from_service: a, from_channel: ac, to_service: b, to_channel: bc });
    renderEditor();
  });
  renderEditor();
}

window.initEditor = initEditor;
window.loadEditorFromRedis = loadEditorFromRedis;

window.addEventListener("DOMContentLoaded", () => {
  initEditor().then(() => {
    const p = location.pathname.replace(/\/+$/, "") || "/";
    if (p === "/edit") return loadEditorFromRedis();
  }).catch((err) => editStatus(String(err), false));
});
