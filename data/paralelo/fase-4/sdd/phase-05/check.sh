#!/usr/bin/env bash
# Gate objetivo de Phase 05 — Path Tracing explícito con MIS (criterios 5b de spec.md).
# Se ejecuta desde la raíz del proyecto del agente (cwd = start/):
#   make && bash sdd/phase-05/check.sh
# TODA ejecución de ./rt va con stdin cerrado (</dev/null) — nunca se espera un TTY.
# Tolerancias calibradas contra el código del curso: positivos 0.53–0.95 (maxd ≤16);
# escena/fórmula equivocada ≥12.5 (maxd ≥187); PointL con I=2000 desvía la media
# global ~29 unidades (>>5). Los renders de 512 spp se validan sobre los entregables
# (re-renderizarlos costaría ~30+ min). El gate de 5a es la propia convergencia de 5b
# (usa el estimador MIS en cada vértice) más las autoverificaciones del prompt.
set -euo pipefail

RT=./rt
REF=sdd/phase-05
fail() { echo "✗ check phase-05: $*" >&2; exit 1; }

command -v python3 >/dev/null || fail "se necesita python3 (con Pillow)"
python3 -c "import PIL" 2>/dev/null || fail "falta Pillow (python3 -m pip install Pillow)"
[ -x "$RT" ] || fail "no existe ./rt — corre make primero"
[ -f sdd/tools/compare-ref.py ] || fail "falta sdd/tools/compare-ref.py"

# ppm_valid <archivo> — P3, 1024x768, todos los valores enteros en [0,255].
# Caza los píxeles NaN (toDisplayValue(NaN) → tokens negativos gigantes).
ppm_valid() {
  python3 - "$1" <<'PY' || exit 1
import sys
toks = " ".join(l.split("#")[0] for l in open(sys.argv[1]).read().splitlines()).split()
if toks[:1] != ["P3"]:
    print(f"✗ check phase-05: {sys.argv[1]}: no es PPM P3", file=sys.stderr); sys.exit(1)
try:
    w, h, mx = int(toks[1]), int(toks[2]), int(toks[3])
    vals = [int(v) for v in toks[4:]]
except ValueError:
    print(f"✗ check phase-05: {sys.argv[1]}: token no numérico (¿NaN?)", file=sys.stderr); sys.exit(1)
if (w, h, mx) != (1024, 768, 255) or len(vals) != w * h * 3:
    print(f"✗ check phase-05: {sys.argv[1]}: cabecera/tamaño inválido", file=sys.stderr); sys.exit(1)
bad = sum(1 for v in vals if v < 0 or v > 255)
if bad:
    print(f"✗ check phase-05: {sys.argv[1]}: {bad} valores fuera de [0,255] "
          f"(criterio 5b.5: píxeles NaN/negativos — faltan guardas en las pdf)", file=sys.stderr)
    sys.exit(1)
PY
}

render() { # render <spp> <escena> <destino> <timeout_s>
  rm -f image.ppm
  timeout "$4" "$RT" ptexp "$1" "$2" </dev/null >/dev/null 2>&1 \
    || fail "'./rt ptexp $1 $2' terminó con error o superó $4 s"
  [ -s image.ppm ] || fail "'./rt ptexp $1 $2' no produjo image.ppm"
  cp image.ppm "$3"
}

# ── Criterio 4: reproducibilidad (escena point: la más rápida) ────────────────
render 32 point .check-p5-a.ppm 1200
render 32 point .check-p5-b.ppm 1200
cmp -s .check-p5-a.ppm .check-p5-b.ppm \
  || fail "criterio 4: dos corridas de './rt ptexp 32 point' difieren (RNG no determinista)"
rm -f image.ppm
OMP_NUM_THREADS=1 timeout 2400 "$RT" ptexp 32 point </dev/null >/dev/null 2>&1 \
  || fail "'./rt ptexp 32 point' con OMP_NUM_THREADS=1 terminó con error o superó 40 min"
cmp -s .check-p5-a.ppm image.ppm \
  || fail "criterio 4: OMP_NUM_THREADS=1 produce una imagen distinta (RNG compartido entre hilos)"
cp .check-p5-a.ppm image-PTEXP-PointL-10b-32.ppm
rm -f .check-p5-a.ppm .check-p5-b.ppm

# ── Criterios 1 y 5: las tres escenas a 32 spp (re-render) ────────────────────
ppm_valid image-PTEXP-PointL-10b-32.ppm
python3 sdd/tools/compare-ref.py image-PTEXP-PointL-10b-32.ppm "${REF}/image-PTEXP-PointL-10b-32.jpg" \
  --down 64x48 --mean 3.0 --maxd 40 --gmean 110.1,120.3,98.5 --gmtol 5 --label "phase-05 PointL-32" || exit 1

render 32 areal image-PTEXP-AreaL-10b-32.ppm 1800
ppm_valid image-PTEXP-AreaL-10b-32.ppm
python3 sdd/tools/compare-ref.py image-PTEXP-AreaL-10b-32.ppm "${REF}/image-PTEXP-AreaL-10b-32.jpg" \
  --down 64x48 --mean 3.0 --maxd 40 --gmean 105.2,112.9,95.4 --gmtol 5 --label "phase-05 AreaL-32" || exit 1

render 32 multil image-PTEXP-MultiL-10b-32.ppm 1800
ppm_valid image-PTEXP-MultiL-10b-32.ppm
python3 sdd/tools/compare-ref.py image-PTEXP-MultiL-10b-32.ppm "${REF}/image-PTEXP-MultiL-10b-32.jpg" \
  --down 64x48 --mean 3.0 --maxd 40 --gmean 135.0,121.1,106.1 --gmtol 5 --label "phase-05 MultiL-32" || exit 1

# ── Criterio 1: entregables de 512 spp (sin re-render) ────────────────────────
for ESC in AreaL PointL MultiL; do
  F="image-PTEXP-${ESC}-10b-512.ppm"
  [ -s "${F}" ] || fail "falta el entregable ${F} (genera './rt ptexp 512 <escena>' y renombra)"
  ppm_valid "${F}"
done
python3 sdd/tools/compare-ref.py image-PTEXP-AreaL-10b-512.ppm "${REF}/image-PTEXP-AreaL-10b-512.jpg" \
  --down 64x48 --mean 3.0 --maxd 40 --gmean 105.5,113.2,95.6 --gmtol 5 --label "phase-05 AreaL-512" || exit 1
python3 sdd/tools/compare-ref.py image-PTEXP-PointL-10b-512.ppm "${REF}/image-PTEXP-PointL-10b-512.jpg" \
  --down 64x48 --mean 3.0 --maxd 40 --gmean 110.5,120.7,98.7 --gmtol 5 --label "phase-05 PointL-512" || exit 1
python3 sdd/tools/compare-ref.py image-PTEXP-MultiL-10b-512.ppm "${REF}/image-PTEXP-MultiL-10b-512.jpg" \
  --down 64x48 --mean 3.0 --maxd 40 --gmean 135.8,121.6,106.4 --gmtol 5 --label "phase-05 MultiL-512" || exit 1

echo "✓ check phase-05: criterios en verde (reproducibilidad, 3 escenas a 32 spp, entregables 512, sin NaN)"
