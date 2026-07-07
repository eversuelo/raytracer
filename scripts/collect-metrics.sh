#!/usr/bin/env bash
# Recolecta las métricas primarias (M1–M15) de una corrida y las persiste como
# evidencia versionable:
#   data/runs/faseN-COND-<run_id>.runshow.txt   salida cruda de `aitl run-show`
#   data/metricas.csv                           una fila por celda (hoja §9 de METRICAS.md)
#
# Uso: scripts/collect-metrics.sh <run_id> <fase> <condición> [método]
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [ $# -lt 3 ]; then
  echo "Uso: scripts/collect-metrics.sh <run_id> <fase 0-5> <condición> [loop|host]" >&2
  echo "  El run_id es el UUID que imprime 'aitl run' (o búscalo en data/logs/*.log)." >&2
  exit 1
fi
RUN_ID="$1"; FASE="$2"; COND="$3"; METHOD="${4:-loop}"
echo "${RUN_ID}" | grep -qE '^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$' \
  || { echo "✗ '${RUN_ID}' no parece un run_id (UUID). Orden: <run_id> <fase> <condición>." >&2; exit 1; }
CSV="${ROOT}/data/metricas.csv"
RAW_DIR="${ROOT}/data/runs"; mkdir -p "${RAW_DIR}"
RAW="${RAW_DIR}/fase${FASE}-${COND}-${RUN_ID}.runshow.txt"

aitl run-show "${RUN_ID}" | tee "${RAW}"

# Extracción tolerante: primer número que aparezca tras la etiqueta (vacío si no está).
# La fuente de verdad es SIEMPRE el .runshow.txt crudo + la colección runs en Mongo.
num() { grep -iE "$1" "${RAW}" | grep -oE '[0-9]+([.][0-9]+)?' | head -1 || true; }

DUR="$(num 'duration')"
ITERS="$(num '\biters?\b|iteraci')"
TOOLS="$(num 'tool[_ ]?calls')"
TOK_IN="$(num 'input')"
TOK_OUT="$(num 'output')"
TOK_TOT="$(num 'total')"
GATES="$(num 'gate[_ ]?denials|denegaci')"
SUPERV="$(num 'supervision')"
STATUS="$(grep -oiE 'status[^a-z]*(done|error)' "${RAW}" | grep -oiE 'done|error' | head -1 || true)"

[ -f "${CSV}" ] || echo "fecha,fase,cond,metodo,run_id,status,duration_ms,iters,tool_calls,tokens_input,tokens_output,tokens_total,gate_denials,supervision_min,verify_1er_intento,imagen_ok,adr,notas" > "${CSV}"
echo "$(date -Iseconds),${FASE},${COND},${METHOD},${RUN_ID},${STATUS},${DUR},${ITERS},${TOOLS},${TOK_IN},${TOK_OUT},${TOK_TOT},${GATES},${SUPERV},,,," >> "${CSV}"

echo "→ fila añadida a data/metricas.csv (completa a mano: verify_1er_intento, imagen_ok, adr)"
echo "  ⚠ tokens_input crudo NO se compara entre condiciones (regla #4, METRICAS.md §3 M26–M29)"

# Regenerar el resumen si existe el generador
[ -x "${ROOT}/scripts/resumen.sh" ] && bash "${ROOT}/scripts/resumen.sh" || true
