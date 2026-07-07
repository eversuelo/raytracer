---
type: spec
project: aitl-raytracer
fase: 4
title: Phase 04 — Microfacets: conductores ásperos (Proyecto 4)
base: ../phase-03 (resultado aceptado)
enunciado: ./Readme.md
status: draft
---

# Phase 04 — Modelo microfacet para conductores ásperos

## Contexto

Proyecto 4 del curso (`Readme.md` de esta carpeta manda). Se introduce el primer material
no difuso: **conductores ásperos** con el modelo microfacet (Torrance–Sparrow: término
D·G·F). Dos partes que producen la misma física con dos estrategias de muestreo distintas
— la comparación entre ambas es justamente el insumo del MIS de la fase final.

## Cambios a la escena (del enunciado)

Mismas fuentes luminosas que las etapas anteriores (escena 2A1P de la phase-03), pero las
**dos esferas inferiores** se reemplazan por conductores ásperos con **α = 0.3**:

| Esfera | Material |
|---|---|
| Abajo-izq (−23, −24.3, −34.6) | **Aluminio** (η, k del curso/PBR) |
| Abajo-der (23, −24.3, −3.6) | **Oro** (η, k del curso/PBR) |

Los índices (η, k) por canal RGB se toman de lo visto en clase (o tablas del PBR book);
documentar los valores exactos usados en un comentario.

## Requisitos funcionales

**RF-1 — BRDF microfacet de conductor.** `fr = D(ωh)·G(ωo,ωi)·F(ωo·ωh) / (4·cosθo·cosθi)`
con distribución D y término de sombreado G según lo visto en clase (Beckmann o GGX —
documentar la elección), y Fresnel de conductor (η, k) por canal.

**RF-2 — Parte 1: muestreo de fuente de luz.** Estimar iluminación directa sobre los
conductores muestreando las fuentes (ángulo sólido, phase-03) y evaluando la BRDF
microfacet en la dirección muestreada.

**RF-3 — Parte 2: muestreo de BRDF.** Muestrear la dirección según la distribución D
(half-vector), reflejar `ωo` para obtener `ωi`, y evaluar: `pdf(ωi) = D(ωh)·cosθh /
(4·(ωo·ωh))`. La contribución solo suma si el rayo muestreado alcanza un emisor.

**RF-4 — API del material: sample / eval / pdf.** Separar las tres operaciones (el
temario las exige) de modo que la fase final pueda combinarlas: `sample(ωo) → ωi`,
`eval(ωo, ωi) → fr`, `pdf(ωo, ωi) → p`. Los difusos existentes se adaptan a la misma
interfaz (coseno hemisférico).

## Criterios de aceptación

1. Los renders coinciden con las referencias del curso: parte 2 con
   `image-ISBRDF-{32,512,2048}` — reflejos metálicos con highlights rugosos en aluminio
   (neutro) y oro (amarillo), tonos de las luces roja/azul visibles en los reflejos.
2. Consistencia física entre partes: parte 1 y parte 2 convergen a la **misma imagen** al
   subir spp (comparar 2048 spp: diferencia RMS pequeña y decreciente); difieren solo en
   dónde está el ruido (parte 1 ruidosa en reflejos nítidos, parte 2 en sombras/luz).
3. Conservación de energía: sin píxeles NaN ni fireballs negativos; F ≤ 1 por canal.
4. Los materiales difusos del resto de la escena no cambian (sin regresión vs phase-03).
5. Reproducibilidad por semilla, 1 o N hilos.

## No-objetivos

Dieléctricos ásperos (extensión posterior si el curso la exige); combinación de ambos
muestreos en un solo estimador (MIS — fase final).

## Prompt de tarea (idéntico para C0 y C2)

> Partiendo del trazador de la fase 3, implementa el Proyecto 4 según
> `sdd/phase-04/spec.md`: modelo microfacet para conductores ásperos (D·G·F con Fresnel
> conductor por canal), reemplazando las dos esferas inferiores por aluminio y oro con
> α=0.3. Parte 1: iluminación directa muestreando la fuente. Parte 2: muestreo de BRDF
> vía half-vector. Expón sample/eval/pdf como API del material (difusos incluidos).
> Verifica contra las referencias image-ISBRDF-* y los criterios de aceptación.

## Entregables

`rt.cpp` extendido; renders de ambas partes a 32/512/2048 spp; comparación RMS
parte 1 vs parte 2 a 2048 spp; `run_id` C0/C2 + `run-show` según `TRAZABILIDAD.md`.
