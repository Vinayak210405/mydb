"use strict";
// mydb web console. Talks to the HTTP API served by mydb itself (see src/server/Api.h).

const $ = (sel) => document.querySelector(sel);
const esc = (s) => String(s).replace(/[&<>"']/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]));

const state = {
  schema: [],
  results: [],
  selected: -1,
  tree: null,
  selectedNode: null,
};

// ---------------------------------------------------------------------------
// API
// ---------------------------------------------------------------------------
async function api(path, options = {}) {
  let res;
  try {
    res = await fetch(path, options);
  } catch (e) {
    setConn(false);
    throw new Error("Cannot reach the mydb server. Is it still running?");
  }
  setConn(true);
  const body = await res.json().catch(() => ({}));
  if (!res.ok) throw new Error(body.error || `Request failed (${res.status})`);
  return body;
}

function setConn(up) {
  const el = $("#conn");
  el.classList.toggle("is-up", up);
  el.classList.toggle("is-down", !up);
  el.textContent = up ? "Connected" : "Server offline";
}

function toast(msg) {
  const t = document.createElement("div");
  t.className = "toast";
  t.setAttribute("role", "status");
  t.textContent = msg;
  document.body.appendChild(t);
  setTimeout(() => t.remove(), 3500);
}

// ---------------------------------------------------------------------------
// Views
// ---------------------------------------------------------------------------
function showView(name) {
  for (const tab of document.querySelectorAll(".view-tab")) {
    const on = tab.dataset.view === name;
    tab.classList.toggle("is-active", on);
    tab.setAttribute("aria-pressed", on);
  }
  for (const v of document.querySelectorAll(".view")) v.hidden = v.id !== `view-${name}`;
  if (name === "index") loadIndexView();
  if (name === "storage") loadStorageView();
}

// ---------------------------------------------------------------------------
// Schema sidebar
// ---------------------------------------------------------------------------
async function loadSchema() {
  try {
    const data = await api("/api/schema");
    state.schema = data.tables;
    renderSchema();
  } catch (e) {
    $("#schema").innerHTML = `<p class="empty">${esc(e.message)}</p>`;
  }
}

function renderSchema() {
  const el = $("#schema");
  if (!state.schema.length) {
    el.innerHTML = `<p class="empty">No tables yet. Create one with CREATE TABLE, or load the sample data below.</p>`;
    return;
  }
  el.innerHTML = state.schema.map((t) => `
    <div class="table-card">
      <button class="table-title" data-table="${esc(t.name)}" title="Show rows">
        <strong>${esc(t.name)}</strong>
        <span>${t.rows.toLocaleString()} rows, ${t.pages} ${t.pages === 1 ? "page" : "pages"}</span>
      </button>
      <ul class="cols">
        ${t.columns.map((c) => `
          <li class="${c.indexed ? "is-indexed" : ""}">
            <span>${esc(c.name)}${c.primary_key ? '<span class="tag">key</span>' : ""}${c.indexed && !c.primary_key ? '<span class="tag">indexed</span>' : ""}</span>
            <span class="type">${esc(c.type)}${c.nullable ? "" : " not null"}</span>
          </li>`).join("")}
      </ul>
    </div>`).join("");
  for (const b of el.querySelectorAll(".table-title")) {
    b.addEventListener("click", () => {
      setEditor(`SELECT * FROM ${b.dataset.table} LIMIT 50;`);
      showView("query");
      run();
    });
  }
}

// ---------------------------------------------------------------------------
// Query editor
// ---------------------------------------------------------------------------
function setEditor(sql) {
  $("#sql").value = sql;
  saveEditor();
}
function saveEditor() {
  try { localStorage.setItem("mydb.sql", $("#sql").value); } catch (_) { /* storage unavailable */ }
}
function restoreEditor() {
  try {
    const s = localStorage.getItem("mydb.sql");
    if (s) $("#sql").value = s;
  } catch (_) { /* storage unavailable */ }
}

function currentSql() {
  const ta = $("#sql");
  const selected = ta.value.slice(ta.selectionStart, ta.selectionEnd);
  return (selected.trim() ? selected : ta.value).trim();
}

async function run(sqlOverride) {
  const sql = sqlOverride ?? currentSql();
  if (!sql) {
    $("#result").innerHTML = `<p class="empty">Type a query first, for example SHOW TABLES;</p>`;
    return;
  }
  const btn = $("#run-btn");
  btn.disabled = true;
  btn.textContent = "Running…";
  try {
    const data = await api("/api/query", { method: "POST", headers: { "Content-Type": "text/plain" }, body: sql });
    state.results = data.results;
    // Show the last result that has rows; otherwise the last one (which is the error, if any).
    let pick = state.results.length - 1;
    if (state.results[pick].ok) {
      for (let i = state.results.length - 1; i >= 0; i--) if (state.results[i].columns.length) { pick = i; break; }
    }
    state.selected = pick;
    renderResults();
    if (state.results.some((r) => r.ok && ["ddl", "insert", "update", "delete"].includes(r.kind))) loadSchema();
  } catch (e) {
    state.results = [];
    $("#statement-log").innerHTML = "";
    $("#result").innerHTML = `<div class="error-box"><strong>Could not run the query</strong>${esc(e.message)}</div>`;
    $("#plan").innerHTML = "";
    $("#result-meta").textContent = "";
  } finally {
    btn.disabled = false;
    btn.textContent = "Run";
  }
}

function renderResults() {
  const log = $("#statement-log");
  const rs = state.results;
  // Only list statements when there is more than one, or when one failed.
  if (rs.length > 1 || !rs[0].ok) {
    log.innerHTML = rs.map((r, i) => `
      <button class="stmt ${r.ok ? "" : "is-error"} ${i === state.selected ? "is-selected" : ""}" data-i="${i}">
        <span class="mark" aria-hidden="true">${r.ok ? "✓" : "✕"}</span>
        <code>${esc(r.sql || "(script)")}</code>
        <span class="msg">${esc(r.message)}</span>
      </button>`).join("");
    for (const b of log.querySelectorAll(".stmt")) {
      b.addEventListener("click", () => { state.selected = +b.dataset.i; renderResults(); });
    }
  } else {
    log.innerHTML = "";
  }
  const r = rs[state.selected];
  renderResult(r);
  renderPlan(r);
}

function renderResult(r) {
  const meta = $("#result-meta");
  const body = $("#result");
  if (!r.ok) {
    meta.textContent = "";
    body.innerHTML = `<div class="error-box"><strong>The statement failed</strong>${esc(r.message)}</div>`;
    return;
  }
  meta.textContent = `${r.message}, ${r.elapsed_ms < 1 ? r.elapsed_ms.toFixed(2) : r.elapsed_ms.toFixed(1)} ms`;
  if (!r.columns.length) {
    body.innerHTML = `<div class="message-box">${esc(capitalize(r.message))}.</div>`;
    return;
  }
  if (!r.rows.length) {
    body.innerHTML = tableHtml(r, []) + `<p class="empty">No rows matched.</p>`;
    return;
  }
  const shown = r.rows.slice(0, 1000);
  body.innerHTML = tableHtml(r, shown) +
    (r.rows.length > shown.length ? `<p class="empty">Showing the first 1,000 of ${r.rows.length.toLocaleString()} rows.</p>` : "");
}

function tableHtml(r, rows) {
  const numeric = r.types.map((t) => t === "INT" || t === "FLOAT");
  const head = r.columns.map((c, i) => `<th class="${numeric[i] ? "num" : ""}">${esc(c)}<small>${esc(r.types[i] || "")}</small></th>`).join("");
  const bodyRows = rows.map((row) => `<tr>${row.map((v, i) => {
    if (v === null) return `<td class="null">null</td>`;
    const cls = numeric[i] ? "num" : "";
    let text = String(v);
    if (typeof v === "number" && r.types[i] === "FLOAT") text = Number.isInteger(v) ? v.toFixed(1) : String(+v.toFixed(4));
    return `<td class="${cls}">${esc(text)}</td>`;
  }).join("")}</tr>`).join("");
  return `<table class="grid"><thead><tr>${head}</tr></thead><tbody>${bodyRows}</tbody></table>`;
}

function capitalize(s) { return s ? s[0].toUpperCase() + s.slice(1) : s; }

function renderPlan(r) {
  const el = $("#plan");
  if (!r.ok || !r.plan) {
    el.innerHTML = `<p class="empty">${r.ok ? "This statement has no operator tree (it changes the schema or reads the catalog)." : "No plan: the statement failed before it ran."}</p>`;
    return;
  }
  const node = (n) => {
    const cls = ["op"];
    if (n.name === "IndexScan") cls.push("is-index");
    if (n.name.endsWith("Join")) cls.push("is-join");
    const rows = n.rows >= 0 ? `<span class="op-rows"><b>${n.rows.toLocaleString()}</b> ${n.rows === 1 ? "row" : "rows"}</span>` : `<span class="op-rows">not run</span>`;
    return `<li><div class="${cls.join(" ")}"><span class="op-name">${esc(n.name)}</span>${rows}${n.detail ? `<span class="op-detail">${esc(n.detail)}</span>` : ""}</div>${n.children.length ? `<ul>${n.children.map(node).join("")}</ul>` : ""}</li>`;
  };
  const executed = r.plan.rows >= 0;
  el.innerHTML = `<ul class="plan-tree">${node(r.plan)}</ul>` +
    `<p class="plan-note">${executed ? "Rows flow upward: each operator pulls from the one below it (the Volcano model)." : "EXPLAIN shows the plan without running it."}</p>`;
}

// ---------------------------------------------------------------------------
// B+ tree view
// ---------------------------------------------------------------------------
async function loadIndexView() {
  await loadSchema();
  const sel = $("#index-select");
  const prev = sel.value;
  const options = [];
  for (const t of state.schema) for (const ix of t.indexes) options.push(`${t.name}.${ix.column}`);
  if (!options.length) {
    sel.innerHTML = "";
    $("#tree").innerHTML = `<p class="empty">No indexes yet. A PRIMARY KEY on an INT column gets one automatically, or run CREATE INDEX ON table (column).</p>`;
    $("#index-meta").textContent = "";
    return;
  }
  sel.innerHTML = options.map((o) => `<option ${o === prev ? "selected" : ""}>${esc(o)}</option>`).join("");
  await loadTree();
}

async function loadTree() {
  const [table, column] = $("#index-select").value.split(".");
  try {
    state.tree = await api(`/api/index?table=${encodeURIComponent(table)}&column=${encodeURIComponent(column)}`);
  } catch (e) {
    $("#tree").innerHTML = `<p class="empty">${esc(e.message)}</p>`;
    return;
  }
  const t = state.tree;
  const nodes = t.levels.reduce((n, l) => n + l.length, 0);
  $("#index-meta").textContent = `${t.entries.toLocaleString()} keys, height ${t.height}, ${nodes} ${nodes === 1 ? "page" : "pages"}, up to ${t.max_keys} keys per page`;
  state.selectedNode = null;
  renderTree([]);
  $("#node-detail").innerHTML = "";
}

function nodeLabel(n) {
  if (!n.keys.length) return "empty";
  if (n.keys.length <= 3) return n.keys.join(" · ");
  return `${n.keys[0]} … ${n.keys[n.keys.length - 1]}`;
}

function renderTree(path, target) {
  const t = state.tree;
  const el = $("#tree");
  const onPath = new Set(path);
  el.innerHTML = `<div class="tree-inner">` + t.levels.map((level, depth) => `
    <div class="tree-level">
      ${level.map((n) => `
        <button class="bnode ${n.leaf ? "is-leaf" : ""} ${onPath.has(n.page) ? "on-path" : ""} ${n.page === target ? "is-target" : ""} ${n.page === state.selectedNode ? "is-selected" : ""}"
                data-page="${n.page}" title="Page ${n.page}">
          <span class="keys">${esc(nodeLabel(n))}</span>
          <span class="count">${n.leaf ? "leaf" : depth === 0 ? "root" : "internal"}, ${n.keys.length} ${n.keys.length === 1 ? "key" : "keys"}</span>
        </button>`).join("")}
    </div>`).join("") + `</div>`;
  drawEdges(onPath);
  for (const b of el.querySelectorAll(".bnode")) {
    b.addEventListener("click", () => {
      state.selectedNode = +b.dataset.page;
      for (const x of el.querySelectorAll(".bnode")) x.classList.toggle("is-selected", x === b);
      showNode(+b.dataset.page, null);
    });
  }
}

// Lines from each internal node to its children (only for children that were loaded).
function drawEdges(onPath) {
  const el = $("#tree");
  const box = el.getBoundingClientRect();
  const pos = new Map();
  for (const b of el.querySelectorAll(".bnode")) {
    const r = b.getBoundingClientRect();
    pos.set(+b.dataset.page, { x: r.left - box.left + el.scrollLeft + r.width / 2, top: r.top - box.top + el.scrollTop, bottom: r.bottom - box.top + el.scrollTop });
  }
  let lines = "";
  for (const level of state.tree.levels) {
    for (const n of level) {
      const p = pos.get(n.page);
      for (const c of n.children) {
        const q = pos.get(c);
        if (!p || !q) continue;
        const hot = onPath.has(n.page) && onPath.has(c);
        lines += `<line x1="${p.x}" y1="${p.bottom}" x2="${q.x}" y2="${q.top}" stroke="${hot ? "#D98E04" : "#B7C1CD"}" stroke-width="${hot ? 2.5 : 1}"/>`;
      }
    }
  }
  const svg = `<svg class="edges" width="${el.scrollWidth}" height="${el.scrollHeight}" aria-hidden="true">${lines}</svg>`;
  el.insertAdjacentHTML("afterbegin", svg);
}

// Same rule the engine uses (BTree::find_leaf_for_search): at each internal
// node go to child i, where i = number of keys strictly less than the search key.
function tracePath(key) {
  const byPage = new Map();
  for (const level of state.tree.levels) for (const n of level) byPage.set(n.page, n);
  const path = [];
  let cur = byPage.get(state.tree.levels[0][0].page);
  while (cur) {
    path.push(cur.page);
    if (cur.leaf) break;
    let i = 0;
    while (i < cur.keys.length && cur.keys[i] < key) i++;
    cur = byPage.get(cur.children[i]);
  }
  return path;
}

function findKey() {
  const raw = $("#index-key").value;
  if (!state.tree) return;
  if (raw === "" || !Number.isFinite(+raw)) {
    $("#node-detail").innerHTML = `<p class="empty">Enter a whole number to trace.</p>`;
    return;
  }
  const key = Math.trunc(+raw);
  const path = tracePath(key);
  const leaf = path[path.length - 1];
  state.selectedNode = leaf;
  renderTree(path, leaf);
  showNode(leaf, key, path.length);
  const target = $(`#tree .bnode[data-page="${leaf}"]`);
  if (target) target.scrollIntoView({ block: "nearest", inline: "center" });
}

function showNode(page, key, depth) {
  let node = null;
  for (const level of state.tree.levels) for (const n of level) if (n.page === page) node = n;
  if (!node) return;
  const keys = node.keys.map((k) => (k === key ? `<mark>${k}</mark>` : String(k))).join(", ");
  let headline = `Page ${page}, ${node.leaf ? "leaf" : "internal"} node with ${node.keys.length} ${node.keys.length === 1 ? "key" : "keys"}`;
  if (key !== null && key !== undefined) {
    const hits = node.keys.filter((k) => k === key).length;
    headline = hits
      ? `Found ${key}: ${depth} page reads from the root to leaf page ${page} (${hits} ${hits === 1 ? "entry" : "entries"})`
      : `${key} is not in the index. The search reads ${depth} pages and ends at leaf page ${page}.`;
  }
  $("#node-detail").innerHTML = `<div class="panel"><h2>${esc(headline)}</h2><p class="keylist">${keys || "(no keys)"}</p>${
    node.leaf ? `<p class="meta">Next leaf: ${node.next === null ? "none (last leaf)" : `page ${node.next}`}</p>` : `<p class="meta">Children: pages ${node.children.join(", ")}</p>`}</div>`;
}

// ---------------------------------------------------------------------------
// Storage view
// ---------------------------------------------------------------------------
async function loadStorageView() {
  await loadSchema();
  const sel = $("#pages-select");
  const prev = sel.value;
  sel.innerHTML = state.schema.map((t) => `<option ${t.name === prev ? "selected" : ""}>${esc(t.name)}</option>`).join("");
  if (state.schema.length) await loadPages();
  else {
    $("#page-grid").innerHTML = `<p class="empty">No tables yet.</p>`;
    $("#pages-meta").textContent = "";
  }
  await loadStats();
}

function fillColor(f) {
  // Interpolate from pale rule grey to navy ink.
  const a = [226, 231, 237], b = [27, 37, 51];
  const c = a.map((v, i) => Math.round(v + (b[i] - v) * Math.min(1, Math.max(0, f))));
  return `rgb(${c.join(",")})`;
}

async function loadPages() {
  const table = $("#pages-select").value;
  let data;
  try {
    data = await api(`/api/pages?table=${encodeURIComponent(table)}`);
  } catch (e) {
    $("#page-grid").innerHTML = `<p class="empty">${esc(e.message)}</p>`;
    return;
  }
  const usable = data.page_size - 16;
  const total = data.pages.reduce((s, p) => s + p.live, 0);
  $("#pages-meta").textContent = `${data.page_count} ${data.page_count === 1 ? "page" : "pages"}, ${total.toLocaleString()} live rows, ${(data.page_count * data.page_size / 1024).toLocaleString()} KB on disk`;
  if (!data.pages.length) {
    $("#page-grid").innerHTML = `<p class="empty">This table has no pages yet. Insert a row to allocate the first one.</p>`;
    return;
  }
  $("#page-grid").innerHTML = data.pages.map((p) => {
    const fill = 1 - p.free / usable;
    return `<div class="page-cell" tabindex="0" style="background:${fillColor(fill)}" data-info="${esc(`Page ${p.page}: ${p.live} live rows in ${p.slots} slots, ${p.free.toLocaleString()} bytes free (${Math.round(fill * 100)}% full)`)}"></div>`;
  }).join("") + (data.page_count > data.pages.length ? `<p class="empty">Showing the first ${data.pages.length} pages.</p>` : "");
  for (const c of document.querySelectorAll(".page-cell")) {
    const show = () => { $("#page-detail").textContent = c.dataset.info; };
    c.addEventListener("mouseenter", show);
    c.addEventListener("focus", show);
  }
  $("#page-detail").textContent = "";
}

async function loadStats() {
  let data;
  try {
    data = await api("/api/stats");
  } catch (e) {
    $("#stats").innerHTML = `<p class="empty">${esc(e.message)}</p>`;
    return;
  }
  if (!data.files.length) {
    $("#stats").innerHTML = `<p class="empty">No files yet.</p>`;
    return;
  }
  $("#stats").innerHTML = `<table class="grid"><thead><tr>
      <th>File</th><th class="num">Pages</th><th>Cache hit rate</th><th class="num">Hits</th><th class="num">Misses</th><th class="num">Disk reads</th><th class="num">Disk writes</th><th class="num">Frames used</th>
    </tr></thead><tbody>${data.files.map((f) => {
      const total = f.hits + f.misses;
      const rate = total ? f.hits / total : 0;
      return `<tr><td>${esc(f.name)}</td><td class="num">${f.pages}</td>
        <td><span class="bar"><span style="width:${(rate * 100).toFixed(1)}%"></span></span>${total ? (rate * 100).toFixed(1) + "%" : "no reads yet"}</td>
        <td class="num">${f.hits.toLocaleString()}</td><td class="num">${f.misses.toLocaleString()}</td>
        <td class="num">${f.disk_reads.toLocaleString()}</td><td class="num">${f.disk_writes.toLocaleString()}</td>
        <td class="num">${f.cached} of ${f.pool_size}</td></tr>`;
    }).join("")}</tbody></table>`;
}

// ---------------------------------------------------------------------------
// Sample data + example queries
// ---------------------------------------------------------------------------
function sampleScript() {
  // Deterministic pseudo-random data so every demo looks the same.
  let seed = 42;
  const rnd = () => (seed = (seed * 1103515245 + 12345) % 2147483648) / 2147483648;
  const pick = (a) => a[Math.floor(rnd() * a.length)];
  const first = ["Aarav", "Diya", "Kabir", "Meera", "Rohan", "Saanvi", "Arjun", "Isha", "Vihaan", "Anaya", "Dev", "Tara", "Nikhil", "Riya", "Kunal", "Zara", "Aditya", "Neha", "Yash", "Priya"];
  const last = ["Sharma", "Patel", "Iyer", "Reddy", "Khan", "Gupta", "Nair", "Singh", "Das", "Mehta", "Joshi", "Rao"];
  const cities = ["Pune", "Bengaluru", "Mumbai", "Delhi", "Hyderabad", "Chennai"];
  const depts = ["Engineering", "Data", "Design", "Sales", "Finance", "Operations", "Security", "Research"];

  const lines = [
    "DROP TABLE IF EXISTS assignments;",
    "DROP TABLE IF EXISTS projects;",
    "DROP TABLE IF EXISTS employees;",
    "DROP TABLE IF EXISTS departments;",
    "CREATE TABLE departments (id INT PRIMARY KEY, name VARCHAR NOT NULL, city VARCHAR);",
    "CREATE TABLE employees (id INT PRIMARY KEY, name VARCHAR NOT NULL, dept_id INT, salary INT, age INT, city VARCHAR);",
    "CREATE TABLE projects (id INT PRIMARY KEY, title VARCHAR NOT NULL, dept_id INT, budget FLOAT);",
    "CREATE TABLE assignments (employee_id INT, project_id INT, hours INT);",
    "INSERT INTO departments VALUES " + depts.map((d, i) => `(${i + 1}, '${d}', '${cities[i % cities.length]}')`).join(", ") + ";",
  ];
  const emps = [];
  for (let i = 1; i <= 800; i++) {
    const dept = 1 + Math.floor(rnd() * depts.length);
    const salary = 30000 + Math.floor(rnd() * 90) * 1000 + dept * 1500;
    const age = 21 + Math.floor(rnd() * 40);
    const city = rnd() < 0.08 ? "NULL" : `'${pick(cities)}'`;
    emps.push(`(${i}, '${pick(first)} ${pick(last)}', ${dept}, ${salary}, ${age}, ${city})`);
  }
  for (let i = 0; i < emps.length; i += 200) lines.push(`INSERT INTO employees VALUES ${emps.slice(i, i + 200).join(", ")};`);
  const adjectives = ["Atlas", "Beacon", "Comet", "Delta", "Ember", "Falcon", "Granite", "Harbor", "Ion", "Juniper"];
  const projects = [];
  for (let i = 1; i <= 40; i++) projects.push(`(${i}, '${pick(adjectives)} ${i}', ${1 + Math.floor(rnd() * depts.length)}, ${(50 + Math.floor(rnd() * 950)) * 1000}.0)`);
  lines.push(`INSERT INTO projects VALUES ${projects.join(", ")};`);
  const asg = [];
  for (let i = 0; i < 600; i++) asg.push(`(${1 + Math.floor(rnd() * 800)}, ${1 + Math.floor(rnd() * 40)}, ${4 + Math.floor(rnd() * 37)})`);
  for (let i = 0; i < asg.length; i += 200) lines.push(`INSERT INTO assignments VALUES ${asg.slice(i, i + 200).join(", ")};`);
  lines.push("CREATE INDEX ON employees (dept_id);");
  lines.push("CREATE INDEX ON assignments (employee_id);");
  return lines.join("\n");
}

const EXAMPLES = [
  { title: "Look up one row by key", note: "Uses the B+ tree on employees.id", sql: "SELECT * FROM employees WHERE id = 250;" },
  { title: "Range scan on an index", note: "Leaf pages walked left to right", sql: "SELECT id, name, salary FROM employees WHERE id BETWEEN 100 AND 120;" },
  { title: "Join three tables", note: "Two hash joins built from the ON clauses", sql: "SELECT e.name, p.title, a.hours\nFROM assignments a\nJOIN employees e ON e.id = a.employee_id\nJOIN projects p ON p.id = a.project_id\nWHERE a.hours > 35\nORDER BY a.hours DESC\nLIMIT 15;" },
  { title: "Group and aggregate", note: "Hash aggregation, then HAVING and ORDER BY", sql: "SELECT d.name AS department, COUNT(*) AS people, AVG(e.salary) AS avg_salary, MAX(e.age) AS oldest\nFROM employees e JOIN departments d ON d.id = e.dept_id\nGROUP BY d.name\nHAVING COUNT(*) > 50\nORDER BY avg_salary DESC;" },
  { title: "Filter without an index", note: "Compare the plan with the lookups above", sql: "SELECT name, city, age FROM employees\nWHERE city = 'Pune' AND age < 25\nORDER BY age;" },
  { title: "Update rows, keep indexes in sync", note: "Then rerun the key lookup", sql: "UPDATE employees SET salary = salary + 5000 WHERE dept_id = 7;\nSELECT dept_id, AVG(salary) FROM employees GROUP BY dept_id ORDER BY 1;" },
  { title: "See a plan without running it", note: "EXPLAIN", sql: "EXPLAIN SELECT e.name FROM employees e JOIN departments d ON d.id = e.dept_id WHERE e.id < 50;" },
];

function renderExamples() {
  $("#examples").innerHTML = EXAMPLES.map((e, i) => `
    <li><button class="example" data-i="${i}"><strong>${esc(e.title)}</strong><span>${esc(e.note)}</span></button></li>`).join("");
  for (const b of document.querySelectorAll(".example")) {
    b.addEventListener("click", () => {
      setEditor(EXAMPLES[+b.dataset.i].sql);
      showView("query");
      run();
    });
  }
}

async function loadSample() {
  const btn = $("#load-sample");
  if (state.schema.some((t) => ["employees", "departments", "projects", "assignments"].includes(t.name)) &&
      !confirm("This replaces the employees, departments, projects and assignments tables. Continue?")) return;
  btn.disabled = true;
  btn.textContent = "Loading sample data…";
  try {
    const data = await api("/api/query", { method: "POST", headers: { "Content-Type": "text/plain" }, body: sampleScript() });
    const failed = data.results.find((r) => !r.ok);
    if (failed) throw new Error(failed.message);
    await loadSchema();
    toast("Loaded 4 tables with 1,448 rows. Pick a query on the left to try it.");
  } catch (e) {
    toast(`Sample data failed to load: ${e.message}`);
  } finally {
    btn.disabled = false;
    btn.textContent = "Load sample company data";
  }
}

// ---------------------------------------------------------------------------
// Wiring
// ---------------------------------------------------------------------------
function init() {
  restoreEditor();
  renderExamples();
  loadSchema();

  for (const tab of document.querySelectorAll(".view-tab")) tab.addEventListener("click", () => showView(tab.dataset.view));
  $("#run-btn").addEventListener("click", () => run());
  $("#explain-btn").addEventListener("click", () => {
    const sql = currentSql().replace(/^\s*explain\s+/i, "");
    if (sql) run(`EXPLAIN ${sql}`);
  });
  $("#sql").addEventListener("input", saveEditor);
  $("#sql").addEventListener("keydown", (e) => {
    if ((e.ctrlKey || e.metaKey) && e.key === "Enter") { e.preventDefault(); run(); }
  });
  $("#refresh-schema").addEventListener("click", loadSchema);
  $("#load-sample").addEventListener("click", loadSample);
  $("#index-select").addEventListener("change", loadTree);
  $("#index-find").addEventListener("click", findKey);
  $("#index-key").addEventListener("keydown", (e) => { if (e.key === "Enter") findKey(); });
  $("#pages-select").addEventListener("change", loadPages);
  window.addEventListener("resize", () => {
    if (state.tree && !$("#view-index").hidden) {
      const target = $("#tree .bnode.is-target");
      const path = [...document.querySelectorAll("#tree .bnode.on-path")].map((b) => +b.dataset.page);
      renderTree(path, target ? +target.dataset.page : undefined);
    }
  });
}

init();
