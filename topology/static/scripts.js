/* Load and run topology / param / run shell scripts from a repo directory. */

const SCRIPTS_DIR_KEY = "nestdaq-scripts-dir-v2";
const DEFAULT_SCRIPTS_DIR = "topology";

let scriptsDir = DEFAULT_SCRIPTS_DIR;
let scriptsSelected = "";
let scriptsLoaded = false;
let folderRows = [];
let folderParent = null;
let folderOpen = false;

function scriptsStatus(msg, ok) {
  const el = document.getElementById("scripts-status");
  if (!el) return;
  el.textContent = msg || "";
  el.style.color = ok === false ? "#b91c1c" : "";
}

function escHtml(s) {
  return String(s).replace(/[&<>"']/g, (c) => ({
    "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;",
  }[c]));
}

function currentScriptsDir() {
  const inp = document.getElementById("scripts-dir");
  const raw = ((inp && inp.value) || scriptsDir || DEFAULT_SCRIPTS_DIR).trim() || DEFAULT_SCRIPTS_DIR;
  scriptsDir = raw;
  try { localStorage.setItem(SCRIPTS_DIR_KEY, raw); } catch (err) { /* ignore */ }
  const editDir = document.getElementById("edit-script-dir");
  if (editDir && !editDir.dataset.touched) editDir.value = raw;
  return raw;
}

function setFolderOpen(open) {
  folderOpen = open;
  const panel = document.getElementById("scripts-folder-panel");
  if (panel) panel.hidden = !open;
}

function renderFolders() {
  const ul = document.getElementById("scripts-folders");
  if (!ul) return;
  const filterEl = document.getElementById("scripts-folder-filter");
  const filter = ((filterEl && filterEl.value) || "").trim().toLowerCase();
  const rows = folderRows.filter((row) => !filter || row.name.toLowerCase().includes(filter));
  const parts = [];
  if (folderParent && !filter) {
    parts.push(`<li><button type="button" data-dir="${escHtml(folderParent)}">..</button></li>`);
  }
  rows.forEach((row) => {
    parts.push(`<li><button type="button" data-dir="${escHtml(row.path)}">${escHtml(row.name)}</button></li>`);
  });
  ul.innerHTML = parts.join("") || "<li class='edit-hint'>no folders</li>";
  ul.querySelectorAll("[data-dir]").forEach((btn) => {
    btn.addEventListener("click", () => {
      setFolderOpen(false);
      openDir(btn.dataset.dir).catch((err) => scriptsStatus(String(err), false));
    });
  });
}

async function loadFolders() {
  const dir = currentScriptsDir();
  const r = await fetch(`/api/scripts/dirs?dir=${encodeURIComponent(dir)}`, { cache: "no-store" });
  const data = await r.json();
  if (!data.ok) {
    scriptsStatus(data.error || "cannot open directory", false);
    folderRows = [];
    folderParent = null;
    renderFolders();
    return;
  }
  scriptsDir = data.dir || dir;
  const inp = document.getElementById("scripts-dir");
  if (inp) inp.value = scriptsDir;
  try { localStorage.setItem(SCRIPTS_DIR_KEY, scriptsDir); } catch (err) { /* ignore */ }
  folderParent = data.parent || null;
  folderRows = data.folders || [];
  renderFolders();
}

async function openDir(path) {
  const inp = document.getElementById("scripts-dir");
  if (inp) inp.value = path;
  const filterEl = document.getElementById("scripts-folder-filter");
  if (filterEl) filterEl.value = "";
  currentScriptsDir();
  await loadFolders();
  await loadScriptsList();
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
  const inp = document.getElementById("scripts-dir");
  if (!inp) return;
  try {
    const saved = localStorage.getItem(SCRIPTS_DIR_KEY);
    if (saved) scriptsDir = saved;
  } catch (err) { /* ignore */ }
  inp.value = scriptsDir;
  const editDir = document.getElementById("edit-script-dir");
  if (editDir && !editDir.dataset.touched) editDir.value = scriptsDir;
  const upBtn = document.getElementById("scripts-up");
  if (upBtn) {
    upBtn.addEventListener("click", () => {
      if (!folderParent) return;
      openDir(folderParent).catch((err) => scriptsStatus(String(err), false));
    });
  }
  const runBtn = document.getElementById("scripts-run");
  if (runBtn) {
    runBtn.addEventListener("click", () => {
      runSelectedScript().catch((err) => scriptsStatus(String(err), false));
    });
  }
  const filterEl = document.getElementById("scripts-folder-filter");
  if (filterEl) {
    filterEl.addEventListener("input", () => renderFolders());
  }
  if (inp) {
    inp.addEventListener("click", () => {
      setFolderOpen(true);
      loadFolders().catch((err) => scriptsStatus(String(err), false));
    });
    inp.addEventListener("keydown", (ev) => {
      if (ev.key === "Enter") {
        ev.preventDefault();
        setFolderOpen(false);
        openDir(inp.value.trim() || DEFAULT_SCRIPTS_DIR).catch((err) => scriptsStatus(String(err), false));
      } else if (ev.key === "Escape") {
        setFolderOpen(false);
      }
    });
  }
  document.addEventListener("mousedown", (ev) => {
    if (!folderOpen) return;
    const panel = document.getElementById("scripts-folder-panel");
    if (inp && (inp === ev.target || inp.contains(ev.target))) return;
    if (panel && panel.contains(ev.target)) return;
    setFolderOpen(false);
  });
}

