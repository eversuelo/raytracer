---
type: spec
project: aitl-raytracer
fase: 5
title: Phase 05 — Proyecto final: MIS (β=2) y Path Tracing explícito
base: ../phase-04 parte 1 (resultado aceptado)
enunciado: ./README.md
status: approved
---

# Phase 05 — Proyecto final: MIS y Path Tracing explícito

## Contexto

Proyecto final del curso (`README.md` de esta carpeta manda). El enunciado ofrece dos
opciones; **para el experimento de la tesis se implementan AMBAS**, como dos corridas
separadas de la misma fase (5a y 5b), porque juntas cierran el arco "raytracer normal →
path tracing" y cada una ejercita habilidades distintas del agente. Ambas parten de la
estructura del Proyecto 4 parte 1.

## Sub-fase 5a — Muestreo de importancia múltiple (Opción 1)

**RF-5a.1** Combinar muestreo de luz y muestreo de BRDF con la **heurística de potencia
β=2**: `w(p_a, p_b) = p_a² / (p_a² + p_b²)`. Por fuente luminosa, seguir el pseudocódigo
del enunciado: una muestra de luz ponderada por `w(pl(wl), pb(wl))` + una muestra de BRDF
(si alcanza un emisor) ponderada por `w(pb(wb), pl(wb))`. Requiere la API sample/eval/pdf
de la phase-04 para ambos lados.

**Criterios 5a**: (1) las imágenes convergen a lo MISMO que el proyecto 4 (diferencia RMS
a 2048 spp comparable al ruido); (2) a igual spp la **varianza se reduce** vs. cualquiera
de los dos muestreos solos — medir en dos parches: reflejo metálico y sombra suave;
(3) los pesos suman ≤ 1 y no aparecen fireflies nuevos.

## Sub-fase 5b — Path Tracing explícito sesgado (Opción 2)

**RF-5b.1** Recursión `shade(ray, bounce)` según el pseudocódigo del enunciado:
- rayo escapa → 0; rayo toca emisor → su radiancia solo si `bounce == 1`, si no 0
  (la luz directa de rebotes posteriores ya la cuenta la NEE — sesgado por diseño).
- En cada rebote: `Ld` por muestreo de luz con ángulo sólido (phase-03) + rebote
  indirecto muestreando la BRDF: `Lind = fr·(cos/pdf)·shade(nuevo_rayo, bounce+1)`.

**RF-5b.2** Longitud máxima de camino: **B = 10 vértices** ("10b"; el rayo de cámara
entra con `bounce = 1` y en `bounce == B` se devuelve solo Ld; sin ruleta rusa).
Escenas de referencia (caja y metales de la phase-04; solo cambian luces y el α del
aluminio):
- **AreaL**: una esfera emisora r=10.5 en (0, 24.3, 0), Le=(10,10,10); metales α=0.3.
- **PointL**: una puntual r=0 en (0, 24.3, 0) con **I=(4000,4000,4000)** — ojo: 4000
  como en la phase-02, NO el 2000 de la escena 2A1P; metales α=0.3.
- **MultiL**: la escena 2A1P (puntual I=2000 + emisoras (12,5,5) y (5,5,12)) con el
  **aluminio en α = 0.03** (el oro queda en 0.3).

**Criterios 5b**: (1) renders coinciden con `image-PTEXP-{PointL,AreaL,MultiL}-10b-{32,512}`;
(2) aparece iluminación global visible (color bleeding de las paredes en las esferas,
brillo indirecto en sombras) ausente en fases anteriores; (3) el costo crece de forma
acotada con los rebotes (medir tiempo B=1 vs B=10); (4) reproducibilidad por semilla;
(5) **cero píxeles NaN/negativos**: con α=0.03 las pdf del conductor pueden desbordar
o anularse — se exigen guardas (término descartado ante pdf no finita o ≈0, peso MIS
w=0 ante 0/0). El código publicado del curso carece de esas guardas y produce 10.36 %
de píxeles NaN en MultiL; su referencia se generó con guardas — la referencia manda.

## No-objetivos

Ruleta rusa, dieléctricos, mapas de fotones — fuera del alcance del curso base.

## Prompt de tarea (idéntico para C0 y C2; 5a y 5b se implementan en orden en la misma corrida)

**5a (MIS):**

