#!/usr/bin/env bash
# Gate objetivo de Phase 03 — muestreo de importancia de fuentes (criterios de spec.md).
# Se ejecuta desde la raíz del proyecto del agente (cwd = start/):
#   make && bash sdd/phase-03/check.sh
# TODA ejecución de ./rt va con stdin cerrado (</dev/null) — nunca se espera un TTY.
# Tolerancias calibradas contra la reconstrucción validada (ds = media |Δ| a 64×48 BOX;
# pix = media |Δ| a resolución completa): correctos ds ≤ 0.74 / pix ≤ 10.7; escena
# equivocada ds = 21; puntual ausente ds ≈ 6; método intercambiado pix > 2.5 en SA.
# La banda de ruido (std del parche trasero, ±50% del de la referencia) distingue
# área vs ángulo sólido (misma integral, distinta varianza).
set -euo pipefail

RT=./rt
REF=sdd/phase-03
fail() { echo "✗ check phase-03: $*" >&2; exit 1; }

command -v python3 >/dev/null || fail "se necesita python3 (con Pillow)"
python3 -c "import PIL" 2>/dev/null || fail "falta Pillow (python3 -m pip install Pillow)"
[ -x "$RT" ] || fail "no existe ./rt — corre make primero"
[ -f sdd/tools/compare-ref.py ] || fail "falta sdd/tools/compare-ref.py"

render() { # render <modo> <spp> <escena> <destino> <timeout_s>
  rm -f image.ppm
  timeout "$5" "$RT" "$1" "$2" "$3" </dev/null >/dev/null 2>&1 \
    || fail "'./rt $1 $2 $3' terminó con error o superó $5 s"
  [ -s image.ppm ] || fail "'./rt $1 $2 $3' no produjo image.ppm"
  cp image.ppm "$4"
}

# std_band <render.ppm> <ref.jpg> <etiqueta>: ruido del parche trasero dentro de
# ±50% del de la referencia (discrimina área vs ángulo sólido).
std_band() {
  python3 - "$1" "$2" "$3" <<'PY' || exit 1
import sys
from PIL import Image, ImageStat
box = (480, 352, 544, 416)  # parche 64x64 de pared trasera
sa = ImageStat.Stat(Image.open(sys.argv[1]).convert("RGB").crop(box))
sb = ImageStat.Stat(Image.open(sys.argv[2]).convert("RGB").crop(box))
ra = sum(sa.stddev) / 3
rb = sum(sb.stddev) / 3
if not (0.5 * rb <= ra <= 1.5 * rb):
    print(f"✗ check phase-03 [{sys.argv[3]}]: ruido del parche trasero = {ra:.2f}, "
          f"referencia = {rb:.2f} (banda ±50%) — ¿método de muestreo equivocado?",
          file=sys.stderr)
    sys.exit(1)
print(f"✓ ruido [{sys.argv[3]}]: {ra:.2f} vs ref {rb:.2f} (banda ±50%)")
PY
}

# ── Criterio 5: reproducibilidad ──────────────────────────────────────────────
render solidangle 32 1L .check-p3-a.ppm 600
render solidangle 32 1L .check-p3-b.ppm 600
cmp -s .check-p3-a.ppm .check-p3-b.ppm \
  || fail "criterio 5: dos corridas de './rt solidangle 32 1L' difieren (RNG no determinista)"
rm -f image.ppm
OMP_NUM_THREADS=1 timeout 900 "$RT" solidangle 32 1L </dev/null >/dev/null 2>&1 \
  || fail "'./rt solidangle 32 1L' con OMP_NUM_THREADS=1 terminó con error"
cmp -s .check-p3-a.ppm image.ppm \
  || fail "criterio 5: OMP_NUM_THREADS=1 produce una imagen distinta (RNG compartido entre hilos)"
cp .check-p3-a.ppm image-ISLightSolidAngle-32.ppm
rm -f .check-p3-a.ppm .check-p3-b.ppm

