---
type: spec
project: aitl-raytracer
fase: 3
title: Phase 03 — Muestreo de importancia de fuentes esféricas y múltiples emisores (Proyecto 3)
base: ../phase-02 (resultado aceptado)
enunciado: ./README.md
status: draft
---

# Phase 03 — Muestreo de importancia de fuentes + múltiples emisores

## Contexto

Proyecto 3 del curso (`README.md` de esta carpeta manda). Dos partes: (1) muestrear las
fuentes esféricas **por importancia** en lugar de muestrear el hemisferio a ciegas;
(2) soportar **múltiples emisores** — de área y puntuales — acumulando la contribución
de cada fuente en cada muestra del estimador.

## Escena de la parte 2 (del enunciado)

Sobre la escena difusa de fases previas, las fuentes luminosas son:

| Fuente | Radio | Posición | Albedo | Emisión |
|---|---|---|---|---|
| Puntual | 0.0 | (0, 24.3, 0) | (0,0,0) | (2000, 2000, 2000) |
| Esfera emisora 1 | 10.5 | (−23, 24.3, 0) | (1,1,1) | (12, 5, 5) |
| Esfera emisora 2 | 5 | (+23, 24.3, −50) | (1,1,1) | (5, 5, 12) |

Referencias renderizadas a **32 spp**.

## Requisitos funcionales

**RF-1 — Muestreo de área.** Muestrear un punto uniforme sobre la superficie de la
esfera emisora (pdf_área = 1/(4πr²)) y convertir la pdf a ángulo sólido:
`pdf_ω = pdf_área · d² / |cos(θ_luz)|`. Contribución `Le·fr·cosθ/pdf_ω` con test de
visibilidad (rayo de sombra).

**RF-2 — Muestreo del ángulo sólido.** Muestrear direcciones uniformes dentro del cono
subtendido por la esfera desde `x`: `cos(θ_max) = √(1 − (r/d)²)`,
`pdf_ω = 1/(2π(1 − cos θ_max))`. Misma contribución con visibilidad.

**RF-3 — Múltiples emisores.** Por cada muestra del estimador, iterar **todas** las
fuentes y acumular (pseudocódigo del enunciado): puntuales con su término determinista
(phase-02), esféricas con el muestreo de importancia elegido. La puntual se modela como
esfera de radio 0 (o lista aparte — mantener la decisión de la phase-02).

**RF-4 — Selección por CLI** del método de muestreo de fuentes (área | ángulo sólido)
para poder reproducir cada referencia.

## Criterios de aceptación

1. Parte 1: con una sola fuente de área, los renders a 32 spp coinciden con las
   referencias `image-RC-AreaL-ISArea-32` / `image-RC-AreaL-ISSA-32` (del material del
   curso); a igual spp, ambos métodos de importancia muestran **menos ruido** que el
   coseno hemisférico de la phase-01 (varianza medida en un parche).
2. El ángulo sólido produce menor varianza que el muestreo de área para fuentes
   pequeñas/lejanas (comparar parches de la esfera emisora 2).
3. Parte 2: la escena 2A1P (2 área + 1 puntual) coincide con las referencias
   `image-2A1P-ISLightArea-32` / `image-2A1P-ISSolidAngle-32`: tonos rojizo y azulado de
   los dos emisores visibles, sombras múltiples coherentes.
4. Energía no negativa y sin NaN (validar con un barrido de píxeles).
5. Reproducibilidad por semilla, 1 o N hilos.

## No-objetivos

Combinar muestreo de luz con muestreo de BRDF (eso es MIS, phase-05); materiales no
difusos (phase-04).

## Prompt de tarea (idéntico para C0 y C2)

> Partiendo del trazador de la fase 2, implementa el Proyecto 3 según
> `sdd/phase-03/spec.md`: muestreo de importancia de fuentes esféricas por área y por
> ángulo sólido (seleccionable por CLI), y soporte de múltiples emisores acumulando cada
> fuente por muestra (escena 2A1P de la spec: puntual r=0 con Le=2000 + dos esferas
> emisoras). Verifica contra las referencias a 32 spp y los criterios de aceptación.

## Entregables

`rt.cpp` extendido; renders `image-RC-AreaL-IS{Area,SA}-32.ppm` y
`image-2A1P-IS{LightArea,SolidAngle}-32.ppm`; comparación de varianza entre métodos;
`run_id` C0/C2 + `run-show` según `TRAZABILIDAD.md`.
