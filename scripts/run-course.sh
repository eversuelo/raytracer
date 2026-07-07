#!/usr/bin/env bash
# Corre UNA celda del CURSO COMPLETO (condición × modelo): fases 00→05 de corrido
# con tope global de tiempo, gate objetivo por fase, métricas por fase y un resumen
# final en markdown escrito por el propio Claude de la celda.
#
# Uso:
#   scripts/run-course.sh <c0-bare|c2-memory> <sonnet|opus>
#
# Variables de entorno:
#   COURSE_MIN=60    tope global de la celda en minutos (default 60)
#
# ⚠ Corre UNA celda a la vez: todas comparten el working tree de start/.
# ⚠ La celda vive en la rama curso/<condición>-<modelo>, creada desde main.
#   Para relanzar una celda desde cero: git branch -D curso/<condición>-<modelo>
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
COND="${1:?c0-bare | c2-memory}"
MODEL_KEY="${2:?sonnet | opus}"
COURSE_MIN="${COURSE_MIN:-60}"

die() { echo "✗ $*" >&2; exit 1; }

case "${COND}" in c0-bare|c2-memory) ;; *) die "condición inválida: ${COND} (c0-bare|c2-memory)";; esac
case "${MODEL_KEY}" in
  sonnet) MODEL_ID="claude-sonnet-5" ;;
  opus)   MODEL_ID="claude-opus-4-8" ;;
  *) die "modelo inválido: ${MODEL_KEY} (sonnet|opus)" ;;
esac

CELL="${COND}@${MODEL_KEY}"
BRANCH="curso/${COND}-${MODEL_KEY}"
STAMP="$(date +%Y%m%d-%H%M%S)"
LOG_DIR="${ROOT}/data/logs"; mkdir -p "${LOG_DIR}"
CURSO_DIR="${ROOT}/data/curso"; mkdir -p "${CURSO_DIR}"
FASES_CSV="${CURSO_DIR}/fases-${COND}-${MODEL_KEY}.csv"

# ── Preflight ────────────────────────────────────────────────────────────────
command -v aitl >/dev/null   || die "no encuentro 'aitl' en PATH"
command -v claude >/dev/null || die "no encuentro 'claude' (Claude Code) en PATH"
if [ "${COND}" = "c2-memory" ] && [ ! -f "${ROOT}/aitl-raytracer/.mcp.json" ]; then
  die "c2-memory necesita aitl-raytracer/.mcp.json (credenciales del MCP)"
fi
for F in 0 1 2 3 4 5; do
  S="${ROOT}/start/sdd/phase-0${F}/spec.md"
  [ -f "${S}" ] || die "no existe la spec ${S}"
  grep -q "status: approved" "${S}" || die "spec de fase ${F} NO aprobada — el curso exige las 6 specs approved"
  awk '/^## Prompt de tarea/{f=1;next} /^## /{f=0} f' "${S}" | grep -q . \
    || die "la spec de fase ${F} no tiene sección '## Prompt de tarea'"
done

# ── Rama de la celda (SIEMPRE desde main: mismo punto de partida para las 4) ──
cd "${ROOT}"
if git rev-parse --verify -q "${BRANCH}" >/dev/null; then
  git checkout -q "${BRANCH}"
  echo "⚠ la rama ${BRANCH} ya existía — se continúa sobre su estado actual."
else
  git checkout -q -b "${BRANCH}" main
fi
echo "→ rama ${BRANCH} (celda ${CELL}, tope ${COURSE_MIN} min)"

# ── Sembrar el proyecto base + plantilla de la condición + modelo ─────────────
[ -f start/rt.cpp ]   || cp start/sdd/base/rt.cpp start/rt.cpp
[ -f start/Makefile ] || cp start/sdd/base/Makefile start/Makefile
mkdir -p start/out
cp "start/conditions/${COND}/CLAUDE.md" start/CLAUDE.md
mkdir -p start/.claude
cp "start/conditions/${COND}/.claude/settings.json" start/.claude/settings.json
sed -i "s/\"model\": *\"[^\"]*\"/\"model\": \"${MODEL_ID}\"/" start/.claude/settings.json
grep -q "\"${MODEL_ID}\"" start/.claude/settings.json || die "no pude fijar el modelo en start/.claude/settings.json"
if [ "${COND}" = "c2-memory" ]; then
  cp aitl-raytracer/.mcp.json start/.mcp.json   # gitignored: lleva credenciales
fi
echo "→ plantilla ${COND} instalada; modelo del agente: ${MODEL_ID}"

# ── Curso: fases 00→05 con deadline global ────────────────────────────────────
T0=$(date +%s)
DEADLINE=$(( T0 + COURSE_MIN * 60 ))
[ -f "${FASES_CSV}" ] || echo "fase,run_id,status,gate,dur_s" > "${FASES_CSV}"
STOP_REASON="curso completo (fases 0–5)"

