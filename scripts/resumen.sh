#!/usr/bin/env bash
# Genera RESUMEN-METRICAS.md desde el CSV único data/metricas.csv: matriz por
# (celda × fase) con el set mínimo de métricas y deltas C0 − C2 por fase/modelo.
# Corre solo, o automáticamente al final de collect-metrics.sh.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CSV="${ROOT}/data/metricas.csv"
OUT="${ROOT}/RESUMEN-METRICAS.md"

[ -f "${CSV}" ] || { echo "aún no hay ${CSV} — corre una celda primero"; exit 0; }

# CSV: fecha,cell,fase,run_id,status,gate,dur_s,iters,tok_out,tok_total,costo_pond,verify_1er,imagen_ok,adr,notas
# LC_ALL=C: en locales con coma decimal (es_ES) awk lee "0.60" como 0 y arruinaría los
# deltas de costo/float — forzamos el punto como separador decimal.
LC_ALL=C awk -F',' -v date="$(date -Iseconds)" '
NR==1 { next }
{
  fase=$3; cell=$2; key=fase SUBSEP cell
  # última corrida por (fase,cell) — las inválidas quedan en el CSV como historia
  run[key]=$4; status[key]=$5; gate[key]=$6; dur[key]=$7; iters[key]=$8
  tokout[key]=$9; toktot[key]=$10; costo[key]=$11; verify1[key]=$12; imgok[key]=$13; adr[key]=$14
  fases[fase]=1
}
function v(a,f,c,  k){ k=f SUBSEP c; return (k in a && a[k]!="") ? a[k] : "—" }
function has(f,c,   k){ k=f SUBSEP c; return (k in run) }
function delta(a,f,c0,c2,  k0,k2){
  k0=f SUBSEP c0; k2=f SUBSEP c2
  if ((k0 in a) && (k2 in a) && a[k0]!="" && a[k2]!="") return sprintf("%s", a[k0]-a[k2])
  return "—"
}
END {
  print "# Resumen de métricas — experimento raytracer (C0 vs C2)"
  print ""
  print "> Generado por `scripts/resumen.sh` el " date " — **no editar a mano**."
  print "> Fuente única: `data/metricas.csv` (última corrida por celda) + crudos JSON en `data/runs/`."
  print "> `costo_pond` = costo comparable en USD (Cara B: host_meta.cost_usd real de Claude)."
  print ""
  print "## Matriz por celda (fase × celda)"
  print ""
  print "| Fase | Celda | run_id | status | gate | dur_s | iters | tok_out | tok_total | costo_pond (USD) | verify 1er | imagen_ok | ADR |"
  print "|---|---|---|---|---|---|---|---|---|---|---|---|---|"
  n=split("0 1 2 3 4 5", sf, " ")
  ncell=split("c0-bare@sonnet c2-memory@sonnet c0-bare@opus c2-memory@opus", cc, " ")
  for (i=1; i<=n; i++) {
    f=sf[i]; if (!(f in fases)) continue
    for (j=1; j<=ncell; j++) {
      c=cc[j]; if (!has(f,c)) continue
      rid=v(run,f,c); short=(rid=="—" || rid=="")? "—" : substr(rid,1,8)"…"
      printf "| %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s |\n", \
        f, c, short, v(status,f,c), v(gate,f,c), v(dur,f,c), v(iters,f,c), \
        v(tokout,f,c), v(toktot,f,c), v(costo,f,c), v(verify1,f,c), v(imgok,f,c), v(adr,f,c)
    }
  }
  print ""
  print "## Deltas C0 − C2 por fase y modelo (positivo = el harness ahorró)"
  print ""
  print "| Fase | Modelo | Δdur_s | Δiters | Δcosto_pond (USD) |"
  print "|---|---|---|---|---|"
  nm=split("sonnet opus", mm, " ")
  for (i=1; i<=n; i++) {
    f=sf[i]; if (!(f in fases)) continue
    for (k=1; k<=nm; k++) {
      m=mm[k]; c0="c0-bare@" m; c2="c2-memory@" m
      if (!has(f,c0) && !has(f,c2)) continue
      printf "| %s | %s | %s | %s | %s |\n", f, m, \
        delta(dur,f,c0,c2), delta(iters,f,c0,c2), delta(costo,f,c0,c2)
    }
  }
  print ""
  print "## Recordatorios de validez"
  print ""
  print "- Set mínimo a propósito: `tok_input` crudo (no comparable por caché) queda solo en"
  print "  los JSON de `data/runs/`; aquí manda `costo_pond` (USD real de Cara B)."
  print "- Corridas degradadas (Mongo caído): run_id vacío pero status/gate/dur_s registrados."
  print "- Columnas de juicio (`verify_1er`, `imagen_ok`, `ADR`) se editan en el CSV, no aquí."
}' "${CSV}" > "${OUT}"

echo "→ ${OUT} regenerado"
