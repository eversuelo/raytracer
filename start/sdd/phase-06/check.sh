#!/usr/bin/env bash
# Gate objetivo de Phase 06 — medios participantes (Tema 7). Autoverificación
# ANALÍTICA (no hay render de referencia): medio nulo == fase 2, absorción monótona,
# glow de in-scattering y determinismo OpenMP. Se ejecuta desde la raíz del proyecto.
set -euo pipefail

fail() { echo "✗ check phase-06: $*" >&2; exit 1; }
ok()   { echo "✓ $*"; }

command -v python3 >/dev/null || fail "falta python3"
[ -x ./rt ] || fail "no existe ./rt (corre make)"

# Estadísticos de un PPM P3: media global y media de un parche 64x64 centrado en (cx,cy)
ppm_stats() { # $1=archivo $2=cx $3=cy  → imprime "media_global media_parche min max"
  python3 - "$1" "$2" "$3" <<'PY'
import sys
path, cx, cy = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
tok = open(path).read().split()
assert tok[0] == "P3", "no es P3"
w, h, mx = int(tok[1]), int(tok[2]), int(tok[3])
px = list(map(int, tok[4:4 + w*h*3]))
assert len(px) == w*h*3, "PPM truncado"
mn, mxv = min(px), max(px)
mean = sum(px) / len(px)
acc = n = 0
for y in range(max(0, cy-32), min(h, cy+32)):
    for x in range(max(0, cx-32), min(w, cx+32)):
        i = (y*w + x) * 3
        acc += px[i] + px[i+1] + px[i+2]; n += 3
print(f"{mean:.4f} {acc/n:.4f} {mn} {mxv}")
PY
}

# 1) Renders requeridos (regenerar SIEMPRE: nunca confiar en .ppm rancios)
timeout 300 ./rt fog 0 0      </dev/null >/dev/null 2>&1 && cp image.ppm image-fog-000.ppm      || fail "'./rt fog 0 0' falló"
timeout 300 ./rt fog 0.02 0   </dev/null >/dev/null 2>&1 && cp image.ppm image-fog-a02.ppm      || fail "'./rt fog 0.02 0' falló"
timeout 300 ./rt fog 0 0.02   </dev/null >/dev/null 2>&1 && cp image.ppm image-fog-s02.ppm      || fail "'./rt fog 0 0.02' falló"
timeout 300 ./rt fog 0.01 0.02 </dev/null >/dev/null 2>&1 && cp image.ppm image-fog-a01-s02.ppm || fail "'./rt fog 0.01 0.02' falló"
ok "los 4 renders fog se generan"

# 2) Medio nulo == fase 2 (modo point), bit a bit
timeout 300 ./rt point </dev/null >/dev/null 2>&1 || fail "'./rt point' (fase 2) falló"
cmp -s image.ppm image-fog-000.ppm || fail "fog 0 0 NO reproduce el modo point bit a bit (el medio nulo altera la imagen)"
ok "medio nulo: fog 0 0 == point (bit a bit)"

# 3) Rango sano (sin negativos/NaN el parser fallaría; validar 0..255)
read -r M000 P000 MN000 MX000 < <(ppm_stats image-fog-000.ppm 512 200)
read -r MA02 PA02 MNA MXA     < <(ppm_stats image-fog-a02.ppm 512 200)
read -r MS02 PS02 MNS MXS     < <(ppm_stats image-fog-s02.ppm 512 200)
for v in "$MN000" "$MNA" "$MNS"; do [ "$v" -ge 0 ] || fail "píxel negativo"; done
for v in "$MX000" "$MXA" "$MXS"; do [ "$v" -le 255 ] || fail "píxel > 255"; done
ok "rango 0-255 sano en los 3 renders base"

# 4) Absorción monótona (estricta): media global a02 < 000
python3 -c "import sys; sys.exit(0 if float('$MA02') < float('$M000') else 1)" \
  || fail "absorción no atenúa: media fog-a02 ($MA02) >= fog-000 ($M000)"
ok "absorción atenúa: media $MA02 < $M000"

# 5) Glow de in-scattering: el CONTRASTE del parche de la fuente contra la media
# global debe crecer con la dispersión (la extinción oscurece TODO en absoluto, pero
# el in-scattering concentra energía relativa alrededor de la fuente).
python3 -c "
import sys
r000 = float('$P000') / max(float('$M000'), 1e-9)
rs02 = float('$PS02') / max(float('$MS02'), 1e-9)
sys.exit(0 if rs02 > r000 * 1.5 else 1)
" || fail "sin glow de in-scattering: contraste parche/global de s02 ($PS02/$MS02) no supera 1.5x el de 000 ($P000/$M000)"
ok "in-scattering crea glow: contraste s02 = $(python3 -c "print(f'{float('$PS02')/float('$MS02'):.2f}')")x vs 000 = $(python3 -c "print(f'{float('$P000')/float('$M000'):.2f}')")x"

# 6) Determinismo OpenMP en el modo compuesto
timeout 300 ./rt fog 0.01 0.02 </dev/null >/dev/null 2>&1 && cp image.ppm /tmp/fog-nt.ppm
OMP_NUM_THREADS=1 timeout 600 ./rt fog 0.01 0.02 </dev/null >/dev/null 2>&1
cmp -s image.ppm /tmp/fog-nt.ppm || fail "no determinista con OMP_NUM_THREADS=1"
rm -f /tmp/fog-nt.ppm
ok "determinismo OpenMP (1 vs N hilos)"

echo "✓ check phase-06: los 6 criterios en verde"
