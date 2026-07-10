#!/usr/bin/env bash
# Re-corre un RANGO de fases del curso (p. ej. 3 5) con la condición c2-orch-claude
# (orquestador Claude sonnet → sub-agentes haiku), partiendo del tag de la fase
# anterior de la MISMA celda (celda/c2-orch-claude-sonnet/f<desde-1>). Pensado para
# retomar una celda cortada por infraestructura (p. ej. límite de sesión) sin
# repetir las fases ya verdes.
#
# Uso: scripts/run-fases.sh <desde 1-5> <hasta 1-5> [sonnet|opus|haiku]
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DESDE="${1:?fase inicial (1-5)}"
HASTA="${2:?fase final (1-5)}"
MODEL_KEY="${3:-sonnet}"
case "${MODEL_KEY}" in
  sonnet) MODEL_ID="claude-sonnet-5" ;;
  opus)   MODEL_ID="claude-opus-4-8" ;;
  haiku)  MODEL_ID="claude-haiku-4-5-20251001" ;;
  *) echo "modelo inválido" >&2; exit 1 ;;
esac
COND="c2-orch-claude"
PROJECT="aitl-raytracer-orq-sonnet"
CELL="${COND}@${MODEL_KEY}"
CELL_TAG="${COND}-${MODEL_KEY}"
PREV=$(( DESDE - 1 ))
BASE_TAG="celda/${CELL_TAG}/f${PREV}"
COURSE_MIN="${COURSE_MIN:-60}"
STAMP="$(date +%Y%m%d-%H%M%S)"
LOG_DIR="${ROOT}/data/logs"; mkdir -p "${LOG_DIR}"

die() { echo "✗ $*" >&2; exit 1; }
command -v aitl >/dev/null || die "no encuentro aitl"
command -v claude >/dev/null || die "no encuentro claude"
git -C "${ROOT}" rev-parse -q --verify "${BASE_TAG}" >/dev/null || die "no existe el tag base ${BASE_TAG}"
pgrep -f "run-course.sh|run-tema7.sh" >/dev/null && die "otra corrida usa start/ — espera a que termine"

