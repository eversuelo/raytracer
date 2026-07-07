---
type: spec
project: aitl-raytracer
fase: 2
title: Phase 02 — Iluminación directa con fuentes puntuales (Proyecto 2, parte 2)
base: ../phase-01 (resultado aceptado)
enunciado: ./enunciado-original.md
status: approved
---

# Phase 02 — Fuentes puntuales (sombras duras)

## Contexto

Segunda etapa del Proyecto 2: manejar **fuentes de luz puntuales** según la teoría del
curso. Misma escena que la phase-01, pero el emisor esférico se reemplaza por un emisor
puntual en la misma posición. El enunciado original (`enunciado-original.md`) manda.

## Parámetros (del enunciado)

- Posición de la fuente puntual: **(0, 24.3, 0)** (donde estaba la esfera emisora).
- Intensidad radiante: **I = (4000, 4000, 4000)**.
- Materiales: difusos (los albedos de la phase-01).

## Requisitos funcionales

**RF-1 — Contribución de la fuente puntual.** Para el punto de intersección `x` con
normal `n`: `Lo = fr · I · cos(θ) / r²`, con `fr = albedo/π`, `r` la distancia de `x` a
la fuente y `θ` el ángulo entre `n` y la dirección hacia la fuente. Sin Monte Carlo: la
integral colapsa a un término determinista por fuente.

**RF-2 — Rayo de sombra.** Antes de acumular, lanzar un rayo de `x` hacia la fuente; si
alguna esfera lo bloquea **antes de llegar a la fuente** (t_hit < r − ε), la
contribución es cero → sombras duras. Usar el épsilon de la fase 0 para evitar acné.

**RF-3 — Convivencia con emisores.** La arquitectura debe permitir escenas con fuentes
puntuales y/o esferas emisoras (la phase-03 las combinará): las puntuales se representan
como fuente con radio 0 o lista aparte — decisión documentada en el código.

## Criterios de aceptación

1. `make` limpio; el render de la escena con la puntual coincide con
   `image-plight.jpg`: sombras duras y nítidas bajo las tres esferas, caída 1/r²
   visible en paredes.
2. La imagen es **determinista** (sin ruido): dos corridas → `cmp` idéntico.
3. El punto directamente "debajo" de la fuente en el techo no se ilumina a sí mismo
   (sin autoiluminación del plano que la contiene ni acné de sombra).
4. La versión de la phase-01 (emisor esférico + MC) sigue funcionando: un flag/escena
   selecciona el modo — sin regresión (mismos resultados que phase-01).

## No-objetivos

Múltiples fuentes y muestreo de importancia (phase-03); materiales no difusos (phase-04).

## Prompt de tarea (idéntico para C0 y C2)

> Partiendo del trazador de la fase 1, implementa iluminación directa con **fuente
> puntual** según `sdd/phase-02/spec.md`. Escena: idéntica a la fase 1, pero la esfera
> emisora (radio 10.5 en (0, 24.3, 0)) se **elimina** y se reemplaza por una fuente
> puntual en **(0, 24.3, 0)** con intensidad radiante **I = (4000, 4000, 4000)** —
> represéntala como esfera de radio 0 con `ke = I` (ningún rayo la intersecta) o lista
> aparte, documentando la decisión. Los demás objetos y sus albedos no cambian.
> Shading determinista, **sin Monte Carlo, 1 rayo por píxel, sin jitter**: para el
> punto de intersección `x` con normal `n`, calcula `w = normalize(p_luz − x)`,
> `r = |p_luz − x|`, `cosθ = n·w`; si `cosθ ≤ 0` la contribución es 0; lanza un rayo
> de sombra desde `x` en dirección `w` y si alguna esfera lo intersecta con
> `t < r − 1e−4` (épsilon de la fase 0) la contribución es 0 (sombras duras); si es
> visible, `L = (albedo/π) ⊙ I · cosθ / r²` (producto componente a componente).
> No cambies cámara, resolución (1024×768), clamp, gamma (1/2.2) ni formato PPM P3.
> Selección por CLI **sin stdin interactivo**: conserva los modos MC de la fase 1 sin
> regresión y añade el modo puntual exactamente como `./rt point` (el spp no aplica en
> este modo). Guarda el render como **`image-plight.ppm`**. Autoverificación: (a) dos
> corridas → `cmp` idéntico; (b) en el PPM (columna, fila; fila 0 = primera fila del
> archivo, arriba) los valores RGB tras gamma deben ser, con tolerancia ±3 por canal:
> (512,384)=(60,99,60) pared trasera; (512,60)=(121,121,74) techo bajo la luz;
> (60,384)=(97,59,59); (964,384)=(59,59,97); (512,730)=(71,118,118);
> (240,600)=(0,0,0) sombra dura; (700,480)=(96,84,70); (340,440)=(58,70,79);
> (c) compara visualmente con `image-plight.jpg`: sombras duras bajo las esferas,
> caída 1/r² en paredes, techo sin esfera visible y sin acné. Antes de dar por
> terminado, `make && bash sdd/phase-02/check.sh` debe quedar verde.

## Entregables

`rt.cpp` extendido; `image-plight.ppm`; `run_id` C0/C2 + `run-show` según
`TRAZABILIDAD.md`.
