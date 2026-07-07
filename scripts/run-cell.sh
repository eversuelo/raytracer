#!/usr/bin/env bash
# Corre UNA celda del experimento (fase × condición) de punta a punta:
# rama → plantilla de condición → prompt de la spec → corrida → métricas.
#
# Uso:
#   scripts/run-cell.sh <fase 0-5> <c0-bare|c2-memory|c2-orchestrator> [loop|host]
#     loop (default)  = aitl run   (el loop del harness es el agente)
#     host            = aitl run-host --host claude-code (Claude Code es el agente)
#
# Todo es C++ y algunos programas son terminales interactivos: el gate corre con
# stdin cerrado (</dev/null) — los inputs interactivos se scriptean en check.sh.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FASE="${1:?fase 0-5}"
COND="${2:?c0-bare | c2-memory | c2-orchestrator}"
METHOD="${3:-loop}"
BRANCH="fase-${FASE}/${COND}"
PHASE_DIR="start/sdd/phase-0${FASE}"
SPEC="${ROOT}/${PHASE_DIR}/spec.md"
STAMP="$(date +%Y%m%d-%H%M%S)"
LOG_DIR="${ROOT}/data/logs"; mkdir -p "${LOG_DIR}"
LOG="${LOG_DIR}/fase${FASE}-${COND}-${STAMP}.log"

die() { echo "✗ $*" >&2; exit 1; }

# ── Preflight: hace falta un backend de modelo (salvo método host) ───────────
if [ "${METHOD}" != "host" ] && aitl models 2>&1 | grep -q "Ningún LLM configurado"; then
  die "ningún LLM configurado para 'aitl run'. Opciones:
  - export ANTHROPIC_API_KEY=sk-ant-...          (API de Anthropic)
  - lms server start && export LMSTUDIO_MODEL=…  (LM Studio local; obligatorio para c2-orchestrator)
  - o corre esta celda con Claude Code como agente (no necesita key):
      scripts/run-cell.sh ${FASE} ${COND} host
  Verifica con: aitl models"
fi

# ── Validez experimental ─────────────────────────────────────────────────────
[ -f "${SPEC}" ] || die "no existe la spec ${SPEC}"
grep -q "status: approved" "${SPEC}" \
  || die "spec de fase ${FASE} NO aprobada (status: approved) — regla de validez #1"
case "${COND}" in c0-bare|c2-memory|c2-orchestrator) ;; *) die "condición inválida: ${COND}";; esac

# ── 1. Rama de la celda (nace del punto actual = resultado aceptado de N−1) ──
cd "${ROOT}"
git rev-parse --verify -q "${BRANCH}" >/dev/null \
  && git checkout -q "${BRANCH}" \
  || git checkout -q -b "${BRANCH}"
echo "→ rama ${BRANCH}"

# ── 2. Sembrar el proyecto base en start/ (solo la primera vez en la rama) ───
[ -f start/rt.cpp ]   || cp start/sdd/base/rt.cpp start/rt.cpp
[ -f start/Makefile ] || cp start/sdd/base/Makefile start/Makefile
mkdir -p start/out

# ── 3. Instalar la plantilla de la condición ─────────────────────────────────
cp "start/conditions/${COND}/CLAUDE.md" start/CLAUDE.md
mkdir -p start/.claude
cp "start/conditions/${COND}/.claude/settings.json" start/.claude/settings.json
if [ "${COND}" != "c0-bare" ] && [ -f aitl-raytracer/.mcp.json ]; then
  cp aitl-raytracer/.mcp.json start/.mcp.json   # gitignored: lleva credenciales
fi
echo "→ plantilla ${COND} instalada en start/"

# ── 4. Prompt textual de la spec (regla de validez #2: idéntico entre celdas) ─
PROMPT="$(awk '/^## Prompt de tarea/{f=1;next} /^## /{f=0} f' "${SPEC}")"
[ -n "${PROMPT}" ] || die "la spec no tiene sección '## Prompt de tarea'"

# ── 5. Gate objetivo ──────────────────────────────────────────────────────────
VERIFY="make"
[ -f "${ROOT}/${PHASE_DIR}/check.sh" ] && VERIFY="make && bash sdd/phase-0${FASE}/check.sh"
echo "→ gate: ${VERIFY}"

# ── 6. Lanzar la corrida (cwd = start/) ──────────────────────────────────────
# VERBOSE=1     → añade --stream: ves la respuesta del modelo en vivo en el terminal.
#                 ⚠ Con LM Studio viejo los turnos streameados reportan 0 tokens —
#                 úsalo para depurar, NO para celdas oficiales.
# TIMEOUT_MIN=N → freno duro de la celda (default 30 min): corta loops desbocados
#                 antes de que cocinen la máquina.
cd "${ROOT}/start"
TIMEOUT_MIN="${TIMEOUT_MIN:-30}"
if [ "${METHOD}" = "host" ]; then
  CMD=(aitl run-host "${PROMPT}" --project aitl-raytracer --host claude-code --cwd "${ROOT}/start")
else
  case "${COND}" in
    c0-bare)         FLAGS=(--bare) ;;
    c2-memory)       FLAGS=() ;;
    c2-orchestrator) FLAGS=(--model lmstudio --mcp) ;;
  esac
  CMD=(aitl run "${PROMPT}" --project aitl-raytracer "${FLAGS[@]+"${FLAGS[@]}"}" --verify-cmd "${VERIFY}")
  if [ "${VERBOSE:-0}" = "1" ]; then
    CMD+=(--stream)
    echo "⚠ VERBOSE: --stream activo — la respuesta del modelo se ve en vivo, pero los"
    echo "  tokens pueden reportarse en 0 con LM Studio viejo (no usar en celdas oficiales)."
  fi
fi
echo "→ modelo activo: $(aitl models 2>/dev/null | grep -- '← activo' | sed 's/^ *//' || true)"
echo "→ ejecutando (tope ${TIMEOUT_MIN} min): ${CMD[0]} ${CMD[1]:+«prompt de la spec»} ${CMD[*]:2}"
echo "→ salida en vivo del lado servidor: ventana Developer/Server de LM Studio"
set +e
timeout --foreground "$((TIMEOUT_MIN * 60))" "${CMD[@]}" </dev/null 2>&1 | tee "${LOG}"
RC=${PIPESTATUS[0]}
set -e
if [ "${RC}" = "124" ]; then
  echo "⚠ TIMEOUT: la celda superó ${TIMEOUT_MIN} min y fue cortada."
  echo "  Si era esperado, relanza con TIMEOUT_MIN=60 (o más). La corrida parcial queda en Mongo."
fi

# ── 7. Extraer run_id y recolectar métricas ──────────────────────────────────
RUN_ID="$(grep -oiE '[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}' "${LOG}" | tail -1 || true)"
if [ -n "${RUN_ID}" ]; then
  echo "→ run_id: ${RUN_ID}"
  bash "${ROOT}/scripts/collect-metrics.sh" "${RUN_ID}" "${FASE}" "${COND}" "${METHOD}"
else
  echo "⚠ no pude extraer el run_id del log (${LOG})."
  echo "  Recolecta manualmente: scripts/collect-metrics.sh <run_id> ${FASE} ${COND} ${METHOD}"
fi

echo "→ log completo: ${LOG}"
echo "→ celda fase ${FASE} × ${COND} terminada (exit ${RC}). Commitea el resultado en ${BRANCH}."
exit "${RC}"