cd "${ROOT}"
git checkout -q curso
git show "${BASE_TAG}:start/rt.cpp" > start/rt.cpp
cp "start/conditions/${COND}/CLAUDE.md" start/CLAUDE.md
mkdir -p start/.claude
cp "start/conditions/${COND}/.claude/settings.json" start/.claude/settings.json
sed -i "s/\"model\": *\"[^\"]*\"/\"model\": \"${MODEL_ID}\"/" start/.claude/settings.json
rm -f start/*.ppm start/*.png
cp aitl-raytracer/.mcp.json start/.mcp.json
echo "→ base = ${BASE_TAG}; fases ${DESDE}..${HASTA}; orquestador ${MODEL_ID}, subs haiku (tope ${COURSE_MIN} min)"

T0=$(date +%s)
DEADLINE=$(( T0 + COURSE_MIN * 60 ))
STOP_REASON="fases ${DESDE}-${HASTA} completas"

for FASE in $(seq "${DESDE}" "${HASTA}"); do
  NOW=$(date +%s); LEFT=$(( DEADLINE - NOW ))
  [ "${LEFT}" -le 120 ] && { STOP_REASON="tope alcanzado antes de la fase ${FASE}"; echo "⚠ ${STOP_REASON}"; break; }
  SPEC="${ROOT}/start/sdd/phase-0${FASE}/spec.md"
  PROMPT="$(awk '/^## Prompt de tarea/{f=1;next} /^## /{f=0} f' "${SPEC}")"
  LOG="${LOG_DIR}/curso-${CELL_TAG}-fase${FASE}-${STAMP}.log"
  echo "→ fase ${FASE} (quedan $((LEFT / 60)) min) — log: ${LOG}"
  rm -f start/*.ppm start/*.png
  GATE_CMD="make"
  [ -f "${ROOT}/start/sdd/phase-0${FASE}/check.sh" ] && GATE_CMD="make && bash sdd/phase-0${FASE}/check.sh"
  printf '%s\n' "${PROMPT}" > start/FASE-PROMPT.txt
  RUN_PROMPT="Eres el ORQUESTADOR de esta fase. NO implementes tú el código de la fase (nada de Edit/Write sobre rt.cpp): tu trabajo es delegar, verificar y documentar.
La tarea completa de la fase está en el archivo FASE-PROMPT.txt de tu directorio actual.
Sigue EXACTAMENTE estos pasos:
1. Consulta list_decisions y search_memory (project \"${PROJECT}\") por aprendizajes de las fases previas antes de delegar.
2. DELEGA la implementación al sub-agente haiku con tu tool Bash (parámetro timeout: 2400000 — el límite de Bash está elevado en settings; SIEMPRE en primer plano):
env AITL_HOST_ARGS_CLAUDE_CODE='--model claude-haiku-4-5-20251001' aitl run-host \"\$(cat FASE-PROMPT.txt)\" --project ${PROJECT} --host claude-code --cwd .
3. VERIFICA tú mismo con Bash (timeout: 600000): ${GATE_CMD}
4. Si el gate falla, DELEGA una corrección (mismo prefijo env, timeout 2400000, primer plano) citando el error de forma PRECISA: criterio que falló, valor medido, umbral permitido, modo/render afectado y tu hipótesis de la causa en una línea.
5. Cuando el gate quede verde, registra el aprendizaje con la tool MCP write_memory (project \"${PROJECT}\"): slug corto y contenido de 3-6 líneas con el bug encontrado, la causa raíz y la corrección aplicada.
Máximo 3 delegaciones en total. PROHIBIDO run_in_background: en modo headless tu sesión TERMINA al quedarte esperando notificaciones y la fase muere con trabajo a medias — toda delegación va en primer plano con timeout amplio. Tu éxito se mide únicamente con el gate en verde."
  FT0=$(date +%s)
  set +e
  timeout --foreground "${LEFT}" \
    aitl run-host "${RUN_PROMPT}" --project "${PROJECT}" --host claude-code \
    --cwd "${ROOT}/start" </dev/null 2>&1 | tee "${LOG}"
  RC=${PIPESTATUS[0]}
  set -e
  rm -f start/FASE-PROMPT.txt
  DUR_S=$(( $(date +%s) - FT0 ))
  RUN_ID="$(grep -oiE '[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}' "${LOG}" | tail -1 || true)"
  [ "${RC}" = "124" ] && RUN_STATUS="timeout" || RUN_STATUS="done"

  GATE="fail"
  set +e
  ( cd start && make && { [ ! -f "sdd/phase-0${FASE}/check.sh" ] || bash "sdd/phase-0${FASE}/check.sh"; } ) \
    </dev/null >"${LOG%.log}.gate.log" 2>&1 && GATE="ok"
  set -e
  echo "→ fase ${FASE}: run=${RUN_STATUS} gate=${GATE} (${DUR_S}s, run_id=${RUN_ID:-—})"
  bash scripts/collect-metrics.sh "${RUN_ID:--}" "${FASE}" "${CELL}" "${GATE}" "${DUR_S}" "${RUN_STATUS}" || true

  EV_PFX="data/runs/fase${FASE}-${CELL}-${RUN_ID:-sin-id}"
  cp start/rt.cpp "${EV_PFX}.rt.cpp" 2>/dev/null || true
  for F in start/*.ppm; do
    [ -f "${F}" ] || continue
    B="$(basename "${F%.ppm}")"
    python3 -c "from PIL import Image; import sys; Image.open(sys.argv[1]).save(sys.argv[2])" \
      "${F}" "${EV_PFX}.${B}.png" 2>/dev/null || true
  done
  git add start/rt.cpp start/CLAUDE.md start/.claude data/metricas.csv RESUMEN-METRICAS.md 2>/dev/null || true
  git add data/runs/*.png 2>/dev/null || true
  git add data/runs/*.rt.cpp 2>/dev/null || true
  git add data/runs/*.runshow.txt 2>/dev/null || true
  git commit -q -m "curso ${CELL}: fase ${FASE} run=${RUN_STATUS} gate=${GATE} (${DUR_S}s, run ${RUN_ID:-sin-id}) [re-corrida]" || true
  git tag -f "celda/${CELL_TAG}/f${FASE}" >/dev/null 2>&1 || true

  [ "${RUN_STATUS}" = "timeout" ] && { STOP_REASON="tope durante la fase ${FASE}"; echo "⚠ ${STOP_REASON}"; break; }
  [ "${GATE}" != "ok" ] && { STOP_REASON="gate de la fase ${FASE} en rojo"; echo "⚠ ${STOP_REASON}"; break; }
done

AITL_BIN="$(readlink -f "$(command -v aitl)")"
AITL_HOME="$(dirname "$(dirname "$(dirname "${AITL_BIN}")")")"
node scripts/collect-sessions.mjs "${AITL_HOME}" "${PROJECT}" "${CELL}" || true
git add data/metricas.csv 2>/dev/null || true
git commit -q -m "curso ${CELL}: sesiones de la re-corrida al CSV (${STOP_REASON})" || true
echo "→ re-corrida terminada: ${STOP_REASON}"
