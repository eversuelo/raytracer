---
type: spec
project: aitl-raytracer
fase: 2
title: Phase 02 — Iluminación directa con fuentes puntuales (Proyecto 2, parte 2)
base: ../phase-01 (resultado aceptado)
enunciado: ./enunciado-original.md
status: draft
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

> Partiendo del trazador de la fase 1, agrega fuentes de luz puntuales según
> `sdd/phase-02/spec.md`: contribución determinista `fr·I·cosθ/r²` con rayo de sombra
> (oclusión → contribución cero), fuente en (0, 24.3, 0) con intensidad (4000,4000,4000)
> sobre la escena difusa existente. Conserva el modo de la fase 1 sin regresión. Verifica
> contra `image-plight.jpg` y los criterios de aceptación.

## Entregables

`rt.cpp` extendido; `image-plight.ppm`; `run_id` C0/C2 + `run-show` según
`TRAZABILIDAD.md`.
