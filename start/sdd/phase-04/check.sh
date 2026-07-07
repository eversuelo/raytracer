#!/usr/bin/env bash
# Gate objetivo de Phase 04 — microfacets (criterios de spec.md).
# Se ejecuta desde la raíz del proyecto del agente (cwd = start/):
#   make && bash sdd/phase-04/check.sh
# TODA ejecución de ./rt va con stdin cerrado (</dev/null) — nunca se espera un TTY.
# Tolerancias calibradas contra la solución del profesor (objective/Proyecto 4):
#   variantes de luz correctas ≤ 0.74/9 (gate 2.0/12); escena equivocada ≥ 19;
#   método IS equivocado max 22–27. ISBRDF-512 se valida sobre el entregable (el
#   re-render costaría ~5 min).
set -euo pipefail

RT=./rt
REF=sdd/phase-04
fail() { echo "✗ check phase-04: $*" >&2; exit 1; }

command -v python3 >/dev/null || fail "se necesita python3 (con Pillow)"
python3 -c "import PIL" 2>/dev/null || fail "falta Pillow (python3 -m pip install Pillow)"
[ -x "$RT" ] || fail "no existe ./rt — corre make primero"
[ -f sdd/tools/compare-ref.py ] || fail "falta sdd/tools/compare-ref.py"

render() { # render <modo> <spp> <escena> <destino>
  rm -f image.ppm
  timeout 600 "$RT" "$1" "$2" "$3" </dev/null >/dev/null 2>&1 \
    || fail "'./rt $1 $2 $3' terminó con error o superó 600 s"
  [ -s image.ppm ] || fail "'./rt $1 $2 $3' no produjo image.ppm"
  cp image.ppm "$4"
}

# ── Criterio 5: reproducibilidad (escena point: la más rápida) ────────────────
render issa 32 point .check-p4-a.ppm
render issa 32 point .check-p4-b.ppm
cmp -s .check-p4-a.ppm .check-p4-b.ppm \
  || fail "criterio 5: dos corridas de './rt issa 32 point' difieren (RNG no determinista)"
rm -f image.ppm
OMP_NUM_THREADS=1 timeout 900 "$RT" issa 32 point </dev/null >/dev/null 2>&1 \
  || fail "'./rt issa 32 point' con OMP_NUM_THREADS=1 terminó con error"
cmp -s .check-p4-a.ppm image.ppm \
  || fail "criterio 5: OMP_NUM_THREADS=1 produce una imagen distinta (RNG compartido entre hilos)"
cp .check-p4-a.ppm image-RC-Point-32.ppm
rm -f .check-p4-a.ppm .check-p4-b.ppm

# ── Criterio 1: variantes de muestreo de luz a 32 spp ─────────────────────────
python3 sdd/tools/compare-ref.py image-RC-Point-32.ppm "${REF}/image-RC-Point-32.jpg" \
  --down 64x48 --mean 2.0 --maxd 12 --label "phase-04 RC-Point" || exit 1
render issa 32 2a1p image-RC-ISSA-32.ppm
python3 sdd/tools/compare-ref.py image-RC-ISSA-32.ppm "${REF}/image-RC-ISSA-32.jpg" \
  --down 64x48 --mean 2.0 --maxd 12 --label "phase-04 RC-ISSA" || exit 1
render isarea 32 2a1p image-RC-ISArea-32.ppm
python3 sdd/tools/compare-ref.py image-RC-ISArea-32.ppm "${REF}/image-RC-ISArea-32.jpg" \
  --down 64x48 --mean 2.0 --maxd 12 --label "phase-04 RC-ISArea" || exit 1
render issa 32 areal image-RC-AreaL-ISSA-32.ppm
python3 sdd/tools/compare-ref.py image-RC-AreaL-ISSA-32.ppm "${REF}/image-RC-AreaL-ISSA-32.jpg" \
  --down 64x48 --mean 2.0 --maxd 12 --label "phase-04 RC-AreaL-ISSA" || exit 1
render isarea 32 areal image-RC-AreaL-ISArea-32.ppm
python3 sdd/tools/compare-ref.py image-RC-AreaL-ISArea-32.ppm "${REF}/image-RC-AreaL-ISArea-32.jpg" \
  --down 64x48 --mean 2.0 --maxd 12 --label "phase-04 RC-AreaL-ISArea" || exit 1

# ── Criterio 1: muestreo de BRDF (escena areal) ───────────────────────────────
render isbrdf 32 areal image-ISBRDF-32.ppm
python3 sdd/tools/compare-ref.py image-ISBRDF-32.ppm "${REF}/image-ISBRDF-32.jpg" \
  --down 64x48 --mean 6.0 --maxd 40 --label "phase-04 ISBRDF-32" || exit 1
[ -s image-ISBRDF-512.ppm ] \
  || fail "falta el entregable image-ISBRDF-512.ppm (genera './rt isbrdf 512 areal' y renombra)"
python3 sdd/tools/compare-ref.py image-ISBRDF-512.ppm "${REF}/image-ISBRDF-512.jpg" \
  --down 64x48 --mean 2.5 --maxd 15 --label "phase-04 ISBRDF-512" || exit 1

echo "✓ check phase-04: criterios en verde (reproducibilidad, 5 variantes RC, ISBRDF 32/512)"
