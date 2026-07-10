#!/usr/bin/env bash
# Corre SOLO la fase 07 (Tema 7: medios participantes — estado del arte) con la
# condición c2-orch-claude: orquestador Claude Code (modelo del 2º arg, default
# sonnet) delegando en sub-agentes claude-code HAIKU, project aitl-raytracer-orq-sonnet.
#
# A diferencia del curso, la base NO es sdd/base: la fase 7 parte del resultado
# ACEPTADO de la fase 2 de la misma celda (tag celda/c2-orch-claude-sonnet/f2).
#
# Uso: scripts/run-tema7.sh [sonnet|opus|haiku]   (default sonnet)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MODEL_KEY="${1:-sonnet}"
case "${MODEL_KEY}" in
  sonnet) MODEL_ID="claude-sonnet-5" ;;
  opus)   MODEL_ID="claude-opus-4-8" ;;
  haiku)  MODEL_ID="claude-haiku-4-5-20251001" ;;
  *) echo "modelo inválido: ${MODEL_KEY}" >&2; exit 1 ;;
esac
COND="c2-orch-claude"
PROJECT="aitl-raytracer-orq-sonnet"
CELL="${COND}@${MODEL_KEY}"
CELL_TAG="${COND}-${MODEL_KEY}"
BASE_TAG="celda/${CELL_TAG}/f5"
FASE=7
STAMP="$(date +%Y%m%d-%H%M%S)"
LOG_DIR="${ROOT}/data/logs"; mkdir -p "${LOG_DIR}"
LOG="${LOG_DIR}/curso-${CELL_TAG}-fase${FASE}-${STAMP}.log"
TIMEOUT_S="${TEMA7_TIMEOUT:-2400}"   # 40 min default para la fase completa

die() { echo "✗ $*" >&2; exit 1; }
command -v aitl >/dev/null || die "no encuentro aitl"
command -v claude >/dev/null || die "no encuentro claude"
git -C "${ROOT}" rev-parse -q --verify "${BASE_TAG}" >/dev/null || die "no existe el tag base ${BASE_TAG}"
S="${ROOT}/start/sdd/phase-07/spec.md"
grep -q "status: approved" "${S}" || die "spec de fase 7 no aprobada"

cd "${ROOT}"
git checkout -q curso

