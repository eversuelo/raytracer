---
type: spec
project: aitl-raytracer
fase: 6
title: Phase 06 — Medios participantes: niebla homogénea con single scattering (Tema 7)
base: resultado aceptado de phase-02 (fuente puntual determinista)
enunciado: state-of-art/Tema 7 - Medios participantes.pdf
status: approved
---

# Phase 06 — Medios participantes (estado del arte, Tema 7)

## Contexto

Extensión FUERA del temario de proyectos 1-5: llevar el trazador al estado del arte del
curso (Tema 7, "Medios participantes", basado en Jarosz/Nowrouzezahrai). La escena de la
phase-02 (Cornell de esferas + fuente puntual en (0, 24.3, 0), I = (4000,4000,4000)) se
sumerge en un **medio participante homogéneo** (niebla) y se renderiza con **ray
marching + single scattering**. No hay render de referencia: la autoverificación es
analítica (propiedades físicas del medio), como corresponde a trabajo nuevo.

## Teoría aplicable (Tema 7)

- RTE con extinción σt = σa + σs; en medio homogéneo la transmitancia colapsa a
  `Tr(x1,x2) = exp(−σt·‖x2−x1‖)`.
- Estimador de ray marching (single scattering):
  `L(x,ω) ≈ Tr(x,xs)·Lo(xs,ω) + Σ_t Tr(x,xt)·σs·p(ω,ωl)·I·Tr(xt,xl)·Δt / d²`
  con xs el hit de superficie, xt los puntos de marcha sobre el rayo primario, xl la
  fuente puntual, d = ‖xl−xt‖ y `Lo` la radiancia de superficie de la phase-02
  ATENUADA además por la transmitancia del rayo de sombra (`Tr(x_hit, xl)`).
- Función de fase **isotrópica**: `p = 1/(4π)` (documenta en un comentario dónde
  entraría Henyey-Greenstein con g≠0; no la implementes).

## Prompt de tarea

> Partiendo del `rt.cpp` actual (resultado de la fase 2: escena difusa + fuente puntual
> determinista en (0,24.3,0) con I=(4000,4000,4000)), implementa un MEDIO PARTICIPANTE
> HOMOGÉNEO con single scattering por ray marching, según el Tema 7 del curso:
> (1) Nuevo modo CLI: `./rt fog <sigma_a> <sigma_s>` (ambos double ≥ 0). El medio llena
> toda la escena; σt = σa + σs; transmitancia homogénea Tr(s) = exp(−σt·s).
> (2) Rayo primario por píxel (idéntico al de la fase 2, sin jitter): encuentra el hit
> de superficie a distancia s. La radiancia final del píxel es
> `L = Tr(s)·Lo_atenuada + L_medio`, donde `Lo_atenuada` es el shading difuso de la
> fase 2 en el hit pero con su término de luz multiplicado por `exp(−σt·d_sombra)`
> (la transmitancia del rayo de sombra hacia la fuente), y `L_medio` es la suma de
> ray marching: para pasos t = Δ/2, 3Δ/2, … < s con Δ = 2.0 unidades de escena,
> `L_medio += exp(−σt·t) · σs · (1/(4π)) · I · exp(−σt·d_t) · Δ / d_t²`,
> con d_t la distancia del punto de marcha a la fuente puntual (visibilidad del rayo
> de sombra contra las esferas de la escena incluida: si está ocluido, esa muestra de
> luz vale 0). Comenta en el código dónde entraría la función de fase Henyey-Greenstein.
> (3) `./rt fog 0 0` debe reproducir EXACTAMENTE la imagen de la fase 2 (modo point):
> mismo archivo bit a bit — el medio nulo no puede alterar nada.
> (4) Genera exactamente estos renders (P3, 1024×768, gamma 1/2.2, nombres literales):
> `image-fog-000.ppm` (fog 0 0), `image-fog-a02.ppm` (fog 0.02 0),
> `image-fog-s02.ppm` (fog 0 0.02), `image-fog-a01-s02.ppm` (fog 0.01 0.02).
> (5) Determinismo: el modo fog es determinista (sin RNG nuevo); misma imagen con 1 o
> N hilos OpenMP.
> Autoverificación analítica (no hay referencia): (a) `image-fog-000.ppm` == render del
> modo point de la fase 2 (cmp bit a bit); (b) la media global de `image-fog-a02.ppm`
> debe ser MENOR que la de fog-000 (la absorción solo quita energía); (c) el CONTRASTE
> parche/global del parche 64×64 centrado en el píxel de la fuente (512, 200) debe
> crecer al menos 1.5× en `image-fog-s02.ppm` respecto a fog-000 (glow de
> in-scattering: la extinción oscurece todo en absoluto, pero la dispersión concentra
> energía relativa alrededor de la fuente); (d) ningún píxel NaN ni negativo.
> Antes de dar por terminado, `make && bash sdd/phase-06/check.sh` debe quedar verde.

## Criterios de aceptación (gate objetivo — check.sh)

1. `make` compila sin errores.
2. `./rt fog 0 0` produce `image.ppm` idéntica bit a bit al modo `point` (medio nulo).
3. Absorción monótona: media global fog-a02 < fog-000 (estrictamente).
4. Glow de in-scattering: media del parche 64×64 en (512,200) de fog-s02 > fog-000.
5. Determinismo OpenMP: `OMP_NUM_THREADS=1` vs default → `cmp` exacto en fog-a01-s02.
6. Sin NaN/negativos (el PPM P3 no admite negativos; validar rango 0-255).
