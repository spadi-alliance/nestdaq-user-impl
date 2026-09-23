/* Load and run topology / param / run shell scripts from a repo directory. */

const SCRIPTS_DIR_KEY = "nestdaq-scripts-dir";

let scriptsDir = "topology/run";
let scriptsSelected = "";
let scriptsLoaded = false;

function scriptsStatus(msg, ok) {
  const el = document.getElementById("scripts-status");
  if (!el) return;
  el.textContent = msg || "";
  el.style.color = ok === false ? "#b91c1c" : "";
}

function currentScriptsDir() {
  const inp = document.getElementById("scripts-dir");
  const raw = ((inp && inp.value) || scriptsDir || "run").trim() || "run";
  scriptsDir = raw;
  try { localStorage.setItem(SCRIPTS_DIR_KEY, raw); } catch (err) { /* ignore */ }
  return raw;
}

function fillDirLists(dirs) {
  const dl = document.getElementById("scripts-dir-list");
  if (dl) {
    dl.innerHTML = (dirs || []).map((d) => `<option value="${d}"></option>`).join("");
  }
  const editDir = document.getElementById("edit-script-dir");
  if (editDir && !editDir.value) editDir.value = scriptsDir;
}

function renderScriptGroup(kind, rows) {
  const el = document.getElementById(kind === "run" ? "scripts-run-list" : `scripts-${kind}`);
  if (!el) return;
  el.innerHTML = (rows || []).map((s) =>
    `<li><button type="button" data-script="${s.name}"${s.name === scriptsSelected ? " class='active'" : ""}>${s.name}</button></li>`
  ).join("") || "<li class='edit-hint'>none</li>";
  el.querySelectorAll("[data-script]").forEach((btn) => {
    btn.addEventListener("click", () => {
      scriptsSelected = btn.dataset.script;
      document.querySelectorAll(".scripts-list button").forEach((b) => {
        b.classList.toggle("active", b.dataset.script === scriptsSelected);
      });
      previewScript(scriptsSelected).catch((err) => scriptsStatus(String(err), false));
    });
  });
}

async function loadScriptsList() {
  const dir = currentScriptsDir();
  const r = await fetch(`/api/scripts?dir=${encodeURIComponent(dir)}`, { cache: "no-store" });
  const data = await r.json();
  if (!data.ok) {
    scriptsStatus(data.error || "list failed", false);
    return;
  }
  scriptsDir = data.dir || dir;
  const inp = document.getElementById("scripts-dir");
  if (inp) inp.value = scriptsDir;
  fillDirLists(data.dirs || []);
  const g = data.groups || {};
  renderScriptGroup("topology", g.topology);
  renderScriptGroup("param", g.param);
  renderScriptGroup("run", g.run);
  renderScriptGroup("other", g.other);
  scriptsLoaded = true;
  if (scriptsSelected && !(data.scripts || []).some((s) => s.name === scriptsSelected)) {
    scriptsSelected = "";
  }
  scriptsStatus(`${scriptsDir}/  ${(data.scripts || []).length} scripts`, true);
}

async function previewScript(name) {
  const dir = currentScriptsDir();
  const title = document.getElementById("scripts-preview-title");
  const pre = document.getElementById("scripts-preview");
  if (title) title.textContent = name;
  const r = await fetch(
    `/api/scripts/text?dir=${encodeURIComponent(dir)}&name=${encodeURIComponent(name)}`,
    { cache: "no-store" },
  );
  const data = await r.json();
  if (!data.ok) {
    scriptsStatus(data.error || "read failed", false);
    if (pre) pre.textContent = "";
    return;
  }
  if (pre) pre.textContent = data.text || "";
  scriptsStatus(`${dir}/${name}`, true);
}

async function runSelectedScript() {
  if (!scriptsSelected) {
    scriptsStatus("select a script first", false);
    return;
  }
  if (!window.confirm(`Run ${currentScriptsDir()}/${scriptsSelected} ?`)) return;
  const out = document.getElementById("scripts-output");
  if (out) out.textContent = "running…";
  scriptsStatus(`running ${scriptsSelected}…`, true);
  const r = await fetch("/api/scripts/run", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ dir: currentScriptsDir(), name: scriptsSelected }),
  });
  const data = await r.json();
  if (out) out.textContent = data.output || data.error || "";
  if (!data.ok) {
    scriptsStatus(data.error || `exit ${data.exit}`, false);
    return;
  }
  scriptsStatus(`${scriptsSelected}  exit ${data.exit}`, true);
}

function initScripts() {
  const listBtn = document.getElementById("scripts-list");
  if (!listBtn) return;
  try {
    const saved = localStorage.getItem(SCRIPTS_DIR_KEY);
    if (saved && saved !== "run") scriptsDir = saved;
    else if (saved === "run") scriptsDir = "topology/run";
  } catch (err) { /* ignore */ }
  const inp = document.getElementById("scripts-dir");
  if (inp) inp.value = scriptsDir;
  const editDir = document.getElementById("edit-script-dir");
  if (editDir && !editDir.dataset.touched) editDir.value = scriptsDir;
  listBtn.addEventListener("click", () => {
    loadScriptsList().catch((err) => scriptsStatus(String(err), false));
  });
  const runBtn = document.getElementById("scripts-run");
  if (runBtn) {
    runBtn.addEventListener("click", () => {
      runSelectedScript().catch((err) => scriptsStatus(String(err), false));
    });
  }
  if (inp) {
    inp.addEventListener("change", () => {
      currentScriptsDir();
      loadScriptsList().catch((err) => scriptsStatus(String(err), false));
    });
    inp.addEventListener("keydown", (ev) => {
      if (ev.key === "Enter") {
        ev.preventDefault();
        loadScriptsList().catch((err) => scriptsStatus(String(err), false));
      }
    });
  }
}

function showScriptsView() {
  if (!scriptsLoaded) {
    loadScriptsList().catch((err) => scriptsStatus(String(err), false));
  }
}

window.initScripts = initScripts;
window.showScriptsView = showScriptsView;
window.currentScriptsDir = currentScriptsDir;

window.addEventListener("DOMContentLoaded", () => {
  initScripts();
  loadScriptsList().catch(() => {});
});
