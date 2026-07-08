#!/usr/bin/env python3
"""Sondeo de píxeles para el gate de Phase 00 (criterios 2 y 3 de spec.md).

Uso: python3 sdd/phase-00/probe.py <image-normal.ppm> <image-distance.ppm>

El modo normales está totalmente determinado por la spec (escena y cámara fijas,
color = obj.c + n como en los renders de referencia del curso, gamma 2.2), así que
el valor esperado de cada sonda se calcula aquí de forma analítica con la misma
aritmética doble que rt.cpp. Tolerancia: ±10/255 por canal tras gamma (criterio 2).

El modo distancia deja el mapeo al implementador (RF-3), así que se verifica solo
lo que la spec exige: gris (r==g==b), gradiente real (no binario), monotonía en t
y siluetas de las tres esferas distinguibles contra la pared trasera (criterio 3).

`--map` imprime la tabla analítica de sondas (uso del investigador, no del gate).
"""
import math
import sys

W, H = 1024, 768
TOL = 10          # criterio 2: ±10/255 por canal
SIL_MIN = 20      # criterio 3: contraste mínimo esfera vs pared trasera
GRADIENT_MIN = 16 # criterio 3: niveles de gris distintos (no saturado a 2 tonos)

# Escena de la spec (radio, centro, color) — índices como en spheres[] de rt.cpp
SPH = [
    (1e5, (-1e5 - 49, 0, 0), (.75, .25, .25)),      # 0 pared izquierda
    (1e5, (1e5 + 49, 0, 0), (.25, .25, .75)),       # 1 pared derecha
    (1e5, (0, 0, -1e5 - 81.6), (.75, .75, .75)),    # 2 pared trasera
    (1e5, (0, -1e5 - 40.8, 0), (.75, .75, .75)),    # 3 suelo
    (1e5, (0, 1e5 + 40.8, 0), (.75, .75, .75)),     # 4 techo
    (16.5, (-23, -24.3, -34.6), (.999, .999, .999)),  # 5 esfera abajo-izq
    (16.5, (23, -24.3, -3.6), (.999, .999, .999)),    # 6 esfera abajo-der
    (10.5, (0, 24.3, 0), (1.0, 1.0, 1.0)),            # 7 esfera arriba
]
NAMES = ["pared izquierda", "pared derecha", "pared trasera", "suelo", "techo",
         "esfera abajo-izq", "esfera abajo-der", "esfera arriba"]


def norm(v):
    l = math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2])
    return (v[0] / l, v[1] / l, v[2] / l)


CAM_O = (0.0, 11.2, 214.0)
CAM_D = norm((0.0, -0.042612, -1.0))
CX = (W * 0.5095 / H, 0.0, 0.0)
_cross = (CX[1] * CAM_D[2] - CX[2] * CAM_D[1],
          CX[2] * CAM_D[0] - CX[0] * CAM_D[2],
          CX[0] * CAM_D[1] - CX[1] * CAM_D[0])
CY = tuple(c * 0.5095 for c in norm(_cross))


def ray_dir(px, py):
    """px = columna, py = renglón del archivo (0 = arriba); como en rt.cpp."""
    y = H - 1 - py
    d = tuple(CX[i] * (px / W - 0.5) + CY[i] * (y / H - 0.5) + CAM_D[i]
              for i in range(3))
    return norm(d)


def hit(d):
    tmin, imin = 1e20, -1
    for i, (r, p, _c) in enumerate(SPH):
        oc = (CAM_O[0] - p[0], CAM_O[1] - p[1], CAM_O[2] - p[2])
        b = oc[0] * d[0] + oc[1] * d[1] + oc[2] * d[2]
        det = b * b - (oc[0] ** 2 + oc[1] ** 2 + oc[2] ** 2 - r * r)
        if det < 0:
            continue
        s = math.sqrt(det)
        for t in (-b - s, -b + s):
            if 1e-4 < t < tmin:
                tmin, imin = t, i
                break
    return (tmin, imin)


def disp(x):
    x = 0.0 if x < 0 else (1.0 if x > 1 else x)
    return int(x ** (1 / 2.2) * 255 + 0.5)


def expected_normal(px, py):
    d = ray_dir(px, py)
    t, i = hit(d)
    x = tuple(CAM_O[j] + d[j] * t for j in range(3))
    r, p, c = SPH[i]
    n = norm((x[0] - p[0], x[1] - p[1], x[2] - p[2]))
    rgb = tuple(disp(c[j] + n[j]) for j in range(3))
    return rgb, t, i


# Sondas: (px, py, id de esfera que el modelo analítico DEBE golpear ahí).
# Elegidas lejos de todo borde de silueta (ver imágenes de referencia).
WALL_PROBES = [(60, 384, 0), (964, 384, 1), (512, 384, 2), (512, 730, 3), (512, 60, 4)]
SPHERE_PROBES = [
    (372, 535, 5), (340, 535, 5), (372, 570, 5),   # abajo-izq: centro, izq, abajo
    (671, 565, 6), (630, 565, 6), (671, 600, 6),   # abajo-der
    (512, 225, 7), (487, 225, 7), (512, 250, 7),   # arriba
]
# Pared trasera adyacente a cada esfera (contraste de silueta, criterio 3)
BACKWALL_NEAR = {5: (372, 400), 6: (671, 410), 7: (650, 225)}
# Monotonía en t: sondas con t bien separadas (izq. cercana → trasera lejana)
MONO_PROBES = [(60, 384), (512, 225), (372, 535), (512, 384)]


