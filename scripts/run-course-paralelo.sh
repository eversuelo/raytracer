#!/usr/bin/env bash
# Curso PARALELO — celda c2-orchestrator@auto: el loop nativo del harness (modelo local
# de LM Studio) RUTEA cada fase a un tier de Claude (haiku/sonnet/opus) y lanza las 6
# delegaciones EN PARALELO, cada una en su workspace aislado con su propio rt.cpp y su
# gate. Viable porque cada '## Prompt de tarea' es autocontenido (validado contra
# objective/) y cada check.sh evalúa el rt.cpp de SU carpeta.
#
# Uso:
#   scripts/run-course-paralelo.sh            # tope global COURSE_MIN (default 90 min)
#
# Qué mide (≠ run-course.sh): 6 tareas independientes desde la base + la capacidad de
# RUTEO y paralelización del orquestador. NO es comparable fase a fase con el curso
# secuencial: aquí la fase N no hereda el código de la N-1, y dur_s corre con las 6
# fases compitiendo por CPU (renders OpenMP). Comparar a nivel celda, no fila.
#
# ⚠ Corre SOLO cuando ninguna otra celda esté activa (comparte máquina, Mongo y git).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CELL="c2-orchestrator@auto"
CELL_TAG="c2-orchestrator-auto"
BRANCH="curso"
COURSE_MIN="${COURSE_MIN:-90}"
STAMP="$(date +%Y%m%d-%H%M%S)"
LOG_DIR="${ROOT}/data/logs"; mkdir -p "${LOG_DIR}"
CURSO_DIR="${ROOT}/data/curso"; mkdir -p "${CURSO_DIR}"
PAR_DIR="${ROOT}/data/paralelo"
LOG="${LOG_DIR}/paralelo-${CELL_TAG}-${STAMP}.log"

die() { echo "✗ $*" >&2; exit 1; }

# ── Preflight ────────────────────────────────────────────────────────────────
command -v aitl >/dev/null    || die "no encuentro 'aitl' en PATH"
command -v claude >/dev/null  || die "no encuentro 'claude' (Claude Code) en PATH"
command -v python3 >/dev/null || die "no encuentro 'python3'"
python3 -c "import PIL" 2>/dev/null || die "falta Pillow: pip install pillow"
[ -f "${ROOT}/aitl-raytracer/.mcp.json" ] || die "falta aitl-raytracer/.mcp.json (credenciales del MCP)"
# Solo una INVOCACIÓN real (script + condición); un shell que apenas mencione el
# nombre (monitor/cola) no debe disparar el preflight.
pgrep -f 'run-course\.sh (c0-bare|c2-memory|c2-orchestrator)' >/dev/null \
  && die "hay otra celda corriendo (run-course.sh) — una a la vez"
# El id efectivo del orquestador sale de env > ~/.aitl/config.json (ver run-course.sh).
LM_LINE="$(cd "${ROOT}" && aitl models 2>/dev/null | grep -i 'lmstudio' || true)"
echo "${LM_LINE}" | grep -qi 'activo' \
  || die "necesita LM Studio ACTIVO: lms load <modelo> -c 16384 --parallel 1 -y"
echo "→ orquestador LM Studio: $(echo "${LM_LINE}" | awk '{print $3}')"
for F in 0 1 2 3 4 5; do
  S="${ROOT}/start/sdd/phase-0${F}/spec.md"
  [ -f "${S}" ] || die "no existe la spec ${S}"
  grep -q "status: approved" "${S}" || die "spec de fase ${F} NO aprobada"
  awk '/^## Prompt de tarea/{f=1;next} /^## /{f=0} f' "${S}" | grep -q . \
    || die "la spec de fase ${F} no tiene '## Prompt de tarea'"
done

cd "${ROOT}"
git rev-parse --verify -q "${BRANCH}" >/dev/null || die "no existe la rama ${BRANCH}"
[ "$(git branch --show-current)" = "${BRANCH}" ] || git checkout -q "${BRANCH}"
echo "→ rama ${BRANCH} · celda ${CELL} (tope ${COURSE_MIN} min, 6 fases en paralelo)"

# ── Workspaces aislados: fase-N parte de la base, con specs/gates y plantilla C2 ──
rm -rf "${PAR_DIR}"; mkdir -p "${PAR_DIR}"
for F in 0 1 2 3 4 5; do
  WS="${PAR_DIR}/fase-${F}"
  mkdir -p "${WS}/out" "${WS}/.claude"
  cp start/sdd/base/rt.cpp   "${WS}/rt.cpp"
  cp start/sdd/base/Makefile "${WS}/Makefile"
  cp -r start/sdd            "${WS}/sdd"
  cp start/conditions/c2-orchestrator/CLAUDE.md "${WS}/CLAUDE.md"
  cp start/conditions/c2-orchestrator/.claude/settings.json "${WS}/.claude/settings.json"
  sed -i 's/"model": *"[^"]*"/"model": "claude-sonnet-5"/' "${WS}/.claude/settings.json"
  cp aitl-raytracer/.mcp.json "${WS}/.mcp.json"   # gitignored: credenciales
  awk '/^## Prompt de tarea/{f=1;next} /^## /{f=0} f' "start/sdd/phase-0${F}/spec.md" \
    > "${WS}/FASE-PROMPT.txt"
