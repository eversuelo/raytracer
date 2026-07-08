#!/usr/bin/env bash
# Gate objetivo de Phase 00 — implementa los 5 criterios de aceptación de spec.md.
# Se ejecuta desde la raíz del proyecto del agente (cwd = start/):
#   make && bash sdd/phase-00/check.sh
# Los programas pueden ser terminales interactivos: TODA ejecución de ./rt va con
# stdin cerrado (</dev/null) — nunca se espera un TTY.
set -euo pipefail

RT=./rt
fail() { echo "✗ check phase-00: $*" >&2; exit 1; }

command -v python3 >/dev/null || fail "se necesita python3 para sondear los PPM"
[ -x "$RT" ] || fail "no existe ./rt — corre make primero"
[ -f sdd/phase-00/probe.py ] || fail "falta sdd/phase-00/probe.py"

render() { # render <modo> <destino>
  rm -f image.ppm
  timeout 180 "$RT" "$1" </dev/null >/dev/null 2>&1 || fail "'./rt $1' terminó con error o superó 180 s"
  [ -s image.ppm ] || fail "'./rt $1' no produjo image.ppm en la raíz del proyecto"
  cp image.ppm "$2"
}

# ── Criterio 4: determinismo (dos corridas idénticas; 1 hilo == default) ──────
render normal image-normal.ppm
render normal .check-normal-2.ppm
cmp -s image-normal.ppm .check-normal-2.ppm \
  || fail "criterio 4: dos corridas consecutivas de './rt normal' difieren (no determinista)"
rm -f .check-normal-2.ppm image.ppm
OMP_NUM_THREADS=1 timeout 300 "$RT" normal </dev/null >/dev/null 2>&1 \
  || fail "'./rt normal' con OMP_NUM_THREADS=1 terminó con error"
cmp -s image-normal.ppm image.ppm \
  || fail "criterio 4: OMP_NUM_THREADS=1 produce una imagen distinta al default (no determinista)"

render distance image-distance.ppm
render distance .check-distance-2.ppm
cmp -s image-distance.ppm .check-distance-2.ppm \
  || fail "criterio 4: dos corridas consecutivas de './rt distance' difieren (no determinista)"
rm -f .check-distance-2.ppm

# ── Los dos modos deben responder al argumento (imágenes distintas) ───────────
if cmp -s image-normal.ppm image-distance.ppm; then
  fail "'./rt normal' y './rt distance' producen la MISMA imagen — los modos no responden al argumento"
fi

# ── RF-4: paralelización OpenMP presente en el bucle de píxeles ───────────────
grep -Eq '#[[:space:]]*pragma[[:space:]]+omp[[:space:]]+parallel' rt.cpp \
  || fail "RF-4: no hay '#pragma omp parallel' en rt.cpp"

# ── Criterio 5: el delete[] existente se conserva ─────────────────────────────
grep -q 'delete\[\]' rt.cpp \
  || fail "criterio 5: desapareció el 'delete[]' de pixelColors en rt.cpp"

# ── Criterios 2 y 3: sondeo de píxeles contra la spec (RF-3 y referencias) ────
python3 sdd/phase-00/probe.py image-normal.ppm image-distance.ppm || exit 1

echo "✓ check phase-00: los 5 criterios de aceptación en verde"
