---
type: spec
project: aitl-raytracer
fase: 4
title: Phase 04 — Microfacets: conductores ásperos (Proyecto 4)
base: ../phase-03 (resultado aceptado)
enunciado: ./Readme.md
status: approved
---

# Phase 04 — Modelo microfacet para conductores ásperos

## Contexto

Proyecto 4 del curso (`Readme.md` de esta carpeta manda). Se introduce el primer material
no difuso: **conductores ásperos** con el modelo microfacet (Torrance–Sparrow: término
D·G·F). Dos partes que producen la misma física con dos estrategias de muestreo distintas
— la comparación entre ambas es justamente el insumo del MIS de la fase final.

## Cambios a la escena (del enunciado)

Las **dos esferas inferiores** (r = 16.5) se reemplazan por conductores ásperos con
**α = 0.3**; los índices exactos (η, k) por canal RGB son los de la solución del curso:

| Esfera | Material | η | k |
|---|---|---|---|
| Abajo-izq (−23, −24.3, −34.6) | **Aluminio** | (1.66058, 0.88143, 0.521467) | (9.2282, 6.27077, 4.83803) |
| Abajo-der (23, −24.3, −3.6) | **Oro** | (0.143245, 0.377423, 1.43919) | (3.98479, 2.3847, 1.60434) |

Tres configuraciones de luces (una por familia de referencias):
- **point**: solo la puntual r=0 en (0, 24.3, 0), Le=(2000,2000,2000) → `image-RC-Point-32`.
- **areal**: una esfera emisora r=10.5 en (0, 24.3, 0), Le=(10,10,10) →
  `image-RC-AreaL-*` **y todas las `image-ISBRDF-*`** (la parte 2 usa ESTA escena).
- **2a1p** (la de phase-03): puntual + emisora r=10.5 en (−23, 24.3, 0), Le=(12,5,5) +
  emisora r=5 en (23, 24.3, −50), Le=(5,5,12) → `image-RC-ISSA-32`, `image-RC-ISArea-32`.

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

1. Los renders coinciden con las referencias del curso: parte 1 con las cinco
   `image-RC-*-32` (los tonos de las luces roja/azul aparecen en los reflejos de la
   escena 2a1p) y parte 2 con `image-ISBRDF-{32,512}` sobre la escena **areal** —
   reflejos metálicos con highlights rugosos en aluminio (neutro) y oro (amarillo).
2. Consistencia física entre partes: sobre la escena areal, parte 1 y parte 2 convergen
   a la misma imagen al subir spp; difieren solo en dónde está el ruido (parte 1
   ruidosa en reflejos nítidos, parte 2 en sombras/luz).
3. Conservación de energía: sin píxeles NaN ni fireballs negativos; F ≤ 1 por canal.
4. Los materiales difusos del resto de la escena no cambian (sin regresión vs phase-03).
5. Reproducibilidad por semilla, 1 o N hilos.

## No-objetivos

Dieléctricos ásperos (extensión posterior si el curso la exige); combinación de ambos
muestreos en un solo estimador (MIS — fase final).

## Prompt de tarea (idéntico para C0 y C2)

