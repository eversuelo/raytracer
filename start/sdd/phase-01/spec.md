---
type: spec
project: aitl-raytracer
fase: 1
title: Phase 01 — Iluminación directa por Monte Carlo con emisores esféricos (Proyecto 2, parte 1)
base: ../phase-00 (resultado aceptado)
enunciado: ./enunciado-original.md
status: approved
---

# Phase 01 — Iluminación directa por Monte Carlo (emisores esféricos, difusos)

## Contexto

Extiende el trazador de la phase-00: `shade` deja de pintar depuración y pasa a **estimar
la ecuación de renderizado de iluminación directa por Monte Carlo**. Algunas esferas
emiten luz y todos los materiales son difusos (Lambert). El enunciado original del curso
está en `enunciado-original.md` y manda sobre esta spec en caso de conflicto.

## Cambios a la escena (obligatorios, del enunciado)

`Sphere` gana un cuarto parámetro: la **emisión** `Le` (Color). Escena nueva:

| Objeto | Radio | Posición | Albedo | Emisión |
|---|---|---|---|---|
| Pared izq | 1e5 | (−1e5−49, 0, 0) | (.75,.25,.25) | (0,0,0) |
| Pared der | 1e5 | (1e5+49, 0, 0) | (.25,.25,.75) | (0,0,0) |
| Pared detrás | 1e5 | (0, 0, −1e5−81.6) | (.25,.75,.25) | (0,0,0) |
| Suelo | 1e5 | (0, −1e5−40.8, 0) | (.25,.75,.75) | (0,0,0) |
| Techo | 1e5 | (0, 1e5+40.8, 0) | (.75,.75,.25) | (0,0,0) |
| Esfera abajo-izq | 16.5 | (−23, −24.3, −34.6) | (.2,.3,.4) | (0,0,0) |
| Esfera abajo-der | 16.5 | (23, −24.3, −3.6) | (.4,.3,.2) | (0,0,0) |
| Esfera arriba (emisor) | 10.5 | (0, 24.3, 0) | (1,1,1) | **(10,10,10)** |

## Requisitos funcionales

**RF-1 — Estimador Monte Carlo de iluminación directa.** Por píxel, promediar N
muestras (spp configurable). Cada muestra: desde el punto de intersección `x` con normal
`n`, muestrear una dirección `wi`, lanzar rayo; si toca un emisor con radiancia `Le`,
acumular `Le · fr · cos(θ) / pdf(wi)` con `fr = albedo/π` (difuso). Si el rayo de cámara
toca directamente un emisor, devolver su emisión (aclaración del enunciado: el emisor no
se pinta negro).

**RF-2 — Tres métodos de muestreo de direcciones**, seleccionables (argumento CLI):
1. **Uniforme esférico** — pdf = 1/(4π).
2. **Uniforme hemisférico** (alineado a `n`) — pdf = 1/(2π).
3. **Coseno hemisférico** — pdf = cos(θ)/π.
Construir base ortonormal local sobre `n` para los hemisféricos.

**RF-3 — spp configurable** por CLI: al menos 32, 512 y 2048 (los tres valores de las
referencias).

**RF-4 — RNG seguro para OpenMP.** Generador por hilo (o por píxel, sembrado por índice
de píxel): sin `rand()` global compartido. Con semilla fija, la imagen es reproducible.

## Criterios de aceptación

1. `make` limpio; `./rt <sampler> <spp>` produce `image.ppm` 1024×768.
2. Las 9 combinaciones (3 samplers × 32/512/2048 spp) coinciden visualmente con
   `image-{uniformsphere,uniformhemi,cosinehemi}{32,512,2048}.jpg`:
   mismo tono global, sombra suave bajo las esferas, techo iluminado por el emisor.
3. El ruido **disminuye** al subir spp, y a igual spp: coseno < hemisférico < esférico
   (varianza muestral medida sobre un parche uniforme, p. ej. 32×32 px de la pared trasera).
4. El emisor se ve blanco brillante (clamp de su emisión), nunca negro.
5. Reproducibilidad: misma semilla → mismo archivo (`cmp` exacto), con 1 o N hilos.

## No-objetivos

