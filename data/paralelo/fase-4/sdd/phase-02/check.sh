#!/usr/bin/env bash
# Gate objetivo de Phase 02 — fuente puntual determinista (criterios de spec.md).
# Se ejecuta desde la raíz del proyecto del agente (cwd = start/):
#   make && bash sdd/phase-02/check.sh
# TODA ejecución de ./rt va con stdin cerrado (</dev/null) — nunca se espera un TTY.
set -euo pipefail

RT=./rt
fail() { echo "✗ check phase-02: $*" >&2; exit 1; }

command -v python3 >/dev/null || fail "se necesita python3 (con Pillow) para comparar contra la referencia"
python3 -c "import PIL" 2>/dev/null || fail "falta Pillow (python3 -m pip install Pillow)"
[ -x "$RT" ] || fail "no existe ./rt — corre make primero"
[ -f sdd/tools/compare-ref.py ] || fail "falta sdd/tools/compare-ref.py"

# render_point <destino>: corre './rt point' y deja el PPM resultante en <destino>.
# La spec fija la salida como image-plight.ppm; se acepta image.ppm como fallback.
render_point() {
  rm -f image.ppm image-plight.ppm
  timeout 300 "$RT" point </dev/null >/dev/null 2>&1 \
    || fail "'./rt point' terminó con error o superó 300 s"
  if [ -s image-plight.ppm ]; then cp image-plight.ppm "$1"
  elif [ -s image.ppm ]; then cp image.ppm "$1"
  else fail "'./rt point' no produjo image-plight.ppm (ni image.ppm)"
  fi
}

# ── Criterio 2: determinismo exacto (sin MC no hay ruido) ─────────────────────
render_point .check-plight-1.ppm
render_point .check-plight-2.ppm
cmp -s .check-plight-1.ppm .check-plight-2.ppm \
  || fail "criterio 2: dos corridas de './rt point' difieren (debe ser determinista, sin MC)"
mv .check-plight-1.ppm image-plight.ppm
rm -f .check-plight-2.ppm

# ── Criterio 1: coincide con la referencia del curso ─────────────────────────
# Tolerancias calibradas contra la reconstrucción validada (media |Δ| medida ≤ 0.7;
# 0.32% de píxeles > 12, puro ringing JPEG en siluetas).
python3 sdd/tools/compare-ref.py image-plight.ppm sdd/phase-02/image-plight.jpg \
  --mean 2.0 --thr 12 --pct 2.0 --label "phase-02 puntual" || exit 1

# ── Criterio 4: la fase 1 sigue funcionando (no regresión) ────────────────────
if [ -f sdd/phase-01/check.sh ]; then
  bash sdd/phase-01/check.sh || fail "criterio 4: el gate de la fase 1 dejó de estar verde (regresión)"
fi

echo "✓ check phase-02: criterios en verde (determinismo, referencia, no-regresión)"