done
echo "→ 6 workspaces sembrados en data/paralelo/fase-{0..5}"

# ── Helper determinista de delegación: el orquestador solo decide FASE→MODELO ─────
# y el paralelismo (&/wait). El modelo del sub-agente viaja como argv extra del host
# (AITL_HOST_ARGS_CLAUDE_CODE, hosts/base.ts) y le gana al settings.json.
cat > "${PAR_DIR}/delegar.sh" <<'HELPER'
#!/usr/bin/env bash
# delegar.sh <fase 0-5> <modelo-claude> — delega la fase a Claude Code en su workspace
# y corre su gate. Deja fase-N/delegacion.log, gate.log y GATE (ok|fail).
set -u
F="${1:?fase 0-5}"; M="${2:?modelo claude}"
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/fase-${F}"
cd "${DIR}" || exit 1
echo "modelo=${M}" > MODELO
AITL_HOST_ARGS_CLAUDE_CODE="--model ${M}" aitl run-host "$(cat FASE-PROMPT.txt)" \
  --project aitl-raytracer --host claude-code --cwd . </dev/null > delegacion.log 2>&1
GATE="fail"
( make && { [ ! -f "sdd/phase-0${F}/check.sh" ] || bash "sdd/phase-0${F}/check.sh"; } ) \
  </dev/null > gate.log 2>&1 && GATE="ok"
echo "${GATE}" > GATE
echo "fase ${F} [${M}]: gate=${GATE}"
HELPER
chmod +x "${PAR_DIR}/delegar.sh"

# ── Orquestador: UNA corrida del loop nativo; decide ruteo y lanza en paralelo ────
GATES_OK_CMD='n=0; for f in 0 1 2 3 4 5; do [ "$(cat fase-$f/GATE 2>/dev/null)" = ok ] && n=$((n+1)); done; [ "$n" = 6 ]'
ORCH_PROMPT="Eres el ORQUESTADOR de un curso de raytracing C++ de 6 fases. NO escribas ni edites código tú mismo.
En tu directorio hay 6 carpetas fase-0 .. fase-5; la tarea de cada una está en fase-N/FASE-PROMPT.txt.
Sigue EXACTAMENTE estos pasos:
1. DECIDE el modelo Claude para CADA fase según su dificultad:
   - claude-haiku-4-5-20251001 → tarea simple/mecánica
   - claude-sonnet-5 → implementación estándar
   - claude-opus-4-8 → matemática compleja
   Guía de dificultad: fase 0 intersección esfera + modos debug; fase 1 iluminación directa Monte Carlo; fase 2 fuente puntual y sombras; fase 3 muestreo por importancia con múltiples fuentes; fase 4 microfacetas conductoras; fase 5 MIS / path tracing.
2. LANZA LAS 6 FASES EN PARALELO con UNA sola llamada a tu herramienta shell con timeout=5400, sustituyendo cada <modelo>:
bash delegar.sh 0 <modelo> & bash delegar.sh 1 <modelo> & bash delegar.sh 2 <modelo> & bash delegar.sh 3 <modelo> & bash delegar.sh 4 <modelo> & bash delegar.sh 5 <modelo> & wait
3. Lee el resultado de cada fase con shell: cat fase-0/GATE fase-1/GATE fase-2/GATE fase-3/GATE fase-4/GATE fase-5/GATE
4. Si alguna fase quedó en fail, relánzala UNA sola vez con un modelo superior: bash delegar.sh <N> <modelo-superior> (shell, timeout=5400; puedes relanzar varias en paralelo con & y wait).
5. Reporta la tabla final: fase, modelo usado, gate.
Tu éxito se mide únicamente con los gates en verde."
T0=$(date +%s)
set +e
# Watchdog anti-cuelgue (ver run-course.sh): mata 'aitl run' si sigue vivo 90s
# después de imprimir su resumen run_id= — para ese punto ya está persistido.
(
  for _ in $(seq 1 $(( COURSE_MIN * 6 + 1 ))); do
    sleep 10
    grep -q '^run_id=' "${LOG}" 2>/dev/null && { sleep 90; pkill -TERM -f 'aitl run ' 2>/dev/null; break; }
  done
) &
WATCHDOG=$!
( cd "${PAR_DIR}" && timeout --foreground $(( COURSE_MIN * 60 )) \
    aitl run "${ORCH_PROMPT}" --project aitl-raytracer --model lmstudio --mcp \
    --verify-cmd "${GATES_OK_CMD}" </dev/null 2>&1 ) | tee "${LOG}"
