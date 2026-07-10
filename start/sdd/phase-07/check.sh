#!/usr/bin/env bash
# Gate objetivo de Phase 07 — path tracing volumétrico con fuentes de área (Temas 6+7).
# Verificación ANALÍTICA: medio nulo bit a bit, extinción monótona, glow volumétrico
# en la banda de luces, glow CROMÁTICO (R/B por mitades) y reproducibilidad.
set -euo pipefail

fail() { echo "✗ check phase-07: $*" >&2; exit 1; }
ok()   { echo "✓ $*"; }
command -v python3 >/dev/null || fail "falta python3"
[ -x ./rt ] || fail "no existe ./rt (corre make)"

# Estadísticos: media global, media banda y∈[100,300), medias R y B por mitades de la banda
ppm_stats() { # $1=archivo → "global banda Rizq Bizq Rder Bder min max"
  python3 - "$1" <<'PY'
import sys
tok = open(sys.argv[1]).read().split()
assert tok[0] == "P3"
w, h = int(tok[1]), int(tok[2])
px = list(map(int, tok[4:4 + w*h*3]))
assert len(px) == w*h*3, "PPM truncado"
g = sum(px) / len(px)
acc = n = 0; ri = bi = rd = bd = 0; ni = nd = 0
for y in range(100, min(h, 300)):
    base = y * w
    for x in range(w):
        i = (base + x) * 3
        acc += px[i] + px[i+1] + px[i+2]; n += 3
        if x < w // 2: ri += px[i]; bi += px[i+2]; ni += 1
        else:          rd += px[i]; bd += px[i+2]; nd += 1
print(f"{g:.4f} {acc/n:.4f} {ri/ni:.4f} {bi/ni:.4f} {rd/nd:.4f} {bd/nd:.4f} {min(px)} {max(px)}")
PY
}

run() { timeout 900 ./rt "$@" </dev/null >/dev/null 2>&1 || fail "'./rt $*' falló"; }

# 1) Renders (regenerar siempre)
run fogpt 64 multil 0 0;        cp image.ppm image-fogpt-multil-000.ppm
run fogpt 64 multil 0 0.015;    cp image.ppm image-fogpt-multil-s.ppm
run fogpt 64 multil 0.008 0.015; cp image.ppm image-fogpt-multil-as.ppm
run fogpt 64 areal 0 0.015;     cp image.ppm image-fogpt-areal-s.ppm
ok "los 4 renders fogpt se generan"

# 2) Medio nulo == ptexp bit a bit
run ptexp 64 multil
cmp -s image.ppm image-fogpt-multil-000.ppm || fail "fogpt 0 0 NO reproduce ptexp bit a bit"
ok "medio nulo: fogpt 64 multil 0 0 == ptexp 64 multil"

read -r G000 B000 _ _ _ _ MN0 MX0 < <(ppm_stats image-fogpt-multil-000.ppm)
read -r GS   BS   RIS BIS RDS BDS MN1 MX1 < <(ppm_stats image-fogpt-multil-s.ppm)
read -r GAS  BAS  _ _ _ _ MN2 MX2 < <(ppm_stats image-fogpt-multil-as.ppm)

# 3) Rango sano
for v in "$MN0" "$MN1" "$MN2"; do [ "$v" -ge 0 ] || fail "píxel negativo"; done
for v in "$MX0" "$MX1" "$MX2"; do [ "$v" -le 255 ] || fail "píxel > 255"; done
ok "rango 0-255 sano"

# 4) Extinción monótona: as < s < 000
python3 -c "import sys; sys.exit(0 if float('$GAS') < float('$GS') < float('$G000') else 1)" \
  || fail "extinción no monótona: as=$GAS s=$GS 000=$G000"
ok "extinción monótona: $GAS < $GS < $G000"

# 5) Glow volumétrico: contraste banda/global crece >= 1.25x
python3 -c "
import sys
r0 = float('$B000') / max(float('$G000'), 1e-9)
rs = float('$BS') / max(float('$GS'), 1e-9)
sys.exit(0 if rs > r0 * 1.25 else 1)
" || fail "sin glow volumétrico: contraste banda s=$BS/$GS vs 000=$B000/$G000"
ok "glow volumétrico: contraste banda crece ($BS/$GS vs $B000/$G000)"

# 6) Glow cromático: R>B a la izquierda, B>R a la derecha (multil-s)
python3 -c "import sys; sys.exit(0 if float('$RIS') > float('$BIS') and float('$BDS') > float('$RDS') else 1)" \
  || fail "glow no cromático: izq R=$RIS B=$BIS · der R=$RDS B=$BDS"
ok "glow cromático: izq R=$RIS>B=$BIS · der B=$BDS>R=$RDS"

# 7) Reproducibilidad (semilla y OMP)
run fogpt 64 multil 0.008 0.015; cp image.ppm /tmp/fogpt-nt.ppm
OMP_NUM_THREADS=1 timeout 1800 ./rt fogpt 64 multil 0.008 0.015 </dev/null >/dev/null 2>&1
cmp -s image.ppm /tmp/fogpt-nt.ppm || fail "no reproducible (1 vs N hilos)"
rm -f /tmp/fogpt-nt.ppm
ok "reproducible bit a bit (1 vs N hilos)"

echo "✓ check phase-07: los 7 criterios en verde"
