---
type: spec
project: aitl-raytracer
fase: 3
title: Phase 03 — Muestreo de importancia de fuentes esféricas y múltiples emisores (Proyecto 3)
base: ../phase-02 (resultado aceptado)
enunciado: ./README.md
status: approved
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

1. Parte 1: con una sola fuente de área (escena 1L, la de la phase-01), los renders a
   32 spp coinciden con `image-ISLightArea-32` / `image-ISLightSolidAngle-32`; a igual
   spp, ambos métodos de importancia muestran **menos ruido** que el coseno
   hemisférico de la phase-01 (varianza medida en un parche).
2. El ángulo sólido produce menor varianza que el muestreo de área para fuentes
   pequeñas/lejanas (comparar parches de la esfera emisora 2).
3. Parte 2: la escena 2A1P (2 área + 1 puntual) coincide con las referencias
   `image-2A1P-ISLightArea-32` / `image-2A1P-ISSolidAngle-32`: tonos rojizo y azulado de
   los dos emisores visibles, sombras múltiples coherentes. El modo coseno hemisférico
   con el término puntual determinista reproduce `image-2A1P-cosinehemi-2048` (sin ese
   término la imagen sale ~18/255 más oscura — error clásico).
4. Energía no negativa y sin NaN (validar con un barrido de píxeles).
5. Reproducibilidad por semilla, 1 o N hilos.

## No-objetivos

Combinar muestreo de luz con muestreo de BRDF (eso es MIS, phase-05); materiales no
difusos (phase-04).

## Prompt de tarea (idéntico para C0 y C2)

> Partiendo del trazador de la fase 2, implementa el Proyecto 3 según `sdd/phase-03/spec.md`:
> muestreo de importancia de fuentes esféricas y soporte de múltiples emisores.
>
> **Escenas.** `1L` = la escena difusa de la fase 1 (emisor `Sphere(10.5,(0,24.3,0),albedo(1,1,1),Le=(10,10,10))`).
> `2A1P` = la misma escena difusa reemplazando ese emisor por TRES fuentes:
> `Sphere(0.0,(0,24.3,0),(0,0,0),Le=(2000,2000,2000))` (puntual, radio 0 — nota: 2000, no el 4000 de la fase 2),
> `Sphere(10.5,(-23,24.3,0),(1,1,1),Le=(12,5,5))` y `Sphere(5,(+23,24.3,-50),(1,1,1),Le=(5,5,12))`.
> No cambies cámara, resolución (1024×768), gamma (1/2.2) ni el PPM P3.
>
> **Modo `arealight`** — por fuente esférica: muestrea x′ = c + r·ω con ω uniforme en la esfera
> (θ=acos(1−2ξ₁), φ=2πξ₂); pdf en ángulo sólido `pdf_ω = d²/(4πr²·cosθ′)` con d=|x′−x| y
> cosθ′ = n_luz·(dirección x′→x); contribución `(albedo/π)·Le·cosθ/pdf_ω` con rayo de sombra
> (x′ visible desde x); si cosθ′≤0 o cosθ<0 la muestra vale 0.
> **Modo `solidangle`** — muestrea el cono hacia la fuente: eje W=normalize(c−x),
> `cosθmax = √(1−(r/|c−x|)²)`, dirección θ=acos(1−ξ₁+ξ₁·cosθmax), φ=2πξ₂ en base local de W;
> `pdf_ω = 1/(2π(1−cosθmax))`; visible si el PRIMER hit del rayo (x,ω) es esa fuente; misma contribución.
> **Fuente puntual (r=0)** — término determinista de la fase 2: `(albedo/π)·Le·cosθ/d²` con rayo de sombra.
> **Múltiples emisores**: en cada muestra del estimador itera TODAS las fuentes y SUMA sus
> contribuciones (las esféricas con el método elegido, las puntuales con su término determinista;
> sin selección aleatoria de fuente, sin dividir entre el número de luces).
> **Modo `coshemi`**: el estimador coseno-hemisférico de la fase 1 MÁS el término determinista de
> cada puntual por muestra (una puntual r=0 nunca es alcanzada por un rayo aleatorio).
> Si el rayo de cámara toca un emisor, devuelve su Le (clampa a blanco). Sin NaN ni energía negativa.
> RNG seguro para OpenMP y reproducible por semilla (misma semilla → mismo archivo, 1 o N hilos).
>
> **CLI y salidas** — `./rt <modo> <spp> <escena>` con modo ∈ {arealight, solidangle, coshemi} y
> escena ∈ {1L, 2A1P}. Genera exactamente estos cinco renders (nombres literales):
> `image-ISLightArea-32.ppm` (arealight 32 1L), `image-ISLightSolidAngle-32.ppm` (solidangle 32 1L),
> `image-2A1P-ISLightArea-32.ppm` (arealight 32 2A1P), `image-2A1P-ISSolidAngle-32.ppm`
> (solidangle 32 2A1P), `image-2A1P-cosinehemi-2048.ppm` (coshemi 2048 2A1P).
>
> **Autoverificación** (el render es estocástico; compara promedios, no píxeles). Media global RGB
> esperada (0–255, tras gamma): ISLightArea-32 ≈ (73, 76, 71); ISLightSolidAngle-32 ≈ (74, 77, 72);
> 2A1P-ISLightArea-32 ≈ (95, 83, 81); 2A1P-ISSolidAngle-32 ≈ (96, 83, 81);
> 2A1P-cosinehemi-2048 ≈ (96, 83, 81) — todas ±2 por canal. Parche plano de pared trasera
> (píxeles x∈[500,532), y∈[380,412)): 1L ≈ (55, 91, 55); 2A1P ≈ (75, 103, 70) ±3.
> Ruido (std de un parche 64×64 de pared trasera a 32 spp): solidangle ≈ 1.4–2.5 ≪ arealight ≈ 5–10
> ≪ coshemi-32 ≈ 28 — verifica ese orden de varianza y repórtalo. Verifica también contra los JPG
> de referencia del directorio de la spec. Antes de dar por terminado,
> `make && bash sdd/phase-03/check.sh` debe quedar verde.

## Entregables

`rt.cpp` extendido; renders `image-ISLightArea-32.ppm`, `image-ISLightSolidAngle-32.ppm`,
`image-2A1P-ISLightArea-32.ppm`, `image-2A1P-ISSolidAngle-32.ppm` e
`image-2A1P-cosinehemi-2048.ppm`; comparación de varianza entre métodos;
`run_id` C0/C2 + `run-show` según `TRAZABILIDAD.md`.
