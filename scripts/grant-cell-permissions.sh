#!/usr/bin/env bash
# Contrato de permisos de los agentes host (claude-code headless) por celda.
#
# Escribe en las 3 plantillas de condición el presupuesto de herramientas que la
# fase necesita: editar código, compilar (make/g++), ejecutar ./rt, correr el
# gate (bash sdd/*/check.sh), python3 y utilidades de archivo. Mantiene la veda
# de objective/ (los agentes JAMÁS ven los renders de referencia).
#
# DEBE ejecutarlo el investigador (humano): otorgar shell sin gate por acción a
# agentes autónomos es una decisión de autorización, no de automatización.
#
# Uso:  bash scripts/grant-cell-permissions.sh
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."

python3 - <<'EOF'
import json

ALLOW = [
    "Edit", "Write",
    "Bash(make:*)", "Bash(make)",
    "Bash(./rt:*)", "Bash(./rt)",
    "Bash(bash:*)", "Bash(sh:*)",
    "Bash(python3:*)", "Bash(python:*)",
    "Bash(g++:*)", "Bash(cmp:*)", "Bash(diff:*)",
    "Bash(cp:*)", "Bash(mv:*)", "Bash(rm:*)", "Bash(touch:*)", "Bash(mkdir:*)",
    "Bash(ls:*)", "Bash(cat:*)", "Bash(head:*)", "Bash(tail:*)", "Bash(grep:*)",
    "Bash(find:*)", "Bash(wc:*)", "Bash(stat:*)", "Bash(sed:*)", "Bash(awk:*)",
    "Bash(sort:*)", "Bash(uniq:*)", "Bash(md5sum:*)", "Bash(sha256sum:*)",
    "Bash(echo:*)", "Bash(time:*)", "Bash(timeout:*)", "Bash(env:*)",
    "Bash(pwd)", "Bash(readlink:*)", "Bash(file:*)", "Bash(tr:*)", "Bash(cut:*)",
    "Bash(date:*)", "Bash(nproc)", "Bash(od:*)",
    "Bash(OMP_NUM_THREADS=1 ./rt:*)", "Bash(OMP_NUM_THREADS=2 ./rt:*)",
    "Bash(OMP_NUM_THREADS=4 ./rt:*)",
]
DENY = [
    "Read(../objective/**)",
    "Read(//home/eversuelo/Code/thesis-harness/metricas/raytracer/objective/**)",
    "Bash(*objective*)",
]

for c in ["c0-bare", "c2-memory", "c2-orchestrator"]:
    p = f"start/conditions/{c}/.claude/settings.json"
    s = json.load(open(p))
    perms = s.setdefault("permissions", {})
    perms["defaultMode"] = "acceptEdits"
    perms["allow"] = ALLOW
    perms["deny"] = DENY
    with open(p, "w") as f:
        json.dump(s, f, indent=2, ensure_ascii=False)
        f.write("\n")
    print(f"✓ {p}: {len(ALLOW)} allow / {len(DENY)} deny, defaultMode=acceptEdits")
EOF

echo "→ listo: revisa con 'git diff start/conditions' y avisa para relanzar las celdas"
