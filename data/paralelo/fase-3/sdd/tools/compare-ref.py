#!/usr/bin/env python3
"""Comparador de renders contra la referencia del curso (gates de phases 01-05).

Uso:
  python3 sdd/tools/compare-ref.py <render.ppm> <referencia.jpg> [opciones]

Modos:
  Determinista (default) — comparación a resolución completa:
    --mean M   media de |Δ| máxima por canal (default 2.0)
    --thr T    umbral por píxel (default 12)
    --pct P    % máximo de píxeles con |Δ| > thr en algún canal (default 2.0)

  Monte Carlo — promedia el ruido antes de comparar:
    --down WxH  reduce ambas imágenes con filtro BOX (p. ej. 64x48) y compara ahí:
    --mean M    media de |Δ| máxima por canal sobre la reducida
    --maxd X    |Δ| máximo permitido en la reducida

  Adicional (cualquier modo):
    --gmean R,G,B --gmtol T   la media global RGB del RENDER debe estar a ±T de
                              (R,G,B) — discrimina sampler/fórmula a bajo spp.
    --size WxH (default 1024x768) · --label texto

Tolerancias por fase calibradas midiendo la solución del profesor (objective/)
contra su propia referencia JPG; ver el check.sh de cada fase.
Salida estilo probe.py: ✓/✗ y exit 0/1.
"""
import argparse
import sys

from PIL import Image, ImageChops, ImageStat


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("render")
    ap.add_argument("referencia")
    ap.add_argument("--mean", type=float, default=2.0)
    ap.add_argument("--thr", type=int, default=12)
    ap.add_argument("--pct", type=float, default=2.0)
    ap.add_argument("--down", default=None, help="WxH: comparar en versión reducida (BOX)")
    ap.add_argument("--maxd", type=float, default=None, help="máx |Δ| permitido (modo --down)")
    ap.add_argument("--gmean", default=None, help="R,G,B esperado de la media global del render")
    ap.add_argument("--gmtol", type=float, default=4.0)
    ap.add_argument("--size", default="1024x768")
    ap.add_argument("--label", default="")
    a = ap.parse_args()
    tag = f" [{a.label}]" if a.label else ""

    def load(path, nombre):
        try:
            return Image.open(path).convert("RGB")
        except Exception as e:
            print(f"✗ compare-ref{tag}: no pude leer {nombre} {path}: {e}", file=sys.stderr)
            sys.exit(1)

    img = load(a.render, "el render")
    ref = load(a.referencia, "la referencia")

    w, h = (int(v) for v in a.size.lower().split("x"))
    if img.size != (w, h):
        print(f"✗ compare-ref{tag}: render {img.size[0]}x{img.size[1]}, la spec exige {w}x{h}",
              file=sys.stderr)
        return 1
    if ref.size != (w, h):
        print(f"✗ compare-ref{tag}: referencia {ref.size[0]}x{ref.size[1]} ≠ {w}x{h} (¿archivo equivocado?)",
              file=sys.stderr)
        return 1

    ok = True

    # Media global del render (independiente de la referencia)
    if a.gmean:
        exp = [float(v) for v in a.gmean.split(",")]
        got = ImageStat.Stat(img).mean
        if any(abs(g - e) > a.gmtol for g, e in zip(got, exp)):
            print(f"✗ compare-ref{tag}: media global del render = "
                  f"({got[0]:.1f}, {got[1]:.1f}, {got[2]:.1f}), esperada "
                  f"({exp[0]:.1f}, {exp[1]:.1f}, {exp[2]:.1f}) ± {a.gmtol}", file=sys.stderr)
            ok = False

    if a.down:
        dw, dh = (int(v) for v in a.down.lower().split("x"))
        di = img.resize((dw, dh), Image.BOX)
        dr = ref.resize((dw, dh), Image.BOX)
        st = ImageStat.Stat(ImageChops.difference(di, dr))
        m = max(st.mean)
        x = max(e[1] for e in st.extrema)
        if m > a.mean:
            print(f"✗ compare-ref{tag}: media |Δ| (reducida {a.down}) = {m:.2f} "
                  f"— máximo permitido {a.mean:.2f}", file=sys.stderr)
            ok = False
        if a.maxd is not None and x > a.maxd:
            print(f"✗ compare-ref{tag}: máx |Δ| (reducida {a.down}) = {x:.0f} "
                  f"— máximo permitido {a.maxd:.0f}", file=sys.stderr)
            ok = False
        if ok:
            print(f"✓ compare-ref{tag}: reducida {a.down} media |Δ| = {m:.2f}, "
                  f"máx = {x:.0f} — dentro de tolerancia")
        return 0 if ok else 1

    # Modo determinista: resolución completa
    diff = ImageChops.difference(img, ref)
    n = w * h
    hist = diff.histogram()
    means = [sum(v * k for k, v in enumerate(hist[c * 256:(c + 1) * 256])) / n for c in range(3)]
    bands = [ch.point(lambda v: 255 if v > a.thr else 0) for ch in diff.split()]
    mask = ImageChops.lighter(ImageChops.lighter(bands[0], bands[1]), bands[2])
    pct = 100.0 * (mask.histogram()[255] / n)

    if any(m > a.mean for m in means):
        print(f"✗ compare-ref{tag}: media |Δ| RGB = ({means[0]:.2f}, {means[1]:.2f}, "
              f"{means[2]:.2f}) — máximo permitido {a.mean:.2f} por canal", file=sys.stderr)
        ok = False
    if pct > a.pct:
        print(f"✗ compare-ref{tag}: {pct:.2f}% de píxeles con |Δ| > {a.thr} "
              f"(máximo {a.pct:.2f}%)", file=sys.stderr)
        ok = False
    if ok:
        print(f"✓ compare-ref{tag}: media |Δ| = ({means[0]:.2f}, {means[1]:.2f}, "
              f"{means[2]:.2f}), {pct:.2f}% > {a.thr} — dentro de tolerancia")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
