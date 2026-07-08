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
# ⚠ Modelo de rama ÚNICA (una sola rama `curso`): cada celda REANCLA start/ a la base
#   (mismo punto de partida para todas) y deja un commit por fase, TAGUEADO como
#   `celda/<condición>-<modelo>/f<N>`. La celda ya no es una rama: es su conjunto de tags.
#   Listar una celda:   git tag -l 'celda/c2-memory-sonnet/*'
#   Relanzar una celda: se re-taguea con -f al volver a correrla (los tags viejos se pisan).
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
CELL_TAG="${COND}-${MODEL_KEY}"          # apto para nombres de tag/ref (sin '@')
BRANCH="curso"                            # rama ÚNICA para todas las celdas
STAMP="$(date +%Y%m%d-%H%M%S)"
LOG_DIR="${ROOT}/data/logs"; mkdir -p "${LOG_DIR}"
CURSO_DIR="${ROOT}/data/curso"; mkdir -p "${CURSO_DIR}"

# ── Preflight ────────────────────────────────────────────────────────────────
command -v aitl >/dev/null   || die "no encuentro 'aitl' en PATH"
command -v claude >/dev/null || die "no encuentro 'claude' (Claude Code) en PATH"
command -v python3 >/dev/null || die "no encuentro 'python3' (para PPM→PNG y métricas)"
python3 -c "import PIL" 2>/dev/null || die "falta Pillow (PPM→PNG): pip install pillow"
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

# ── Rama única `curso` (creada desde main la primera vez) ─────────────────────
cd "${ROOT}"
if git rev-parse --verify -q "${BRANCH}" >/dev/null; then
  git checkout -q "${BRANCH}"
else
  git checkout -q -b "${BRANCH}" main
fi
echo "→ rama ${BRANCH} · celda ${CELL} (tope ${COURSE_MIN} min)"