function showScriptsView() {
  if (!scriptsLoaded) {
    openDir(scriptsDir).catch((err) => scriptsStatus(String(err), false));
  }
}

window.initScripts = initScripts;
window.showScriptsView = showScriptsView;
window.currentScriptsDir = currentScriptsDir;

window.addEventListener("DOMContentLoaded", () => {
  initScripts();
  initEditDirPicker();
  openDir(scriptsDir).catch(() => {});
});

function initEditDirPicker() {
  const inp = document.getElementById("edit-script-dir");
  const panel = document.getElementById("edit-folder-panel");
  const ul = document.getElementById("edit-folders");
  if (!inp || !panel || !ul) return;
  const filterEl = document.getElementById("edit-folder-filter");
  const upBtn = document.getElementById("edit-dir-up");
  let rows = [];
  let parent = null;
  let open = false;

  function setOpen(next) {
    open = next;
    panel.hidden = !next;
  }

  function status(msg, ok) {
    const el = document.getElementById("edit-status");
    if (!el) return;
    el.textContent = msg || "";
    el.style.color = ok === false ? "#b91c1c" : "";
  }

  function render() {
    const filter = ((filterEl && filterEl.value) || "").trim().toLowerCase();
    const shown = rows.filter((row) => !filter || row.name.toLowerCase().includes(filter));
    const parts = [];
    if (parent && !filter) {
      parts.push(`<li><button type="button" data-dir="${escHtml(parent)}">..</button></li>`);
    }
    shown.forEach((row) => {
      parts.push(`<li><button type="button" data-dir="${escHtml(row.path)}">${escHtml(row.name)}</button></li>`);
    });
    ul.innerHTML = parts.join("") || "<li class='edit-hint'>no folders</li>";
    ul.querySelectorAll("[data-dir]").forEach((btn) => {
      btn.addEventListener("click", () => {
        setOpen(false);
        choose(btn.dataset.dir).catch((err) => status(String(err), false));
      });
    });
  }

  async function load(dir) {
    const r = await fetch(`/api/scripts/dirs?dir=${encodeURIComponent(dir)}`, { cache: "no-store" });
    const data = await r.json();
    if (!data.ok) {
      status(data.error || "cannot open directory", false);
      rows = [];
      parent = null;
      render();
      return null;
    }
    inp.value = data.dir || dir;
    inp.dataset.touched = "1";
    parent = data.parent || null;
    rows = data.folders || [];
    render();
    return data;
  }

  function choose(path) {
    inp.value = path;
    inp.dataset.touched = "1";
    if (filterEl) filterEl.value = "";
    return load(path);
  }

  inp.addEventListener("click", () => {
    setOpen(true);
    load((inp.value || DEFAULT_SCRIPTS_DIR).trim() || DEFAULT_SCRIPTS_DIR).catch((err) => status(String(err), false));
  });
  inp.addEventListener("keydown", (ev) => {
    if (ev.key === "Enter") {
      ev.preventDefault();
      setOpen(false);
      choose((inp.value || DEFAULT_SCRIPTS_DIR).trim() || DEFAULT_SCRIPTS_DIR).catch((err) => status(String(err), false));
    } else if (ev.key === "Escape") {
      setOpen(false);
    }
  });
  if (upBtn) {
    upBtn.addEventListener("click", () => {
      const go = parent
        ? Promise.resolve(parent)
        : load((inp.value || DEFAULT_SCRIPTS_DIR).trim() || DEFAULT_SCRIPTS_DIR).then(() => parent);
      go.then((dest) => {
        if (!dest) return;
        setOpen(false);
        choose(dest).catch((err) => status(String(err), false));
      }).catch((err) => status(String(err), false));
    });
  }
  if (filterEl) filterEl.addEventListener("input", () => render());
  document.addEventListener("mousedown", (ev) => {
    if (!open) return;
    if (ev.target === inp || inp.contains(ev.target)) return;
    if (panel.contains(ev.target)) return;
    if (upBtn && (ev.target === upBtn || upBtn.contains(ev.target))) return;
    setOpen(false);
  });
}