RC=${PIPESTATUS[0]}
kill "${WATCHDOG}" 2>/dev/null || true
set -e
TOTAL_S=$(( $(date +%s) - T0 ))
ORCH_RUN_ID="$(grep -oE '^run_id=[0-9a-f-]{36}' "${LOG}" | head -1 | cut -d= -f2 || true)"
[ "${RC}" = "124" ] && echo "⚠ tope de ${COURSE_MIN} min alcanzado"
echo "→ orquestador terminó (run ${ORCH_RUN_ID:-sin-id}, $((TOTAL_S / 60)) min $((TOTAL_S % 60)) s)"

# ── Cosecha por fase: gate independiente, métricas, evidencia PNG ─────────────────
RESUMEN_DST="${CURSO_DIR}/resumen-${CELL_TAG}.md"
{
  echo "# Resumen celda ${CELL} (curso paralelo, ${STAMP})"
  echo ""
  echo "Orquestador: $(echo "${LM_LINE}" | awk '{print $3}') · run ${ORCH_RUN_ID:-sin-id} · ${TOTAL_S}s totales (6 fases en paralelo)"
  echo ""
  echo "| fase | modelo elegido | run_id | gate | dur_s |"
  echo "|------|----------------|--------|------|-------|"
} > "${RESUMEN_DST}"
for F in 0 1 2 3 4 5; do
  WS="${PAR_DIR}/fase-${F}"
  # Gate objetivo re-verificado por el script, independiente del orquestador.
  GATE="fail"
  ( cd "${WS}" && make && { [ ! -f "sdd/phase-0${F}/check.sh" ] || bash "sdd/phase-0${F}/check.sh"; } ) \
    </dev/null > "${WS}/gate-final.log" 2>&1 && GATE="ok"
  RUN_ID="$(grep -oiE '[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}' "${WS}/delegacion.log" 2>/dev/null | tail -1 || true)"
  MODELO="$(cut -d= -f2 "${WS}/MODELO" 2>/dev/null || echo '—')"
  DUR_S=""
  [ -f "${WS}/GATE" ] && DUR_S=$(( $(stat -c %Y "${WS}/GATE") - T0 ))
  echo "→ fase ${F}: modelo=${MODELO} gate=${GATE} run=${RUN_ID:-—} (${DUR_S:-—}s)"
  echo "| ${F} | ${MODELO} | ${RUN_ID:-—} | ${GATE} | ${DUR_S:-—} |" >> "${RESUMEN_DST}"
  bash "${ROOT}/scripts/collect-metrics.sh" "${RUN_ID:--}" "${F}" "${CELL}" "${GATE}" "${DUR_S:-0}" "done" || true
  # Evidencia: código + renders comprimidos a PNG + log de la delegación.
  EV_PFX="${ROOT}/data/runs/fase${F}-${CELL}-${RUN_ID:-sin-id}"
  cp "${WS}/rt.cpp" "${EV_PFX}.rt.cpp" 2>/dev/null || true
  cp "${WS}/delegacion.log" "${EV_PFX}.delegacion.log" 2>/dev/null || true
  for P in "${WS}"/*.ppm; do
    [ -f "${P}" ] || continue
    BASE="$(basename "${P%.ppm}")"
    python3 -c "from PIL import Image; import sys; Image.open(sys.argv[1]).save(sys.argv[2])" \
      "${P}" "${EV_PFX}.${BASE}.png" 2>/dev/null || true
  done
done

# ── Commit único de evidencia + tags por fase ─────────────────────────────────────
git add data/metricas.csv RESUMEN-METRICAS.md 2>/dev/null || true
git add data/curso 2>/dev/null || true
git add data/runs/*.png 2>/dev/null || true
git add data/runs/*.rt.cpp 2>/dev/null || true
git add data/runs/*.runshow.txt 2>/dev/null || true
git add data/runs/*.delegacion.log 2>/dev/null || true
git commit -q -m "curso paralelo ${CELL}: 6 fases ruteadas por el orquestador (${TOTAL_S}s, run ${ORCH_RUN_ID:-sin-id})" || true
for F in 0 1 2 3 4 5; do git tag -f "celda/${CELL_TAG}/f${F}" >/dev/null 2>&1 || true; done

echo ""
echo "→ celda ${CELL} terminada."
echo "   resumen:   ${RESUMEN_DST}"
echo "   métricas:  data/metricas.csv (cell=${CELL})"
echo "   evidencia: data/runs/fase*-${CELL}-* (png, rt.cpp, delegacion.log) — commiteada"
echo "   workspaces: data/paralelo/fase-{0..5} (efímeros, se recrean en cada corrida)"
