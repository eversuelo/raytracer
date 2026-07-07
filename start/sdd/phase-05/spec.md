---
type: spec
project: aitl-raytracer
fase: 5
title: Phase 05 — Proyecto final: MIS (β=2) y Path Tracing explícito
base: ../phase-04 parte 1 (resultado aceptado)
enunciado: ./README.md
status: draft
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

**RF-5b.2** Longitud máxima de camino **10 rebotes** (parámetro CLI). Escenas de
referencia: una sola fuente (área o puntual) = escena de fases previas; multi-luz con el
aluminio en **α = 0.03** (del enunciado).

**Criterios 5b**: (1) renders coinciden con `image-PTEXP-{PointL,AreaL,MultiL}-10b-{32,512}`;
(2) aparece iluminación global visible (color bleeding de las paredes en las esferas,
brillo indirecto en sombras) ausente en fases anteriores; (3) el costo crece de forma
acotada con los rebotes (medir tiempo 1 vs 10 rebotes); (4) reproducibilidad por semilla.

## No-objetivos

Ruleta rusa, dieléctricos, mapas de fotones — fuera del alcance del curso base.

## Prompt de tarea (idéntico para C0 y C2 — una corrida por sub-fase)

**5a:**
> Partiendo del trazador de la fase 4 parte 1, implementa MIS con heurística de potencia
> β=2 según `sdd/phase-05/spec.md` (sub-fase 5a) y el pseudocódigo del enunciado:
> combina muestreo de luz y de BRDF con la API sample/eval/pdf. Demuestra la reducción de
> varianza a igual spp contra la fase 4 midiendo dos parches, sin cambiar la imagen
> convergida.

**5b:**
> Partiendo del trazador de la fase 4 parte 1, implementa Path Tracing explícito sesgado
> según `sdd/phase-05/spec.md` (sub-fase 5b): recursión con NEE por ángulo sólido, rebote
> indirecto por muestreo de BRDF, emisores visibles solo en bounce==1, máximo 10 rebotes
> (CLI). Renderiza las tres escenas de referencia (PointL, AreaL, MultiL con aluminio
> α=0.03) y verifica contra image-PTEXP-* y los criterios de aceptación.

## Entregables

`rt.cpp` final; renders 5a (comparativa de varianza) y 5b (`image-PTEXP-*.ppm`);
tabla de varianza/tiempos; `run_id` C0/C2 por sub-fase + `run-show` según
`TRAZABILIDAD.md`. Cierre del experimento: ADR final del proyecto con el veredicto
global C0 vs C2.