for FASE in 0 1 2 3 4 5; do
  NOW=$(date +%s); LEFT=$(( DEADLINE - NOW ))
  if [ "${LEFT}" -le 120 ]; then
    STOP_REASON="tope de ${COURSE_MIN} min alcanzado antes de la fase ${FASE}"
    echo "⚠ ${STOP_REASON}"
    break
  fi

  SPEC="${ROOT}/start/sdd/phase-0${FASE}/spec.md"
  PROMPT="$(awk '/^## Prompt de tarea/{f=1;next} /^## /{f=0} f' "${SPEC}")"
  LOG="${LOG_DIR}/curso-${COND}-${MODEL_KEY}-fase${FASE}-${STAMP}.log"
  echo "→ fase ${FASE} (quedan $((LEFT / 60)) min) — log: ${LOG}"

  FT0=$(date +%s)
  set +e
  timeout --foreground "${LEFT}" \
    aitl run-host "${PROMPT}" --project aitl-raytracer --host claude-code \
    --cwd "${ROOT}/start" </dev/null 2>&1 | tee "${LOG}"
  RC=${PIPESTATUS[0]}
  set -e
  DUR_S=$(( $(date +%s) - FT0 ))
  RUN_ID="$(grep -oiE '[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}' "${LOG}" | tail -1 || true)"
  if [ "${RC}" = "124" ]; then RUN_STATUS="timeout"; else RUN_STATUS="done"; fi

  # Gate objetivo, independiente de lo que reporte el agente (stdin cerrado)
  GATE="fail"
  set +e
  ( cd "${ROOT}/start" && make && { [ ! -f "sdd/phase-0${FASE}/check.sh" ] || bash "sdd/phase-0${FASE}/check.sh"; } ) \
    </dev/null >"${LOG%.log}.gate.log" 2>&1 && GATE="ok"
  set -e
  echo "→ fase ${FASE}: run=${RUN_STATUS} gate=${GATE} (${DUR_S}s, run_id=${RUN_ID:-—})"

  echo "${FASE},${RUN_ID},${RUN_STATUS},${GATE},${DUR_S}" >> "${FASES_CSV}"
  if [ -n "${RUN_ID}" ]; then
    bash "${ROOT}/scripts/collect-metrics.sh" "${RUN_ID}" "${FASE}" "${CELL}" host || true
  fi

  # Evidencia de la fase junto al .runshow.txt: código + renders (gitignored/sobrescribibles
  # entre fases, así que se pierden si no se copian aquí)
  EV_PFX="${ROOT}/data/runs/fase${FASE}-${CELL}-${RUN_ID:-sin-id}"
  cp start/rt.cpp "${EV_PFX}.rt.cpp" 2>/dev/null || true
  for F in start/*.ppm; do
    [ -f "${F}" ] && cp "${F}" "${EV_PFX}.$(basename "${F}")"
  done

  # Commit del avance de la fase en la rama de la celda (renders quedan gitignored)
  git add start/rt.cpp start/Makefile start/CLAUDE.md start/.claude
  git commit -q -m "curso ${CELL}: fase ${FASE} run=${RUN_STATUS} gate=${GATE} (${DUR_S}s, run ${RUN_ID:-sin-id})" || true

  if [ "${RUN_STATUS}" = "timeout" ]; then
    STOP_REASON="tope de ${COURSE_MIN} min alcanzado durante la fase ${FASE}"
    echo "⚠ ${STOP_REASON}"
    break
  fi
  if [ "${GATE}" != "ok" ]; then
    STOP_REASON="gate de la fase ${FASE} en rojo — el curso es secuencial, no se avanza sobre base rota"
    echo "⚠ ${STOP_REASON}"
    break
  fi
done

TOTAL_S=$(( $(date +%s) - T0 ))
echo "→ curso terminado: ${STOP_REASON}. Tiempo total: $((TOTAL_S / 60)) min $((TOTAL_S % 60)) s."

# ── Resumen final escrito por el propio Claude de la celda ────────────────────
RESUMEN_DST="${CURSO_DIR}/resumen-${COND}-${MODEL_KEY}.md"
RESUMEN_PROMPT="Acabas de terminar la celda '${CELL}' del experimento: completar el curso del
raytracer (fases 00→05, una por proyecto) con tope de ${COURSE_MIN} minutos.
Resultado global: ${STOP_REASON}. Tiempo total usado: ${TOTAL_S} s.
Datos por fase (fase,run_id,status,gate,dur_s):
$(cat "${FASES_CSV}")

Escribe el archivo RESUMEN-CURSO.md en la raíz del proyecto (tu cwd) con, en español:
1. Tabla por fase: fase, proyecto, run_id, gate, duración.
2. Hasta qué proyecto llegaste y por qué se detuvo el curso.
3. Por cada fase completada: qué implementaste (2-3 líneas) y cómo lo verificaste.
4. Incidencias/decisiones técnicas relevantes.
5. Autoevaluación breve: qué harías distinto con más tiempo.
Usa 'git log --oneline' y los archivos del proyecto para reconstruir el detalle.
No modifiques rt.cpp ni ningún otro archivo: SOLO crea RESUMEN-CURSO.md y termina."
echo "→ generando resumen de la celda (lo escribe el propio Claude)…"
set +e
timeout --foreground 600 \
  aitl run-host "${RESUMEN_PROMPT}" --project aitl-raytracer --host claude-code \
  --cwd "${ROOT}/start" </dev/null 2>&1 | tee "${LOG_DIR}/curso-${COND}-${MODEL_KEY}-resumen-${STAMP}.log"
set -e
if [ -f "${ROOT}/start/RESUMEN-CURSO.md" ]; then
  mv "${ROOT}/start/RESUMEN-CURSO.md" "${RESUMEN_DST}"
  echo "→ resumen de la celda: ${RESUMEN_DST}"
else
  echo "⚠ el agente no dejó RESUMEN-CURSO.md — revisa el log del resumen."
fi

echo ""
echo "→ celda ${CELL} terminada."
echo "   fases:    ${FASES_CSV}"
echo "   resumen:  ${RESUMEN_DST}"
echo "   métricas: data/metricas.csv (cond=${CELL}) + data/runs/*.runshow.txt"
echo "   código:   rama ${BRANCH} (un commit por fase)"
