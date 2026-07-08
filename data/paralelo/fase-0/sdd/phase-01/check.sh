#!/usr/bin/env bash
# Gate objetivo de Phase 01 — iluminación directa por Monte Carlo (criterios de spec.md).
# Se ejecuta desde la raíz del proyecto del agente (cwd = start/):
#   make && bash sdd/phase-01/check.sh
# TODA ejecución de ./rt va con stdin cerrado (</dev/null) — nunca se espera un TTY.
# Tolerancias calibradas contra los renders del profesor (objective/Proyecto 2 Parte 1):
#   32 spp correcto: media |Δ| reducida ≤ 4.6 medida (gate 6.0); sampler equivocado ≥ 8.2.
set -euo pipefail

RT=./rt
fail() { echo "✗ check phase-01: $*" >&2; exit 1; }

command -v python3 >/dev/null || fail "se necesita python3 (con Pillow)"
python3 -c "import PIL" 2>/dev/null || fail "falta Pillow (python3 -m pip install Pillow)"
[ -x "$RT" ] || fail "no existe ./rt — corre make primero"
[ -f sdd/tools/compare-ref.py ] || fail "falta sdd/tools/compare-ref.py"

render() { # render <sampler> <spp> <destino>
  rm -f image.ppm
  timeout 600 "$RT" "$1" "$2" </dev/null >/dev/null 2>&1 \
    || fail "'./rt $1 $2' terminó con error o superó 600 s"
  [ -s image.ppm ] || fail "'./rt $1 $2' no produjo image.ppm"
  cp image.ppm "$3"
}

# ── Criterio 5: reproducibilidad (RF-4) — misma imagen con 1 o N hilos ────────
render cosinehemi 32 .check-mc-a.ppm
render cosinehemi 32 .check-mc-b.ppm
cmp -s .check-mc-a.ppm .check-mc-b.ppm \
  || fail "criterio 5: dos corridas de './rt cosinehemi 32' difieren (RNG no determinista)"
rm -f image.ppm
OMP_NUM_THREADS=1 timeout 900 "$RT" cosinehemi 32 </dev/null >/dev/null 2>&1 \
  || fail "'./rt cosinehemi 32' con OMP_NUM_THREADS=1 terminó con error"
cmp -s .check-mc-a.ppm image.ppm \
  || fail "criterio 5: OMP_NUM_THREADS=1 produce una imagen distinta (RNG compartido entre hilos)"
rm -f .check-mc-a.ppm .check-mc-b.ppm

# ── Criterio 2: los tres samplers a 32 spp contra la referencia ───────────────
# La media global discrimina el sampler; la comparación reducida, la imagen.
render uniformsphere 32 image-uniformsphere32.ppm
python3 sdd/tools/compare-ref.py image-uniformsphere32.ppm sdd/phase-01/image-uniformsphere32.jpg \
  --down 64x48 --mean 6.0 --maxd 45 --gmean 36.4,36.2,35.0 --gmtol 4 --label "phase-01 uniformsphere32" || exit 1
render uniformhemi 32 image-uniformhemi32.ppm
python3 sdd/tools/compare-ref.py image-uniformhemi32.ppm sdd/phase-01/image-uniformhemi32.jpg \
  --down 64x48 --mean 6.0 --maxd 45 --gmean 46.0,46.0,44.1 --gmtol 4 --label "phase-01 uniformhemi32" || exit 1
render cosinehemi 32 image-cosinehemi32.ppm
python3 sdd/tools/compare-ref.py image-cosinehemi32.ppm sdd/phase-01/image-cosinehemi32.jpg \
  --down 64x48 --mean 6.0 --maxd 45 --gmean 49.8,50.5,48.8 --gmtol 4 --label "phase-01 cosinehemi32" || exit 1

# ── Criterio 2/3: convergencia — cosinehemi a 512 spp ─────────────────────────
render cosinehemi 512 image-cosinehemi512.ppm
python3 sdd/tools/compare-ref.py image-cosinehemi512.ppm sdd/phase-01/image-cosinehemi512.jpg \
  --down 64x48 --mean 3.0 --maxd 20 --label "phase-01 cosinehemi512" || exit 1

# ── Criterio 3: la varianza baja con mejor sampler (parche 32×32 pared trasera) ─
python3 - <<'PY' || exit 1
from PIL import Image, ImageStat
import sys
var = {}
for name in ("uniformsphere", "uniformhemi", "cosinehemi"):
    img = Image.open(f"image-{name}32.ppm").convert("RGB").crop((496, 368, 528, 400))
    var[name] = sum(v ** 2 for v in ImageStat.Stat(img).stddev)
if not (var["cosinehemi"] < var["uniformhemi"] < var["uniformsphere"]):
    print(f"✗ check phase-01: criterio 3: varianza no ordenada "
          f"(cos={var['cosinehemi']:.1f}, hemi={var['uniformhemi']:.1f}, "
          f"sph={var['uniformsphere']:.1f})", file=sys.stderr)
    sys.exit(1)
print(f"✓ varianza ordenada: cos {var['cosinehemi']:.1f} < hemi {var['uniformhemi']:.1f} "
      f"< sph {var['uniformsphere']:.1f}")
PY

echo "✓ check phase-01: criterios en verde (reproducibilidad, 3 samplers vs referencia, convergencia, varianza)"