> Partiendo del trazador de la fase 3, implementa el modelo microfacet para conductores
> ásperos (solo iluminación directa, sin rebotes). Reemplaza las dos esferas inferiores
> (r=16.5) por conductores con **α=0.3**: abajo-izq (−23,−24.3,−34.6) **aluminio**
> η=(1.66058, 0.88143, 0.521467), k=(9.2282, 6.27077, 4.83803); abajo-der (23,−24.3,−3.6)
> **oro** η=(0.143245, 0.377423, 1.43919), k=(3.98479, 2.3847, 1.60434). Paredes, cámara,
> resolución (1024×768) y salida (image.ppm P3, gamma 2.2) no se tocan.
> BRDF: `fr = D(ωh)·G·F/(4|n·ωi||n·ωo|)` (0 si ωi u ωo quedan bajo el hemisferio), con:
> — **D de Beckmann**: `D(ωh) = χ⁺(cosθh)·exp(−tan²θh/α²)/(π·α²·cos⁴θh)`.
> — **G de Smith**: `G = G1(ωi)·G1(ωo)`; con `a = 1/(α·tanθv)`:
>   `G1 = (3.535a+2.181a²)/(1+2.276a+2.577a²)` si `a<1.6`, si no `1`; por `χ⁺((v·ωh)/(v·n))`.
> — **Fresnel de conductor exacto por canal RGB** con θ = acos(ωi·ωh) y medio η=1:
>   `a²b² = √((η²−k²−sin²θ)² + 4η²k²)`, `A = √((a²b²+η²−k²−sin²θ)/2)`,
>   `R⊥ = (a²b²+cos²θ−2A·cosθ)/(a²b²+cos²θ+2A·cosθ)`,
>   `R∥ = R⊥·(a²b²·cos²θ+sin⁴θ−2A·cosθ·sin²θ)/(a²b²·cos²θ+sin⁴θ+2A·cosθ·sin²θ)`,
>   `F = (R⊥+R∥)/2`.
> Expón la API sample/eval/pdf para todo material. Difusos: coseno hemisférico
> (`θ=acos(√(1−ξ₁))`, `φ=2πξ₂`, pdf=cosθ/π, fr=ρ/π). Conductores (muestreo de BRDF):
> `θh = atan(√(−α²·ln(1−ξ₁)))`, `φh = 2πξ₂`, `ωi = 2(ωh·ωo)ωh − ωo`,
> `pdf(ωi) = D(ωh)·cosθh/(4|ωo·ωh|)`; la muestra solo suma si el rayo alcanza un emisor:
> `fr⊙Le·(n·ωi)/pdf`. En los modos de muestreo de luz (ángulo sólido y área de la fase 3)
> acumula TODAS las fuentes por muestra — puntuales con `fr⊙Le·cosθ/d²` y visibilidad —,
> evalúa la BRDF microfacet con `ωh = (ωo+ωi)/|ωo+ωi|` y recorta contribuciones
> negativas a 0. Un rayo de cámara que toca un emisor devuelve su Le.
> CLI: `./rt <modo> <spp> <escena>` (mismo orden que la fase 3) con modos
> `issa|isarea|isbrdf` y escenas
> `point` (solo puntual r=0 en (0,24.3,0), Le=(2000,2000,2000)),
> `areal` (una emisora r=10.5 en (0,24.3,0), Le=(10,10,10)) y
> `2a1p` (la puntual + emisoras r=10.5 en (−23,24.3,0), Le=(12,5,5) y r=5 en
> (23,24.3,−50), Le=(5,5,12)). RNG sembrado fijo y reproducible con 1 o N hilos.
> Genera (renombrando image.ppm): `image-RC-Point-32.ppm` (point, issa),
> `image-RC-ISSA-32.ppm` y `image-RC-ISArea-32.ppm` (2a1p), `image-RC-AreaL-ISSA-32.ppm`
> y `image-RC-AreaL-ISArea-32.ppm` (areal), e `image-ISBRDF-{32,512}.ppm`
> (areal, isbrdf); el render 2048 es opcional (su referencia sirve para comparar
> convergencia visual, no lo exige el gate). Autoverificación (render estocástico:
> compara promedios, no píxeles): reduce tu PPM y el JPG de referencia con filtro BOX
> a 64×48 (PIL `Image.BOX`) y exige media |Δ| ≤ 2.0 y máx |Δ| ≤ 12 (escala 0–255) en
> todas las variantes de muestreo de luz a 32 spp; para ISBRDF: ≤ 6.0/40 a 32 spp y
> ≤ 2.5/15 a 512. Antes de dar por terminado, `make && bash sdd/phase-04/check.sh`
> debe quedar verde.

## Entregables

`rt.cpp` extendido; renders `image-RC-*-32.ppm` (5 variantes) e
`image-ISBRDF-{32,512}.ppm` (2048 opcional); `run_id` C0/C2 + `run-show` según
`TRAZABILIDAD.md`.