> Partiendo del trazador de la fase 4 (escena 2A1P: puntual r=0 en (0,24.3,0) con
> I=(2000,2000,2000), esfera emisora r=10.5 en (−23,24.3,0) con Le=(12,5,5), esfera
> emisora r=5 en (23,24.3,−50) con Le=(5,5,12); aluminio η=(1.66058,0.88143,0.521467),
> κ=(9.2282,6.27077,4.83803) y oro η=(0.143245,0.377423,1.43919),
> κ=(3.98479,2.3847,1.60434), ambos α=0.3), implementa MIS para la iluminación directa
> con la heurística de potencia β=2: `w(p_a,p_b) = p_a²/(p_a²+p_b²)`. Por cada fuente
> de ÁREA acumula exactamente dos términos: (1) una muestra de luz wl por ángulo
> sólido — cono con θmax = asin(r_luz/|c_luz−x|), θ = acos(1−ξ₁+ξ₁·cosθmax),
> φ = 2π·ξ₂, pdf_luz = 1/(2π·(1−cosθmax)) — con contribución Ll = fr·Le·cosθ/pdf_luz
> si el rayo de sombra alcanza esa fuente, ponderada por w(pl(wl), pb(wl)); y (2) una
> muestra de BRDF wb — difuso: coseno-hemisférico θ = acos(√(1−ξ)), pdf = cosθ/π;
> conductor: half-vector Beckmann θh = atan(√(−α²·ln(1−ξ))), wi = 2(wo·wh)wh − wo,
> pdf = D(wh)·cosθh/(4|wo·wh|) — con contribución Lb = fr·Le·cosθ/pb(wb) SOLO si el
> rayo intersecta esa misma fuente, ponderada por w(pb(wb), pl(wb)). Ojo: las dos pdf
> de cada peso se evalúan sobre la MISMA dirección y en el MISMO marco de referencia
> (global o local consistente), y si ambas pdf son 0 o no finitas el término se
> descarta (w=0), nunca 0/0. Las puntuales conservan su término determinista
> fr·(I/d²)·cosθ con rayo de sombra, sin peso MIS. La imagen convergida NO debe
> cambiar respecto a la fase 4; a igual spp la varianza baja: mídela en dos parches de
> 32×32 —uno sobre el reflejo del aluminio, otro en una sombra suave— contra las dos
> partes de la fase 4 (solo luz / solo BRDF). Los pesos cumplen 0 ≤ w ≤ 1 y no deben
> aparecer fireflies nuevos. RNG determinista por renglón (erand48 con semilla fija
> derivada del índice del renglón): misma corrida → mismo PPM bit a bit, con 1 o N hilos.

**5b (Path Tracing explícito):**

> Partiendo del resultado de 5a, implementa Path Tracing explícito sesgado con
> `shade(ray, bounce)` (el rayo de cámara entra con bounce=1): si el rayo escapa →
> negro; si toca un emisor → devuelve su Le solo si bounce==1, si no devuelve 0 (la
> luz directa de vértices posteriores ya la cuenta la NEE — sesgado por diseño; SIN
> ruleta rusa). En cada vértice x: Ld = iluminación directa con el estimador MIS de 5a
> (fuentes de área con β=2; puntuales deterministas); luego una muestra de BRDF wr con
> pdf pr (fórmulas de 5a) genera el rebote indirecto
> `Lind = fr·(cosθr/pr)·shade(Ray(x,wr), bounce+1)` y devuelve Ld+Lind; si bounce==B
> (longitud máxima, default 10) devuelve solo Ld — con B=10 el camino tiene hasta 10
> vértices, con NEE en todos. CLI: `./rt ptexp <spp> <escena> [B]` con
> escena ∈ {areal, point, multil}, escribiendo `image.ppm`. La caja no cambia (paredes
> r=1e5: izq (−1e5−49,0,0) albedo (.75,.25,.25); der (1e5+49,0,0) (.25,.25,.75); fondo
> (0,0,−1e5−81.6) (.25,.75,.25); suelo (0,−1e5−40.8,0) (.25,.75,.75); techo
> (0,1e5+40.8,0) (.75,.75,.25); esferas r=16.5 aluminio en (−23,−24.3,−34.6) y oro en
> (23,−24.3,−3.6), η/κ de 5a). Las tres escenas SOLO cambian luces y α del aluminio:
> **areal** = una esfera emisora r=10.5 en (0,24.3,0) con Le=(10,10,10), ambos metales
> α=0.3; **point** = una puntual r=0 en (0,24.3,0) con I=(4000,4000,4000)
> (contribución fr·(I/d²)·cosθ; nunca visible a cámara), metales α=0.3; **multil** =
> la escena 2A1P de 5a (puntual I=2000 + emisoras (12,5,5) y (5,5,12)) pero con el
> ALUMINIO en α=0.03 (el oro queda en 0.3). Con α=0.03 las pdf del conductor pueden
> desbordar o anularse: descarta contribuciones con pdf no finita o ≈0 y aplica la
> regla w=0 de 5a — CERO píxeles NaN o negativos en el PPM. Cámara fija: origen
> (0,11.2,214), dirección (0,−0.042612,−1) normalizada, 1024×768,
> cx=(w·0.5095/h,0,0), cy=(cx×d̂)·0.5095, jitter uniforme ±0.5 píxel; promedio de spp,
> clamp a [0,1] y gamma 1/2.2 (`int(pow(v,1/2.2)*255+.5)`); salida PPM P3. Genera los
> seis renders `image-PTEXP-<Escena>-10b-<spp>.ppm` (Escena ∈ {AreaL, PointL, MultiL},
> spp ∈ {32,512}) copiando `image.ppm`. El render es estocástico — NO compares píxel a
> píxel: usa `python3 sdd/tools/compare-ref.py <render.ppm>
> sdd/phase-05/image-PTEXP-<Escena>-10b-<spp>.jpg --down 64x48 --mean 3.0 --maxd 40`
> (correcto ≈0.6–1.0; escena o fórmula equivocada ≥12). Verifica además: color
> bleeding (paredes reflejadas en las esferas, sombras no negras — ausente en fase 4),
> emisores visibles solo en areal/multil, costo acotado al crecer B (mide B=1 vs
> B=10), y reproducibilidad: misma semilla → PPM idéntico bit a bit con 1 o N hilos.
> Antes de dar por terminado, `make && bash sdd/phase-05/check.sh` debe quedar verde.

## Entregables

`rt.cpp` final; renders 5a (comparativa de varianza) y 5b (`image-PTEXP-*.ppm`);
tabla de varianza/tiempos; `run_id` C0/C2 por sub-fase + `run-show` según
`TRAZABILIDAD.md`. Cierre del experimento: ADR final del proyecto con el veredicto
global C0 vs C2.