# Reanclar al resultado de la fase 2 de la celda + plantilla de condición
git show "${BASE_TAG}:start/rt.cpp" > start/rt.cpp
cp start/sdd/base/Makefile start/Makefile 2>/dev/null || true
cp "start/conditions/${COND}/CLAUDE.md" start/CLAUDE.md
mkdir -p start/.claude
cp "start/conditions/${COND}/.claude/settings.json" start/.claude/settings.json
sed -i "s/\"model\": *\"[^\"]*\"/\"model\": \"${MODEL_ID}\"/" start/.claude/settings.json
rm -f start/*.ppm start/*.png
cp aitl-raytracer/.mcp.json start/.mcp.json
echo "→ base = ${BASE_TAG} + plantilla ${COND}; orquestador ${MODEL_ID}, subs haiku"

PROMPT="$(awk '/^## Prompt de tarea/{f=1;next} /^## /{f=0} f' "${S}")"
printf '%s\n' "${PROMPT}" > start/FASE-PROMPT.txt
GATE_CMD="make && bash sdd/phase-07/check.sh"
RUN_PROMPT="Eres el ORQUESTADOR de esta fase. NO implementes tú el código de la fase (nada de Edit/Write sobre rt.cpp): tu trabajo es delegar, verificar y documentar.
La tarea completa de la fase está en el archivo FASE-PROMPT.txt de tu directorio actual. El material teórico son los Temas 6 y 7: path tracing con MIS (fase 5) + medio participante homogéneo con in-scattering desde fuentes de ÁREA (muestreo de esfera por punto de marcha) y la puntual.
Sigue EXACTAMENTE estos pasos:
1. Consulta list_decisions y search_memory (project \"${PROJECT}\") por aprendizajes de las fases previas antes de delegar.
2. DELEGA la implementación al sub-agente haiku con tu tool Bash (parámetro timeout: 600000, en primer plano):
env AITL_HOST_ARGS_CLAUDE_CODE='--model claude-haiku-4-5-20251001' aitl run-host \"\$(cat FASE-PROMPT.txt)\" --project ${PROJECT} --host claude-code --cwd .
3. VERIFICA tú mismo con Bash (timeout: 600000): ${GATE_CMD}
4. Si el gate falla, DELEGA una corrección (mismo prefijo env, timeout 600000) citando el error de forma PRECISA: criterio que falló, valor medido, valor esperado y tu hipótesis de la causa en una línea.
5. Cuando el gate quede verde, registra el aprendizaje con la tool MCP write_memory (project \"${PROJECT}\"): slug corto y contenido de 3-6 líneas con qué se implementó del path tracing volumétrico (Temas 6+7), el bug más difícil y su solución.
Máximo 3 delegaciones en total. No uses run_in_background. Tu éxito se mide únicamente con el gate en verde."

T0=$(date +%s)
set +e
timeout --foreground "${TIMEOUT_S}" \
  aitl run-host "${RUN_PROMPT}" --project "${PROJECT}" --host claude-code \
  --cwd "${ROOT}/start" </dev/null 2>&1 | tee "${LOG}"
RC=${PIPESTATUS[0]}
set -e
rm -f start/FASE-PROMPT.txt
DUR_S=$(( $(date +%s) - T0 ))
RUN_ID="$(grep -oiE '[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}' "${LOG}" | tail -1 || true)"
[ "${RC}" = "124" ] && RUN_STATUS="timeout" || RUN_STATUS="done"

GATE="fail"
set +e
( cd start && make && bash sdd/phase-07/check.sh ) </dev/null >"${LOG%.log}.gate.log" 2>&1 && GATE="ok"
set -e
echo "→ fase 7: run=${RUN_STATUS} gate=${GATE} (${DUR_S}s, run_id=${RUN_ID:-—})"

bash scripts/collect-metrics.sh "${RUN_ID:--}" "${FASE}" "${CELL}" "${GATE}" "${DUR_S}" "${RUN_STATUS}" || true
AITL_BIN="$(readlink -f "$(command -v aitl)")"
AITL_HOME="$(dirname "$(dirname "$(dirname "${AITL_BIN}")")")"
node scripts/collect-sessions.mjs "${AITL_HOME}" "${PROJECT}" "${CELL}" || true

EV_PFX="data/runs/fase${FASE}-${CELL}-${RUN_ID:-sin-id}"
cp start/rt.cpp "${EV_PFX}.rt.cpp" 2>/dev/null || true
for F in start/*.ppm; do
  [ -f "${F}" ] || continue
  B="$(basename "${F%.ppm}")"
  python3 -c "from PIL import Image; import sys; Image.open(sys.argv[1]).save(sys.argv[2])" \
    "${F}" "${EV_PFX}.${B}.png" 2>/dev/null || true
done

git add start/rt.cpp start/CLAUDE.md start/.claude start/sdd/phase-07 data/metricas.csv RESUMEN-METRICAS.md 2>/dev/null || true
git add data/runs/*.png 2>/dev/null || true
git add data/runs/*.rt.cpp 2>/dev/null || true
git add data/runs/*.runshow.txt 2>/dev/null || true
git commit -q -m "tema7 ${CELL}: fase 7 run=${RUN_STATUS} gate=${GATE} (${DUR_S}s, run ${RUN_ID:-sin-id})" || true
git tag -f "celda/${CELL_TAG}/f7" >/dev/null 2>&1 || true
echo "→ fase 07 (fogpt) terminada: gate=${GATE} · evidencia en data/runs/fase6-* · tag celda/${CELL_TAG}/f7"
