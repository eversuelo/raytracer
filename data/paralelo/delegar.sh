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
