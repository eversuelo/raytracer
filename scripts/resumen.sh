#!/usr/bin/env bash
# Genera RESUMEN-METRICAS.md a partir de data/metricas.csv: matriz por celda y
# deltas C0 vs C2 (M49–M51 del catálogo). Corre solo, o automáticamente al final
# de collect-metrics.sh.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CSV="${ROOT}/data/metricas.csv"
OUT="${ROOT}/RESUMEN-METRICAS.md"

[ -f "${CSV}" ] || { echo "aún no hay ${CSV} — corre una celda primero"; exit 0; }

awk -F',' -v date="$(date -Iseconds)" '
NR==1 { next }
{
  fase=$2; cond=$3
  key=fase SUBSEP cond
  # nos quedamos con la corrida MÁS RECIENTE por celda (las inválidas quedan en el CSV como historia)
  run[key]=$5; status[key]=$6; dur[key]=$7; iters[key]=$8; tools[key]=$9
  tokout[key]=$11; toktot[key]=$12; gates[key]=$13; superv[key]=$14
  verify1[key]=$15; imgok[key]=$16; adr[key]=$17
  fases[fase]=1
}
function cell(f,c,   k){ k=f SUBSEP c; return (k in run) ? run[k] : "" }
function v(arr,f,c,  k){ k=f SUBSEP c; return (k in arr && arr[k]!="") ? arr[k] : "—" }
function delta(arr,f,a,b,  ka,kb){
  ka=f SUBSEP a; kb=f SUBSEP b
  if (ka in arr && kb in arr && arr[ka]!="" && arr[kb]!="") return arr[ka]-arr[kb]
  return "—"
}
END {
  print "# Resumen de métricas — experimento raytracer (C0 vs C2)"
  print ""
  print "> Generado por `scripts/resumen.sh` el " date " — **no editar a mano**."
  print "> Fuente: `data/metricas.csv` (última corrida por celda) + crudos en `data/runs/`."
  print "> Definiciones y reglas de comparación: [`METRICAS.md`](METRICAS.md) (M1–M61)."
  print ""
  print "## Matriz por celda (fase × condición)"
  print ""
  print "| Fase | Cond | run_id | status | dur_ms (M1) | iters (M5) | tools (M6) | tok_out (M26) | tok_total (M29) | gates (M7) | superv_min (M10) | verify 1er (M52) | imagen_ok (M44) | ADR |"
  print "|---|---|---|---|---|---|---|---|---|---|---|---|---|---|"
  n=split("0 1 2 3 4 5 5a 5b", sf, " ")
  split("c0-bare c2-memory c2-orchestrator", conds, " ")
  for (i=1; i<=n; i++) {
    f=sf[i]
    if (!(f in fases)) continue
    for (j=1; j<=3; j++) {
      c=conds[j]
      if (cell(f,c)=="" ) continue
      printf "| %s | %s | `%.8s…` | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s |\n", \
        f, c, cell(f,c), v(status,f,c), v(dur,f,c), v(iters,f,c), v(tools,f,c), \
        v(tokout,f,c), v(toktot,f,c), v(gates,f,c), v(superv,f,c), \
        v(verify1,f,c), v(imgok,f,c), v(adr,f,c)
    }
  }
  print ""
  print "## Deltas C0 − C2 por fase (M49–M51; positivo = el harness ahorró)"
  print ""
  print "| Fase | Δdur vs c2-memory | Δdur vs c2-orch | Δiters vs c2-memory | Δiters vs c2-orch | Δtok_total vs c2-memory | Δtok_total vs c2-orch |"
  print "|---|---|---|---|---|---|---|"
  for (i=1; i<=n; i++) {
    f=sf[i]
    if (!(f in fases)) continue
    printf "| %s | %s | %s | %s | %s | %s | %s |\n", f, \
      delta(dur,f,"c0-bare","c2-memory"),   delta(dur,f,"c0-bare","c2-orchestrator"), \
      delta(iters,f,"c0-bare","c2-memory"), delta(iters,f,"c0-bare","c2-orchestrator"), \
      delta(toktot,f,"c0-bare","c2-memory"),delta(toktot,f,"c0-bare","c2-orchestrator")
  }
  print ""
  print "## Recordatorios de validez"
  print ""
  print "- `tokens_input` crudo no aparece aquí a propósito: no es comparable entre"
  print "  condiciones (caché). Usar las vistas M26–M29 desde los crudos de `data/runs/`."
  print "- Celdas con corridas invalidadas: la historia queda en `data/metricas.csv`;"
  print "  esta tabla muestra la última corrida por celda."
  print "- Columnas manuales (`verify_1er`, `imagen_ok`, `ADR`) se editan en el CSV,"
  print "  no aquí — este archivo se regenera."
}' "${CSV}" > "${OUT}"

echo "→ ${OUT} regenerado"
