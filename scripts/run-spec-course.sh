#!/usr/bin/env bash
# Celda EXPLORATORIA c2-sonnet-spec: "planifica el grande, ejecuta el barato".
# Por cada fase 0→5, secuencial:
#   1. SONNET (arquitecto) lee la spec + el código ACTUAL y escribe PLAN-SONNET.md
#      (plan anclado al código, fórmulas exactas, trampas conocidas). NO implementa.
#   2. HAIKU (implementador) corre one-shot estilo c2-memory (hidratación + MCP)
#      con el prompt de la fase + la instrucción de seguir el plan del arquitecto.
# Project MCP propio: aitl-raytracer-spec-sonnet. Cell: c2-sonnet-spec@haiku
# (el planner registra como c2-sonnet-spec@haiku-plan).
#
# Uso: scripts/run-spec-course.sh          (COURSE_MIN=90 default: 2 pasadas/fase)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
COND="c2-sonnet-spec"
PROJECT="aitl-raytracer-spec-sonnet"
CELL="${COND}@haiku"
CELL_TAG="${COND}-haiku"
IMPL_MODEL="claude-haiku-4-5-20251001"
PLAN_MODEL="claude-sonnet-5"
COURSE_MIN="${COURSE_MIN:-90}"
STAMP="$(date +%Y%m%d-%H%M%S)"
LOG_DIR="${ROOT}/data/logs"; mkdir -p "${LOG_DIR}"

die() { echo "✗ $*" >&2; exit 1; }
command -v aitl >/dev/null || die "no encuentro aitl"
command -v claude >/dev/null || die "no encuentro claude"
pgrep -f "run-course.sh|run-tema7.sh|run-fases.sh|run-fase07.sh" >/dev/null && die "otra corrida usa start/"
for F in 0 1 2 3 4 5; do
  grep -q "status: approved" "${ROOT}/start/sdd/phase-0${F}/spec.md" || die "spec fase ${F} no aprobada"
done

