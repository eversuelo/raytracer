// Recolecta los runs de SUB-AGENTES (host:claude-code) de una celda orquestada y los
// añade a data/metricas.csv como filas cell=<cell>-session, una por sesión, con la
// fase mapeada por la ventana temporal del run del orquestador (model=lmstudio) que
// la delegó. Los runs fuera de toda ventana (p. ej. el agente del resumen) van con
// fase "-". Idempotente: no duplica run_ids ya presentes en el CSV.
//
// Uso: node scripts/collect-sessions.mjs <aitl_root> <project> <cell> [csv]
//   aitl_root = raíz del harness (para su node_modules/mongodb)
import { readFileSync, appendFileSync } from "node:fs";

const [aitlRoot, project, cell, csvArg] = process.argv.slice(2);
if (!aitlRoot || !project || !cell) {
  console.error("Uso: collect-sessions.mjs <aitl_root> <project> <cell> [csv]");
  process.exit(1);
}
const CSV = csvArg ?? new URL("../data/metricas.csv", import.meta.url).pathname;
const { MongoClient } = await import(`${aitlRoot}/node_modules/mongodb/lib/index.js`);
const cfg = JSON.parse(readFileSync(`${process.env.HOME}/.aitl/config.json`, "utf8"));
const client = new MongoClient(process.env.MONGODB_URI ?? cfg.MONGODB_URI);
await client.connect();
const db = client.db(process.env.MONGODB_DB ?? cfg.MONGODB_DB ?? "aitl");

const all = await db.collection("runs").find({ project }).sort({ started_at: 1 }).toArray();
const orq = all.filter(r => r.model === "lmstudio");
const subs = all.filter(r => String(r.model).startsWith("host:"));
const existing = readFileSync(CSV, "utf8");
let added = 0;
for (const s of subs) {
  if (existing.includes(String(s._id))) continue;
  const fase = orq.findIndex(o => s.started_at >= o.started_at && (!o.ended_at || s.started_at <= o.ended_at));
  const hm = s.host_meta || {}, tk = s.token_usage || {};
  const dur = s.ended_at && s.started_at ? Math.round((s.ended_at - s.started_at) / 1000) : "";
  const cost = hm.cost_usd != null ? Number(hm.cost_usd).toFixed(4) : "";
  const nota = fase >= 0
    ? `sub-agente delegado por ${String(orq[fase]._id).slice(0, 8)}`
    : "run-host fuera de fase (p. ej. resumen)";
  const row = `${new Date(s.started_at).toISOString()},${cell}-session,${fase >= 0 ? fase : "-"},${s._id},${s.status},,${dur},${s.iters ?? ""},${tk.output ?? ""},${(tk.input ?? 0) + (tk.output ?? 0)},${cost},,,,${nota}`;
  appendFileSync(CSV, row + "\n");
  console.log(`+ sesión ${String(s._id).slice(0, 8)} → fase ${fase >= 0 ? fase : "-"} (${s.status}, ${dur}s)`);
  added++;
}
console.log(`→ ${added} sesiones de sub-agente añadidas al CSV (cell=${cell}-session)`);
await client.close();