Luces puntuales (phase-02), muestreo de importancia de fuentes (phase-03), materiales no
difusos (phase-04+).

## Prompt de tarea (idéntico para C0 y C2)

> Partiendo del `rt.cpp` de la fase 0, implementa iluminación directa por Monte Carlo.
> (1) `Sphere` gana un cuarto parámetro `Color ke` (emisión). Reemplaza la escena por:
> `{1e5,(-1e5-49,0,0),(.75,.25,.25),(0,0,0)}, {1e5,(1e5+49,0,0),(.25,.25,.75),(0,0,0)},`
> `{1e5,(0,0,-1e5-81.6),(.25,.75,.25),(0,0,0)}, {1e5,(0,-1e5-40.8,0),(.25,.75,.75),(0,0,0)},`
> `{1e5,(0,1e5+40.8,0),(.75,.75,.25),(0,0,0)}, {16.5,(-23,-24.3,-34.6),(.2,.3,.4),(0,0,0)},`
> `{16.5,(23,-24.3,-3.6),(.4,.3,.2),(0,0,0)}, {10.5,(0,24.3,0),(1,1,1),(10,10,10)}`.
> No toques cámara, resolución (1024×768), gamma ni formato PPM (P3) de la fase 0.
> (2) `shade`: sin intersección → negro; si el objeto intersectado emite (ke≠0) →
> devuelve su emisión como radiancia saliente (el clamp la deja blanca); si no, genera
> UNA muestra del estimador `L = fr ⊙ Le · (n·wi) / pdf(wi)` con `fr = albedo/π` y
> `Le` = ke de la esfera más cercana alcanzada por el rayo (x, wi) con épsilon 1e−4
> (la visibilidad va implícita en ese rayo). En `main`, promedia N muestras por píxel
> y clampea después de promediar. NO agregues jitter de píxel: el rayo de cámara de un
> píxel es idéntico en las N muestras (las referencias no tienen antialiasing).
> (3) Tres muestreadores (ξ1,ξ2 uniformes en [0,1); dirección local
> (sinθ·cosφ, sinθ·sinφ, cosθ) con φ = 2π·ξ2):
> uniformsphere: cosθ = 1−2ξ1, pdf = 1/(4π); uniformhemi: cosθ = ξ1, pdf = 1/(2π);
> cosinehemi: cosθ = √(1−ξ1), pdf = cosθ/π. Para pasar de local a global construye
> una base ortonormal sobre n: si |n.x|>|n.y| entonces t = (n.z, 0, −n.x)/√(n.x²+n.z²),
> si no t = (0, n.z, −n.y)/√(n.y²+n.z²); s = t×n; w = s·wx + t·wy + n·wz.
> (4) CLI: `./rt <sampler> <spp>` con sampler ∈ {uniformsphere, uniformhemi, cosinehemi}
> y spp entero (32, 512, 2048 son los de referencia). Salida `image.ppm`; renombra cada
> corrida a `image-<sampler><spp>.ppm` (p. ej. `image-cosinehemi32.ppm`).
> (5) RNG: usa `erand48` con estado `unsigned short[3]` POR PÍXEL, sembrado de forma
> determinista con el índice del píxel (nada de estado global compartido ni
> `srand(time(NULL))`): misma imagen bit a bit con 1 o N hilos OpenMP.
> Autoverificación (render estocástico): a 32 spp la media global RGB de `image.ppm`
> debe ser ≈(36,36,35)±4 para uniformsphere, ≈(46,46,44)±4 para uniformhemi y
> ≈(50,50,49)±4 para cosinehemi; el disco del emisor (píxel 512,200) es blanco puro
> (255,255,255); a igual spp el ruido cumple cosinehemi < uniformhemi < uniformsphere
> y disminuye al subir spp. Compara además visualmente contra
> `sdd/phase-01/image-<sampler><spp>.jpg`. Antes de dar por terminado,
> `make && bash sdd/phase-01/check.sh` debe quedar verde.

## Entregables

`rt.cpp` extendido; 9 renders nombrados `image-<sampler><spp>.ppm`; medición de varianza
por sampler; `run_id` C0/C2 + `run-show` archivados según `TRAZABILIDAD.md`.