cd "${ROOT}"
git checkout -q curso
cp start/sdd/base/rt.cpp start/rt.cpp
cp start/sdd/base/Makefile start/Makefile
cp "start/conditions/${COND}/CLAUDE.md" start/CLAUDE.md
mkdir -p start/.claude
cp "start/conditions/${COND}/.claude/settings.json" start/.claude/settings.json
sed -i "s/\"model\": *\"[^\"]*\"/\"model\": \"${IMPL_MODEL}\"/" start/.claude/settings.json
rm -f start/*.ppm start/*.png start/PLAN-SONNET.md
cp aitl-raytracer/.mcp.json start/.mcp.json
echo "→ base reanclada; celda ${CELL}: ${PLAN_MODEL} planifica → ${IMPL_MODEL} implementa (tope ${COURSE_MIN} min)"

T0=$(date +%s)
DEADLINE=$(( T0 + COURSE_MIN * 60 ))
STOP_REASON="curso completo (fases 0-5)"

for FASE in 0 1 2 3 4 5; do
  NOW=$(date +%s); LEFT=$(( DEADLINE - NOW ))
  [ "${LEFT}" -le 180 ] && { STOP_REASON="tope antes de la fase ${FASE}"; echo "⚠ ${STOP_REASON}"; break; }
  SPEC="${ROOT}/start/sdd/phase-0${FASE}/spec.md"
  PROMPT="$(awk '/^## Prompt de tarea/{f=1;next} /^## /{f=0} f' "${SPEC}")"
  LOG_P="${LOG_DIR}/curso-${CELL_TAG}-fase${FASE}-plan-${STAMP}.log"
  LOG_I="${LOG_DIR}/curso-${CELL_TAG}-fase${FASE}-${STAMP}.log"
  GATE_CMD="make"
  [ -f "${ROOT}/start/sdd/phase-0${FASE}/check.sh" ] && GATE_CMD="make && bash sdd/phase-0${FASE}/check.sh"
  printf '%s\n' "${PROMPT}" > start/FASE-PROMPT.txt
  rm -f start/*.ppm start/*.png start/PLAN-SONNET.md

  # ── Paso 1: SONNET escribe el plan (no implementa) ──────────────────────────
  PLAN_PROMPT="Eres el ARQUITECTO SENIOR de esta fase. NO modifiques rt.cpp ni ningún archivo de código: tu único entregable es el archivo PLAN-SONNET.md.
La tarea de la fase está en FASE-PROMPT.txt. Lee también el rt.cpp ACTUAL (el punto de partida real del implementador).
Escribe PLAN-SONNET.md en español con:
1. Plan de implementación paso a paso ANCLADO al código actual (qué función/struct tocar y dónde).
2. Las fórmulas exactas listas para transcribir (pdf, cambios de base, transmitancias — sin ambigüedad).
3. TRAMPAS CONOCIDAS de esta tarea (p. ej.: en muestreo de área la visibilidad se comprueba contra LA ESFERA de luz, no contra el punto muestreado exacto; la dirección del rayo de sombra va HACIA la muestra; normalizaciones 1/π y 1/(4πr²); clamp después de promediar; erand48 por píxel).
4. Estrategia de verificación barata: qué renders de pocos spp correr y qué medias esperar antes de gastar en los definitivos.
5. Criterio de terminado: ${GATE_CMD} en verde.
Sé concreto y breve (60-120 líneas). SOLO crea PLAN-SONNET.md y termina."
  echo "→ fase ${FASE} · plan (${PLAN_MODEL}) — log: ${LOG_P}"
  set +e
  env AITL_HOST_ARGS_CLAUDE_CODE="--model ${PLAN_MODEL}" timeout --foreground 900 \
    aitl run-host "${PLAN_PROMPT}" --project "${PROJECT}" --host claude-code \
    --cwd "${ROOT}/start" </dev/null 2>&1 | tee "${LOG_P}" >/dev/null
  set -e
  PLAN_RUN_ID="$(grep -oiE '[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}' "${LOG_P}" | tail -1 || true)"
  if [ ! -f start/PLAN-SONNET.md ]; then
    echo "⚠ sonnet no dejó PLAN-SONNET.md — la fase corre sin plan (se anota)"
  fi
  bash scripts/collect-metrics.sh "${PLAN_RUN_ID:--}" "${FASE}" "${CELL}-plan" "" "" "plan" || true

  # ── Paso 2: HAIKU implementa one-shot (estilo c2-memory) ─────────────────────
  NOW=$(date +%s); LEFT=$(( DEADLINE - NOW ))
  [ "${LEFT}" -le 120 ] && { STOP_REASON="tope tras el plan de la fase ${FASE}"; echo "⚠ ${STOP_REASON}"; break; }
  IMPL_PROMPT="${PROMPT}

── PLAN DEL ARQUITECTO ──
Antes de escribir código lee PLAN-SONNET.md (plan de un arquitecto senior para esta
tarea exacta sobre este código): síguelo salvo que contradiga la spec. Al terminar,
${GATE_CMD} debe quedar verde."
  echo "→ fase ${FASE} · implementación (haiku) — log: ${LOG_I}"
  FT0=$(date +%s)
  set +e
  timeout --foreground "${LEFT}" \
    aitl run-host "${IMPL_PROMPT}" --project "${PROJECT}" --host claude-code \
    --cwd "${ROOT}/start" </dev/null 2>&1 | tee "${LOG_I}"
  RC=${PIPESTATUS[0]}
  set -e
  DUR_S=$(( $(date +%s) - FT0 ))
  RUN_ID="$(grep -oiE '[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}' "${LOG_I}" | tail -1 || true)"
  [ "${RC}" = "124" ] && RUN_STATUS="timeout" || RUN_STATUS="done"

  GATE="fail"
  set +e
  ( cd start && make && { [ ! -f "sdd/phase-0${FASE}/check.sh" ] || bash "sdd/phase-0${FASE}/check.sh"; } ) \
    </dev/null >"${LOG_I%.log}.gate.log" 2>&1 && GATE="ok"
  set -e
  echo "→ fase ${FASE}: run=${RUN_STATUS} gate=${GATE} (${DUR_S}s, run_id=${RUN_ID:-—})"
  bash scripts/collect-metrics.sh "${RUN_ID:--}" "${FASE}" "${CELL}" "${GATE}" "${DUR_S}" "${RUN_STATUS}" || true

  EV_PFX="data/runs/fase${FASE}-${CELL}-${RUN_ID:-sin-id}"
  cp start/rt.cpp "${EV_PFX}.rt.cpp" 2>/dev/null || true
  cp start/PLAN-SONNET.md "${EV_PFX}.plan-sonnet.md" 2>/dev/null || true
  for F in start/*.ppm; do
    [ -f "${F}" ] || continue
    B="$(basename "${F%.ppm}")"
    python3 -c "from PIL import Image; import sys; Image.open(sys.argv[1]).save(sys.argv[2])" \
      "${F}" "${EV_PFX}.${B}.png" 2>/dev/null || true
  done
  git add start/rt.cpp start/CLAUDE.md start/.claude start/conditions/${COND} data/metricas.csv RESUMEN-METRICAS.md 2>/dev/null || true
  git add data/runs/*.png 2>/dev/null || true
  git add data/runs/*.rt.cpp 2>/dev/null || true
  git add data/runs/*.plan-sonnet.md 2>/dev/null || true
  git add data/runs/*.runshow.txt 2>/dev/null || true
  git commit -q -m "curso ${CELL}: fase ${FASE} run=${RUN_STATUS} gate=${GATE} (${DUR_S}s, run ${RUN_ID:-sin-id})" || true
  git tag -f "celda/${CELL_TAG}/f${FASE}" >/dev/null 2>&1 || true

  [ "${RUN_STATUS}" = "timeout" ] && { STOP_REASON="tope durante la fase ${FASE}"; echo "⚠ ${STOP_REASON}"; break; }
  [ "${GATE}" != "ok" ] && { STOP_REASON="gate de la fase ${FASE} en rojo"; echo "⚠ ${STOP_REASON}"; break; }
done

rm -f start/FASE-PROMPT.txt
TOTAL_S=$(( $(date +%s) - T0 ))
echo "→ celda ${CELL} terminada: ${STOP_REASON}. Total $((TOTAL_S/60))m $((TOTAL_S%60))s."
