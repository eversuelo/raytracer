#!/usr/bin/env bash
# Recolecta las métricas primarias (M1–M15) de una corrida y las persiste como
# evidencia versionable:
#   data/runs/faseN-COND-<run_id>.runshow.txt   salida cruda de `aitl run-show`
#   data/metricas.csv                           una fila por celda (hoja §9 de METRICAS.md)
#
# Uso: scripts/collect-metrics.sh <run_id> <fase> <condición> [método]
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RUN_ID="${1:?run_id}"; FASE="${2:?fase}"; COND="${3:?condición}"; METHOD="${4:-loop}"
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