# ── Criterio 1: escena 1L, ambos métodos ──────────────────────────────────────
python3 sdd/tools/compare-ref.py image-ISLightSolidAngle-32.ppm "${REF}/image-ISLightSolidAngle-32.jpg" \
  --down 64x48 --mean 1.5 --gmean 74,77,72 --gmtol 3 --label "phase-03 SA-1L" || exit 1
python3 sdd/tools/compare-ref.py image-ISLightSolidAngle-32.ppm "${REF}/image-ISLightSolidAngle-32.jpg" \
  --mean 2.5 --thr 254 --pct 100 --label "phase-03 SA-1L pix" || exit 1
std_band image-ISLightSolidAngle-32.ppm "${REF}/image-ISLightSolidAngle-32.jpg" "SA-1L"

render arealight 32 1L image-ISLightArea-32.ppm 600
python3 sdd/tools/compare-ref.py image-ISLightArea-32.ppm "${REF}/image-ISLightArea-32.jpg" \
  --down 64x48 --mean 2.0 --gmean 73,76,71 --gmtol 3 --label "phase-03 Area-1L" || exit 1
python3 sdd/tools/compare-ref.py image-ISLightArea-32.ppm "${REF}/image-ISLightArea-32.jpg" \
  --mean 14 --thr 254 --pct 100 --label "phase-03 Area-1L pix" || exit 1
std_band image-ISLightArea-32.ppm "${REF}/image-ISLightArea-32.jpg" "Area-1L"

# ── Criterio 3: escena 2A1P, ambos métodos + coshemi 2048 ─────────────────────
render arealight 32 2A1P image-2A1P-ISLightArea-32.ppm 600
python3 sdd/tools/compare-ref.py image-2A1P-ISLightArea-32.ppm "${REF}/image-2A1P-ISLightArea-32.jpg" \
  --down 64x48 --mean 2.0 --gmean 95,83,81 --gmtol 3 --label "phase-03 Area-2A1P" || exit 1
python3 sdd/tools/compare-ref.py image-2A1P-ISLightArea-32.ppm "${REF}/image-2A1P-ISLightArea-32.jpg" \
  --mean 10 --thr 254 --pct 100 --label "phase-03 Area-2A1P pix" || exit 1
std_band image-2A1P-ISLightArea-32.ppm "${REF}/image-2A1P-ISLightArea-32.jpg" "Area-2A1P"

render solidangle 32 2A1P image-2A1P-ISSolidAngle-32.ppm 600
python3 sdd/tools/compare-ref.py image-2A1P-ISSolidAngle-32.ppm "${REF}/image-2A1P-ISSolidAngle-32.jpg" \
  --down 64x48 --mean 1.5 --gmean 96,83,81 --gmtol 3 --label "phase-03 SA-2A1P" || exit 1
python3 sdd/tools/compare-ref.py image-2A1P-ISSolidAngle-32.ppm "${REF}/image-2A1P-ISSolidAngle-32.jpg" \
  --mean 2.5 --thr 254 --pct 100 --label "phase-03 SA-2A1P pix" || exit 1
std_band image-2A1P-ISSolidAngle-32.ppm "${REF}/image-2A1P-ISSolidAngle-32.jpg" "SA-2A1P"

render coshemi 2048 2A1P image-2A1P-cosinehemi-2048.ppm 1800
python3 sdd/tools/compare-ref.py image-2A1P-cosinehemi-2048.ppm "${REF}/image-2A1P-cosinehemi-2048.jpg" \
  --down 64x48 --mean 2.0 --gmean 96,83,81 --gmtol 3 --label "phase-03 coshemi-2048" || exit 1
python3 sdd/tools/compare-ref.py image-2A1P-cosinehemi-2048.ppm "${REF}/image-2A1P-cosinehemi-2048.jpg" \
  --mean 6 --thr 254 --pct 100 --label "phase-03 coshemi-2048 pix" || exit 1

echo "✓ check phase-03: criterios en verde (reproducibilidad, 1L y 2A1P vs referencia, bandas de ruido)"
