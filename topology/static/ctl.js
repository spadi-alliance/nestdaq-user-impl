/* DAQ control via Redis PUBLISH daqctl — same path WebGui uses after websocket.
   Does not talk to daq-webctl :8080. */

const COUNT_SHORT = {
  Undefined: "undef", Ok: "ok", Error: "error", Idle: "idle",
  InitializingDevice: "init-dev", Initialized: "init.",
  Binding: "bind.", Bound: "bound", Connecting: "conn.",
  DeviceReady: "dev-ready", InitializingTask: "init-task", Ready: "ready",
  Running: "running", ResettingTask: "reset-task",
  ResettingDevice: "reset-dev", Exiting: "exiting",
};

const DaqCtl = {
  ready: false,
  latest: "",
  startAt: "",
  stopAt: "",
  lastSummary: null,
  selectedSrv: ["all"],
  selectedInst: ["all"],
};

function selectedTexts(sel) {
  return Array.from(sel.options).filter((o) => o.selected && !o.disabled).map((o) => o.text);
}

function fillSelect(sel, names, keep) {
  const chosen = keep && keep.length ? keep : selectedTexts(sel);
  sel.innerHTML = "";
  const hint = document.createElement("option");
  hint.textContent = sel.id === "serviceSelector" ? "services" : "instances";
  hint.disabled = true;
  sel.appendChild(hint);
  const all = document.createElement("option");
  all.textContent = "all";
  sel.appendChild(all);
  names.forEach((name) => {
    const o = document.createElement("option");
    o.textContent = name;
    sel.appendChild(o);
  });
  let any = false;
  Array.from(sel.options).forEach((o) => {
    if (!o.disabled && chosen.includes(o.text)) {
      o.selected = true;
      any = true;
    }
  });
  if (!any) all.selected = true;
}

function setWsPill(kind, text) {
  const el = document.getElementById("daq-ws");
  if (!el) return;
  el.className = "pill " + kind;
  el.textContent = text;
}

function setRunMeta() {
  const el = document.getElementById("daq-run-meta");
  if (!el) return;
  const bits = [];
  if (DaqCtl.latest !== "") bits.push("latest " + DaqCtl.latest);
  if (DaqCtl.startAt) bits.push("start " + DaqCtl.startAt);
  if (DaqCtl.stopAt) bits.push("stop " + DaqCtl.stopAt);
  el.textContent = bits.join(" · ");
}

function setActiveCmd(cmd) {
  document.querySelectorAll("#daq-bar [data-cmd]").forEach((btn) => {
    btn.classList.toggle("active", !!cmd && btn.dataset.cmd === cmd);
  });
}

async function postJson(url, body) {
  const r = await fetch(url, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body || {}),
  });
  return r.json();
}

function applyDaqStatus(data) {
  if (!data || !data.ok) {
    DaqCtl.ready = false;
    setWsPill("stale", (data && data.error) || "redis down");
    return;
  }
  DaqCtl.ready = true;
  setWsPill("ok", "redis · daqctl");
  const inp = document.getElementById("daq-run");
  if (inp && document.activeElement !== inp) inp.value = data.run_number || "";
  DaqCtl.latest = data.latest_run_number || "";
  setRunMeta();
  const a = document.getElementById("wait-device-ready");
  const b = document.getElementById("wait-ready");
  if (a && document.activeElement !== a) a.checked = !!data.wait_device_ready;
  if (b && document.activeElement !== b) b.checked = !!data.wait_ready;
  renderDaqTables(data);
}

async function refreshDaqStatus() {
  try {
    const r = await fetch("/api/daq/status", { cache: "no-store" });
    applyDaqStatus(await r.json());
  } catch (err) {
    DaqCtl.ready = false;
    setWsPill("stale", "redis unreachable");
  }
}

async function publishCmd(cmd) {
  setActiveCmd(cmd);
  if (cmd === "RUN") {
    DaqCtl.startAt = new Date().toLocaleTimeString();
    DaqCtl.stopAt = "";
    setRunMeta();
  } else if (cmd === "STOP") {
    DaqCtl.stopAt = new Date().toLocaleTimeString();
    setRunMeta();
    const auto = document.getElementById("run-number-auto-increment-checkbox");
    if (auto && auto.checked) {
      await postJson("/api/daq/run", { action: "incr" });
    }
  }
  const srv = document.getElementById("serviceSelector");
  const inst = document.getElementById("instanceSelector");
  let out;
  try {
    out = await postJson("/api/daq/command", {
      value: cmd,
      services: srv ? selectedTexts(srv) : ["all"],
      instances: inst ? selectedTexts(inst) : ["all"],
    });
  } catch (err) {
    setWsPill("stale", "command failed");
    return;
  }
  if (!out.ok) {
    setWsPill("warn", out.error || "command rejected");
    return;
  }
  if (out.subscribers === 0) {
    setWsPill("warn", "published, but no device on daqctl");
  } else {
    setWsPill("ok", "redis · daqctl · " + cmd);
  }
  refreshDaqStatus();
}