def read_ppm(path):
    with open(path, "rb") as f:
        raw = f.read().decode("ascii", errors="replace")
    tokens = " ".join(l.split("#")[0] for l in raw.splitlines()).split()
    if not tokens or tokens[0] != "P3":
        return None, f"{path}: no es PPM P3 (ASCII)"
    try:
        w, h, mx = int(tokens[1]), int(tokens[2]), int(tokens[3])
        vals = [int(v) for v in tokens[4:]]
    except (ValueError, IndexError):
        return None, f"{path}: cabecera o píxeles ilegibles"
    if (w, h) != (W, H):
        return None, f"{path}: resolución {w}x{h}, la spec exige {W}x{H}"
    if mx != 255:
        return None, f"{path}: maxval {mx}, la spec exige 255"
    if len(vals) != W * H * 3:
        return None, f"{path}: {len(vals)} valores, se esperaban {W * H * 3}"
    return vals, None


def px(vals, x, y):
    i = (y * W + x) * 3
    return (vals[i], vals[i + 1], vals[i + 2])


def main():
    if "--map" in sys.argv:
        for (x, y, i) in WALL_PROBES + SPHERE_PROBES:
            rgb, t, ihit = expected_normal(x, y)
            mark = "" if ihit == i else f"  ⚠ esperaba id {i}"
            print(f"({x:4},{y:3}) -> {NAMES[ihit]:17} t={t:7.2f} rgb={rgb}{mark}")
        for x, y in MONO_PROBES + list(BACKWALL_NEAR.values()):
            _, t, ihit = expected_normal(x, y)
            print(f"({x:4},{y:3}) -> {NAMES[ihit]:17} t={t:7.2f}")
        return 0

    if len(sys.argv) != 3:
        print("uso: probe.py <image-normal.ppm> <image-distance.ppm>", file=sys.stderr)
        return 2
    errors = []

    # Autoconsistencia del modelo analítico: cada sonda golpea la esfera prevista
    for (x, y, i) in WALL_PROBES + SPHERE_PROBES:
        _, _, ihit = expected_normal(x, y)
        assert ihit == i, f"modelo analítico roto: sonda ({x},{y}) golpea id {ihit}, esperaba {i}"

    # ── Criterio 2: modo normales ────────────────────────────────────────────
    nvals, err = read_ppm(sys.argv[1])
    if err:
        errors.append(f"criterio 2: {err}")
    else:
        for (x, y, i) in WALL_PROBES + SPHERE_PROBES:
            exp, _, _ = expected_normal(x, y)
            got = px(nvals, x, y)
            if any(abs(g - e) > TOL for g, e in zip(got, exp)):
                errors.append(f"criterio 2: {NAMES[i]} en ({x},{y}): "
                              f"esperado ±{TOL} {exp}, obtenido {got}")

    # ── Criterio 3: modo distancia ───────────────────────────────────────────
    dvals, err = read_ppm(sys.argv[2])
    if err:
        errors.append(f"criterio 3: {err}")
    else:
        grays = set()
        gray_ok = True
        for y in range(0, H, 24):
            for x in range(0, W, 24):
                r, g, b = px(dvals, x, y)
                if r != g or g != b:
                    errors.append(f"criterio 3: píxel ({x},{y}) no es gris: {(r, g, b)}")
                    gray_ok = False
                    break
                grays.add(r)
            if not gray_ok:
                break
        if gray_ok:
            if len(grays) < GRADIENT_MIN:
                errors.append(f"criterio 3: solo {len(grays)} niveles de gris "
                              f"(mínimo {GRADIENT_MIN}) — imagen saturada o plana")
            for i, (bx, by) in BACKWALL_NEAR.items():
                gs = px(dvals, *SPHERE_PROBES[[5, 6, 7].index(i) * 3][:2])[0]
                gb = px(dvals, bx, by)[0]
                if abs(gs - gb) < SIL_MIN:
                    errors.append(f"criterio 3: {NAMES[i]} no se distingue de la pared "
                                  f"trasera (gris {gs} vs {gb}, mínimo Δ{SIL_MIN})")
            seq = [px(dvals, x, y)[0] for x, y in MONO_PROBES]
            if not (all(a <= b for a, b in zip(seq, seq[1:])) or
                    all(a >= b for a, b in zip(seq, seq[1:]))):
                errors.append(f"criterio 3: el gris no es monótono con la distancia: "
                              f"{seq} en sondas con t creciente")
            elif abs(seq[0] - seq[-1]) < SIL_MIN:
                errors.append(f"criterio 3: rango de grises insuficiente entre la sonda "
                              f"más cercana y la más lejana ({seq[0]} vs {seq[-1]})")

    if errors:
        for e in errors:
            print(f"✗ {e}", file=sys.stderr)
        return 1
    print("✓ probe.py: criterios 2 y 3 en verde (14 sondas de color, gris monótono, siluetas)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
