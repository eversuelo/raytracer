#!/usr/bin/env bash
# Observabilidad EN VIVO de la celda en curso (método host):
# sigue el transcript JSONL del claude-code anidado que trabaja en start/ y
# muestra cada tool call (Read/Edit/Bash/...), los errores (p. ej. permisos
# denegados) y los mensajes de texto del agente.
#
# Uso:
#   scripts/watch-cell.sh        # sigue el transcript más reciente (Ctrl-C sale)
#   scripts/watch-cell.sh -s     # snapshot: vuelca lo acumulado y termina
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TDIR="$HOME/.claude/projects/$(printf '%s' "${ROOT}/start" | tr '/.' '--')"
T="$(ls -t "${TDIR}"/*.jsonl 2>/dev/null | head -1 || true)"
[ -n "${T:-}" ] || { echo "✗ aún no hay transcript en ${TDIR} (¿ya arrancó la celda?)" >&2; exit 1; }
echo "→ transcript: ${T}"
echo "→ (el agente host NO puede pedir input: corre headless con 'claude -p')"

exec python3 - "${T}" "${1:-}" <<'EOF'
import json, sys, time

path, mode = sys.argv[1], (sys.argv[2] if len(sys.argv) > 2 else "")
follow = mode != "-s"

def emit(line):
    try:
        e = json.loads(line)
    except Exception:
        return
    m = e.get("message") or {}
    c = m.get("content")
    if not isinstance(c, list):
        return
    for b in c:
        t = b.get("type")
        if t == "tool_use":
            i = b.get("input", {}) or {}
            brief = (i.get("file_path") or i.get("command")
                     or i.get("description") or i.get("pattern") or "")
            print("→ " + str(b.get("name")) + ": " + str(brief)[:130], flush=True)
        elif t == "tool_result" and b.get("is_error"):
            print("   ✗ " + str(b.get("content"))[:170], flush=True)
        elif t == "text" and b.get("text"):
            print("💬 " + b["text"][:250], flush=True)

with open(path) as f:
    while True:
        line = f.readline()
        if line:
            emit(line)
        elif follow:
            time.sleep(1)
        else:
            break
EOF