function renderDaqTables(arg) {
  DaqCtl.lastSummary = arg;
  const services = arg.services || [];
  const sum = document.getElementById("daq-summary");
  const inst = document.getElementById("daq-instances");
  if (!sum || !inst) return;

  const names = services[0] ? (services[0].counts || []).map((c) => c.name) : [];
  const head = ["service", "n"].concat(names.map((n) => COUNT_SHORT[n] || n)).concat(["updated"]);
  sum.innerHTML = "<thead><tr>" + head.map((h) => `<th>${esc(h)}</th>`).join("") + "</tr></thead><tbody></tbody>";
  const tb = sum.tBodies[0];
  services.forEach((svc) => {
    const tr = tb.insertRow(-1);
    tr.insertCell(-1).textContent = svc.service;
    tr.insertCell(-1).textContent = svc.n_instances;
    (svc.counts || []).forEach((c) => {
      const td = tr.insertCell(-1);
      const n = Number(c.value) || 0;
      td.textContent = n ? String(n) : "";
      const key = (c.name || "").toLowerCase();
      if (n && key === "running") td.className = "hit-run";
      if (n && key === "error") td.className = "hit-err";
      if (n && key === "ready") td.className = "hit-ready";
    });
    tr.insertCell(-1).textContent = svc.date || "";
  });

  inst.innerHTML = "<thead><tr><th>service</th><th>instance</th><th>state</th><th>updated</th></tr></thead><tbody></tbody>";
  const ib = inst.tBodies[0];
  services.forEach((svc) => {
    (svc.instances || []).forEach((row) => {
      const tr = ib.insertRow(-1);
      tr.insertCell(-1).textContent = row.service;
      tr.insertCell(-1).textContent = row.instance;
      const st = tr.insertCell(-1);
      st.textContent = row.state || "";
      const key = String(row.state || "").toUpperCase();
      if (key === "RUNNING") st.className = "hit-run";
      if (key === "ERROR") st.className = "hit-err";
      if (key === "READY") st.className = "hit-ready";
      tr.insertCell(-1).textContent = row.date || "";
    });
  });

  const srvSel = document.getElementById("serviceSelector");
  const instSel = document.getElementById("instanceSelector");
  if (srvSel) fillSelect(srvSel, services.map((s) => s.service), DaqCtl.selectedSrv);
  if (instSel) {
    const namesI = [];
    services.forEach((s) => (s.instances || []).forEach((x) => namesI.push(s.service + ":" + x.instance)));
    fillSelect(instSel, namesI, DaqCtl.selectedInst);
  }
}

function initDaqCtl() {
  document.querySelectorAll("#daq-bar [data-cmd]").forEach((btn) => {
    btn.addEventListener("click", () => publishCmd(btn.dataset.cmd));
  });
  const set = document.getElementById("daq-run-set");
  const inc = document.getElementById("daq-run-inc");
  const inp = document.getElementById("daq-run");
  if (set && inp) {
    set.addEventListener("click", async () => {
      const n = Number(inp.value);
      if (!Number.isInteger(n)) {
        setWsPill("warn", "invalid run number");
        return;
      }
      await postJson("/api/daq/run", { action: "set", value: String(n) });
      refreshDaqStatus();
    });
  }
  if (inc) {
    inc.addEventListener("click", async () => {
      await postJson("/api/daq/run", { action: "incr" });
      refreshDaqStatus();
    });
  }
  if (inp) {
    inp.addEventListener("keydown", (ev) => {
      if (ev.key === "Enter") {
        ev.preventDefault();
        if (set) set.click();
      }
    });
  }
  ["wait-device-ready", "wait-ready"].forEach((id) => {
    const el = document.getElementById(id);
    if (el) {
      el.addEventListener("change", () => {
        postJson("/api/daq/flag", { name: id, value: String(el.checked) });
      });
    }
  });
  const srv = document.getElementById("serviceSelector");
  const inst = document.getElementById("instanceSelector");
  if (srv) srv.addEventListener("change", () => { DaqCtl.selectedSrv = selectedTexts(srv); });
  if (inst) inst.addEventListener("change", () => { DaqCtl.selectedInst = selectedTexts(inst); });
  refreshDaqStatus();
  setInterval(refreshDaqStatus, 1000);
}