# ── REANCLAR a la base: mismo punto de partida para TODA celda ────────────────
# Force-seed (pisa lo que dejó la celda anterior) + limpia renders viejos (evita el
# gotcha del .ppm rancio: un render de otra fase/celda con el mismo nombre pasaría por
# salida actual). Así la celda N nunca hereda el código de la N-1.
cp start/sdd/base/rt.cpp   start/rt.cpp
cp start/sdd/base/Makefile start/Makefile
mkdir -p start/out
cp "start/conditions/${COND}/CLAUDE.md" start/CLAUDE.md
mkdir -p start/.claude
cp "start/conditions/${COND}/.claude/settings.json" start/.claude/settings.json
sed -i "s/\"model\": *\"[^\"]*\"/\"model\": \"${MODEL_ID}\"/" start/.claude/settings.json
grep -q "\"${MODEL_ID}\"" start/.claude/settings.json || die "no pude fijar el modelo en start/.claude/settings.json"
rm -f start/*.ppm start/*.png
if [ "${COND}" = "c2-memory" ]; then
  cp aitl-raytracer/.mcp.json start/.mcp.json   # gitignored: lleva credenciales
fi
echo "→ base reanclada + plantilla ${COND} instalada; modelo del agente: ${MODEL_ID}"

# ── Curso: fases 00→05 con deadline global ────────────────────────────────────
T0=$(date +%s)
DEADLINE=$(( T0 + COURSE_MIN * 60 ))
STOP_REASON="curso completo (fases 0–5)"
PHASE_TABLE="fase,run_id,status,gate,dur_s"   # se alimenta al resumen final

for FASE in 0 1 2 3 4 5; do
  NOW=$(date +%s); LEFT=$(( DEADLINE - NOW ))
  if [ "${LEFT}" -le 120 ]; then
    STOP_REASON="tope de ${COURSE_MIN} min alcanzado antes de la fase ${FASE}"
    echo "⚠ ${STOP_REASON}"
    break
  fi

  SPEC="${ROOT}/start/sdd/phase-0${FASE}/spec.md"
  PROMPT="$(awk '/^## Prompt de tarea/{f=1;next} /^## /{f=0} f' "${SPEC}")"
  LOG="${LOG_DIR}/curso-${CELL_TAG}-fase${FASE}-${STAMP}.log"
  echo "→ fase ${FASE} (quedan $((LEFT / 60)) min) — log: ${LOG}"

  # Higiene de renders: cada fase parte sin .ppm/.png rancios; el gate los regenera.
  rm -f start/*.ppm start/*.png

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
  PHASE_TABLE="${PHASE_TABLE}
${FASE},${RUN_ID:-—},${RUN_STATUS},${GATE},${DUR_S}"

  # Métricas al CSV único (data/metricas.csv). Si Mongo estaba caído el run_id viene
  # vacío pero la fila se registra igual con status/gate/dur_s — el trabajo NO se pierde.
  bash "${ROOT}/scripts/collect-metrics.sh" "${RUN_ID:--}" "${FASE}" "${CELL}" "${GATE}" "${DUR_S}" "${RUN_STATUS}" || true

  # Evidencia junto al .runshow.txt: código + renders COMPRIMIDOS a PNG (los .ppm pesan
  # ~9 MB c/u; el PNG queda en decenas de KB). El gate ya corrió con el .ppm crudo.
  EV_PFX="${ROOT}/data/runs/fase${FASE}-${CELL}-${RUN_ID:-sin-id}"
  cp start/rt.cpp "${EV_PFX}.rt.cpp" 2>/dev/null || true
  for F in start/*.ppm; do
    [ -f "${F}" ] || continue
    BASE="$(basename "${F%.ppm}")"
    python3 -c "from PIL import Image; import sys; Image.open(sys.argv[1]).save(sys.argv[2])" \
      "${F}" "${EV_PFX}.${BASE}.png" 2>/dev/null || cp "${F}" "${EV_PFX}.${BASE}.ppm"
  done

  # Commit del avance + TAG de la fase (rama única `curso`; renders gitignored).
  git add start/rt.cpp start/Makefile start/CLAUDE.md start/.claude
  git commit -q -m "curso ${CELL}: fase ${FASE} run=${RUN_STATUS} gate=${GATE} (${DUR_S}s, run ${RUN_ID:-sin-id})" || true
  git tag -f "celda/${CELL_TAG}/f${FASE}" >/dev/null 2>&1 || true

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
RESUMEN_DST="${CURSO_DIR}/resumen-${CELL_TAG}.md"
RESUMEN_PROMPT="Acabas de terminar la celda '${CELL}' del experimento: completar el curso del
raytracer (fases 00→05, una por proyecto) con tope de ${COURSE_MIN} minutos.
Resultado global: ${STOP_REASON}. Tiempo total usado: ${TOTAL_S} s.
Datos por fase (fase,run_id,status,gate,dur_s):
${PHASE_TABLE}

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
  --cwd "${ROOT}/start" </dev/null 2>&1 | tee "${LOG_DIR}/curso-${CELL_TAG}-resumen-${STAMP}.log"
set -e
if [ -f "${ROOT}/start/RESUMEN-CURSO.md" ]; then
  mv "${ROOT}/start/RESUMEN-CURSO.md" "${RESUMEN_DST}"
  echo "→ resumen de la celda: ${RESUMEN_DST}"
else
  echo "⚠ el agente no dejó RESUMEN-CURSO.md — revisa el log del resumen."
fi

echo ""
echo "→ celda ${CELL} terminada."
echo "   resumen:  ${RESUMEN_DST}"
echo "   métricas: data/metricas.csv (cell=${CELL}) + data/runs/*.runshow.txt"
echo "   evidencia: data/runs/fase*-${CELL}-*.png (renders) + *.rt.cpp (código)"
echo "   código:   rama ${BRANCH}, tags celda/${CELL_TAG}/f0..f5 (un commit+tag por fase)"
