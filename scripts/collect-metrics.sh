#!/usr/bin/env bash
# Recolecta el set MÍNIMO de métricas de una corrida y las persiste como evidencia
# versionable en UN solo CSV:
#   data/runs/faseN-<cell>-<run_id>.runshow.txt   salida cruda (JSON) de `aitl run-show`
#   data/metricas.csv                             una fila por (cell,fase)
#
# CSV único (reemplaza al esquema de 18 columnas + el fases-*.csv aparte):
#   fecha,cell,fase,run_id,status,gate,dur_s,iters,tok_out,tok_total,costo_pond,verify_1er,imagen_ok,adr,notas
#
# `costo_pond` = costo comparable en USD. En Cara B (run-host/claude-code) es el
# host_meta.cost_usd real que reporta Claude (ya pondera el caché a su tarifa con
# descuento) — más simple y directo que el estimado por tokens. Ver METRICAS.md.
#
# Uso:
#   scripts/collect-metrics.sh <run_id> <fase> <cell> [gate] [dur_s] [status_fallback]
#   - run_id vacío o "-"  → corrida degradada (Mongo caído): no hay run-show; se escribe
#     igual la fila con cell/fase/status/gate/dur_s (el trabajo del agente NO se pierde).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [ $# -lt 3 ]; then
  echo "Uso: scripts/collect-metrics.sh <run_id|-> <fase 0-5> <cell> [gate] [dur_s] [status_fallback]" >&2
  exit 1
fi
RUN_ID="$1"; FASE="$2"; CELL="$3"; GATE="${4:-}"; DUR_S="${5:-}"; STATUS_FB="${6:-}"
[ "${RUN_ID}" = "-" ] && RUN_ID=""

CSV="${ROOT}/data/metricas.csv"
RAW_DIR="${ROOT}/data/runs"; mkdir -p "${RAW_DIR}"
HEADER="fecha,cell,fase,run_id,status,gate,dur_s,iters,tok_out,tok_total,costo_pond,verify_1er,imagen_ok,adr,notas"
[ -f "${CSV}" ] || echo "${HEADER}" > "${CSV}"

STATUS="${STATUS_FB}"; ITERS=""; TOK_OUT=""; TOK_TOT=""; COSTO=""
if echo "${RUN_ID}" | grep -qE '^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$'; then
  RAW="${RAW_DIR}/fase${FASE}-${CELL}-${RUN_ID}.runshow.txt"
  aitl run-show "${RUN_ID}" | tee "${RAW}" >/dev/null
  # Parseo robusto del JSON de run-show (la fuente de verdad sigue siendo el .runshow.txt).
  read -r STATUS ITERS TOK_OUT TOK_TOT COSTO < <(python3 - "${RAW}" <<'PY'
import json, sys
try:
    d = json.load(open(sys.argv[1]))
except Exception:
    print("     ."); sys.exit(0)
hm = d.get("host_meta") or {}
tk = d.get("tokens") or {}
def s(v): return "" if v is None else str(v)
cost = hm.get("cost_usd")
cost_s = "" if cost is None else f"{round(float(cost), 4)}"
print(s(d.get("status")), s(d.get("iters")), s(tk.get("output")), s(tk.get("total")), cost_s)
PY
)
  [ "${STATUS}" = "." ] && STATUS="${STATUS_FB}"
else
  echo "→ corrida sin run_id (degradada/timeout): fila con status/gate/dur_s, sin métricas de run-show" >&2
fi

echo "$(date -Iseconds),${CELL},${FASE},${RUN_ID},${STATUS},${GATE},${DUR_S},${ITERS},${TOK_OUT},${TOK_TOT},${COSTO},,,," >> "${CSV}"
echo "→ fila añadida a data/metricas.csv (cell=${CELL} fase=${FASE} gate=${GATE:-—} costo=${COSTO:-—})"
echo "  columnas de juicio a mano: verify_1er, imagen_ok, adr, notas"

# Regenerar el resumen si existe el generador
[ -x "${ROOT}/scripts/resumen.sh" ] && bash "${ROOT}/scripts/resumen.sh" || true
