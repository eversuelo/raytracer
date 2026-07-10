---
type: spec
project: aitl-raytracer
fase: 7
title: Phase 07 — Path tracing volumétrico: niebla + fuentes de área (Temas 6+7)
base: resultado aceptado de phase-05 (path tracing + MIS)
enunciado: state-of-art/Tema 7 - Medios participantes.pdf (+ Tema 6)
status: approved
---

# Phase 07 — Niebla sobre el path tracer con fuentes de área (estado del arte)

## Contexto

Culminación del estado del arte del laboratorio: combinar el **path tracer con MIS de
la fase 5** con el **medio participante de la fase 6**, ahora con in-scattering desde
las **fuentes de ÁREA** (no solo la puntual). La escena estrella es `multil` (puntual
I=2000 + emisora roja (12,5,5) + emisora azul (5,5,12) + metales): sumergida en niebla
produce **god rays y glows de colores** — el efecto de las diapositivas 2-3 del Tema 7
con múltiples fuentes cromáticas.

## Teoría aplicable

- Modelo de la diapositiva 17-19 del Tema 7: `L = Tr(s)·Lo(xs) + L_medio`, donde `Lo`
  es la radiancia de superficie COMPLETA del path tracer de la fase 5 (todas sus
  rebotes; solo el segmento primario se atenúa) y `L_medio` es la radiancia
  in-scattered acumulada por ray marching sobre el rayo primario.
- In-scattering por fuente, en cada punto de marcha x_t (función de fase isotrópica
  `1/(4π)`, transmitancia homogénea):
  - **Puntual** (r=0): término determinista `σs·p·I·Tr(d)/d²` con rayo de sombra.
  - **Esféricas (área)**: UNA muestra uniforme de la esfera por punto de marcha
    (muestreo de la fase 3: x′ = c + r·ω, pdf_A = 1/(4πr²)), contribución
    `σs·p·Le·cosθ′·Tr(d)/(pdf_A·d²)` con visibilidad del rayo de sombra
    (x_t → x′) y cosθ′ = n_luz·(dir x′→x_t); muestra 0 si cosθ′ ≤ 0 u ocluida.
- El RNG de las muestras de área en el medio usa el MISMO estado erand48 por píxel de
  la fase 5 (reproducible por semilla, 1 o N hilos).

## Prompt de tarea

> Partiendo del `rt.cpp` actual (resultado de la fase 5: path tracer `ptexp` con NEE y
> MIS, escenas {areal, point, multil}), añade un MEDIO PARTICIPANTE HOMOGÉNEO sobre el
> path tracer, con in-scattering desde TODAS las fuentes (puntual Y esféricas de área),
> según el Tema 7:
> (1) Nuevo modo CLI: `./rt fogpt <spp> <escena> <sigma_a> <sigma_s>` con escena ∈
> {areal, point, multil}; σt = σa + σs; Tr(s) = exp(−σt·s).
> (2) Por píxel: `L = Tr(s)·Lo + L_medio`, con `Lo` la radiancia del path tracer de la
> fase 5 (idéntica, con todos sus rebotes y MIS; solo el segmento primario de longitud
> s se atenúa con Tr(s)) y `L_medio` el ray marching del rayo primario con paso
> Δ = 2.0: para t = Δ/2, 3Δ/2, … < s suma, POR CADA fuente de la escena:
> — puntual r=0: `exp(−σt·t)·σs·(1/4π)·I·exp(−σt·d)·Δ/d²` con visibilidad;
> — esférica emisora: UNA muestra uniforme de su superficie con el estado erand48 del
> píxel (θ=acos(1−2ξ₁), φ=2πξ₂; pdf_A = 1/(4πr²)), contribución
> `exp(−σt·t)·σs·(1/4π)·Le·cosθ′·exp(−σt·d)·Δ/(pdf_A·d²)` con d=|x′−x_t|, cosθ′ =
> n_luz·(dir x′→x_t); si cosθ′≤0 o el rayo de sombra x_t→x′ está ocluido, la muestra
> vale 0.
> (3) **Medio nulo**: con σa=σs=0 NO ejecutes el marching ni consumas RNG extra: la
> salida de `./rt fogpt <spp> <escena> 0 0` debe ser idéntica BIT A BIT a
> `./rt ptexp <spp> <escena>`.
> (4) Genera exactamente estos renders (nombres literales, 1024×768, gamma 1/2.2):
> `image-fogpt-multil-000.ppm` (fogpt 64 multil 0 0),
> `image-fogpt-multil-s.ppm` (fogpt 64 multil 0 0.015),
> `image-fogpt-multil-as.ppm` (fogpt 64 multil 0.008 0.015),
> `image-fogpt-areal-s.ppm` (fogpt 64 areal 0 0.015).
> (5) Reproducibilidad: mismo comando → mismo archivo bit a bit, con 1 o N hilos.
> Autoverificación analítica: (a) fogpt multil 0 0 == ptexp multil (bit a bit, mismo
> spp); (b) media global de multil-as < multil-s < ... la absorción y la extinción
> solo pueden bajar la media global respecto a 000; (c) el CONTRASTE banda-de-luces/
> global (filas y∈[100,300), donde viven los tres emisores) debe crecer ≥1.25× en
> multil-s respecto a multil-000 (glow volumétrico de las fuentes, incluidas las de
> área); (d) el glow es CROMÁTICO: en multil-s, el promedio del canal R en la mitad
> izquierda de la banda (columnas < 512) debe superar al canal B ahí mismo, y el B a
> la R en la mitad derecha (la emisora roja está a la izquierda, la azul a la
> derecha); (e) sin NaN ni negativos.
> Antes de dar por terminado, `make && bash sdd/phase-07/check.sh` debe quedar verde.

## Criterios de aceptación (gate objetivo — check.sh)

1. `make` compila sin errores.
2. Medio nulo: `fogpt 64 multil 0 0` == `ptexp 64 multil` bit a bit.
3. Extinción monótona: media global multil-as < multil-s < multil-000 (estricto).
4. Glow volumétrico: contraste banda y∈[100,300) vs global crece ≥1.25× en multil-s
   respecto a multil-000.
5. Glow cromático: R>B en la mitad izquierda de la banda y B>R en la derecha
   (multil-s) — el in-scattering conserva el color de cada fuente de área.
6. Reproducibilidad bit a bit (2 corridas de multil-as) con OMP 1 vs N hilos.
7. Rango 0-255 sano en todos los renders.
