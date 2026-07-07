---
type: spec
project: aitl-raytracer
fase: 0
title: Phase 00 — Primer contacto: intersección y renders de depuración
base: ../base/rt.cpp
status: approved
---

# Phase 00 — Primer contacto: intersección rayo-esfera y renders de depuración

## Contexto

El punto de partida es `sdd/base/rt.cpp` (172 líneas, C++, compilado con
`make` → `g++ -O3 -fopenmp`): un esqueleto docente con la infraestructura ya resuelta
—`Vector`/`Point`/`Color`, `Ray`, la escena, la cámara, el clamp, la corrección gamma
(`toDisplayValue`, γ = 2.2) y el volcado PPM— y **cuatro huecos marcados `PROYECTO 1`**
que esta fase llena. No hay iluminación todavía: el objetivo es ver la geometría.

La escena es una caja tipo Cornell construida con esferas gigantes (r = 1e5) como
paredes, más tres esferas visibles:

| Objeto | Radio | Posición | Color |
|---|---|---|---|
| Pared izquierda | 1e5 | (−1e5−49, 0, 0) | (.75, .25, .25) |
| Pared derecha | 1e5 | (1e5+49, 0, 0) | (.25, .25, .75) |
| Pared trasera | 1e5 | (0, 0, −1e5−81.6) | (.75, .75, .75) |
| Suelo | 1e5 | (0, −1e5−40.8, 0) | (.75, .75, .75) |
| Techo | 1e5 | (0, 1e5+40.8, 0) | (.75, .75, .75) |
| Esfera abajo-izq | 16.5 | (−23, −24.3, −34.6) | (.999, .999, .999) |
| Esfera abajo-der | 16.5 | (23, −24.3, −3.6) | (.999, .999, .999) |
| Esfera arriba | 10.5 | (0, 24.3, 0) | (1, 1, 1) |

Cámara: origen (0, 11.2, 214), dirección (0, −0.042612, −1) normalizada.
Resolución: **1024 × 768**. **La escena y la cámara no se tocan** — son la referencia
contra la que se validan las imágenes.

## Objetivo

Completar el trazador mínimo: que cada rayo de cámara encuentre la intersección más
cercana con la escena y que el programa produzca **dos renders de depuración** que
coincidan con las imágenes de referencia de este directorio:

- `image-normalcolor.jpg` — color = normal del punto de intersección.
- `image-distance.jpg` — tono de gris = distancia de intersección.

## Requisitos funcionales

**RF-1 — Intersección rayo-esfera** (`Sphere::intersect`).
Resolver `|o + t·d − p|² = r²` con `d` normalizada: con `oc = o − p`,
`b = oc·d`, `det = b² − (oc·oc − r²)`:
- `det < 0` → sin intersección → devolver `0.0`.
- Si no, devolver la menor raíz positiva `t = −b ± √det` que sea mayor que un
  épsilon (`t > 1e−4`) para evitar auto-intersección; si ninguna lo es, `0.0`.

**RF-2 — Intersección con la escena** (función global `intersect(r, t, id)`).
Recorrer `spheres[]`, quedarse con la intersección válida **más cercana**; escribir en
`t` la distancia y en `id` el índice de la esfera ganadora; devolver `true`/`false`.

**RF-3 — Shading de depuración** (`shade`).
Con el hit más cercano: punto `x = r.o + r.d·t`; normal `n = (x − obj.p).normalize()`.
Dos modos de color (seleccionables por argumento de línea de comandos, p. ej.
`./rt normal` y `./rt distance`, o por constante documentada si se prefiere):
- **Modo normales**: `color = (n + Vector(1,1,1)) * 0.5` — mapea cada componente de
  [−1,1] a [0,1]. Verificación rápida contra la referencia: pared izquierda
  (n = (1,0,0)) → salmón (1,.5,.5); derecha → azul verdoso (0,.5,.5); techo →
  magenta (.5,0,.5); suelo → verde claro (.5,1,.5); pared trasera → lavanda (.5,.5,1).
- **Modo distancia**: gris monótono en función de `t` (mismo valor en RGB). El mapeo
  exacto queda a elección del implementador pero debe (a) ser monótono, (b) estar
  documentado en un comentario junto a la fórmula, y (c) reproducir cualitativamente
  la referencia: las tres esferas y las paredes distinguibles, sin saturar todo a
  blanco o negro.

**RF-4 — Paralelización** (bucle de píxeles en `main`).
`#pragma omp parallel for` sobre el bucle exterior (renglones), cada hilo computa
renglones completos. El resultado debe ser **determinista**: la imagen es idéntica
bit a bit con 1 hilo o con N (ningún estado compartido mutable entre píxeles; el
contador de progreso en stderr puede variar).

**RF-5 — Salida PPM** (ya esbozada en `main`).
`image.ppm` en formato **P3** (ASCII): cabecera `P3\n<w> <h>\n255\n` y w×h tripletas
RGB pasadas por `toDisplayValue` (clamp + gamma 1/2.2). El archivo debe abrir en
cualquier visor PPM.

## Criterios de aceptación (gate objetivo)

1. `make` compila sin errores (g++ −O3 −fopenmp).
2. `./rt normal` produce un `image.ppm` de 1024×768 cuyos colores de pared coinciden
   con la tabla de RF-3 (muestrear el píxel central de cada pared: tolerancia ±10/255
   por canal tras gamma) y las tres esferas aparecen con su gradiente cian/blanco/rosa
   en las posiciones de `image-normalcolor.jpg`.
3. `./rt distance` produce un gris monótono consistente con `image-distance.jpg`
   (siluetas de las tres esferas visibles contra la pared trasera).
4. Determinismo: dos corridas consecutivas producen archivos idénticos (`cmp` exacto);
   `OMP_NUM_THREADS=1` vs default también.
5. Sin fugas: el `delete[]` existente se conserva; no hay `new` sin liberar añadidos.

## No-objetivos (quedan para fases posteriores)

Iluminación directa, sombras, materiales, reflexión/refracción, antialiasing,
muestreo — nada de eso entra aquí. Phase 00 termina cuando la geometría se ve.

## Prompt de tarea (idéntico para C0 y C2)

> Completa los cuatro bloques marcados `PROYECTO 1` en `rt.cpp` siguiendo la spec de
> `sdd/phase-00/spec.md`: (1) `Sphere::intersect` con la fórmula de la spec y
> épsilon 1e−4; (2) `intersect(r, t, id)` devolviendo el hit más cercano de la escena;
> (3) `shade` con los dos modos de depuración —normales `(n+1)*0.5` y distancia en
> gris monótono— seleccionables por argumento; (4) paraleliza el bucle de renglones
> con OpenMP manteniendo la imagen determinista. No modifiques la escena, la cámara,
> la resolución ni el formato de salida. Verifica contra las imágenes de referencia
> del directorio de la spec y los criterios de aceptación antes de dar por terminado.

## Entregables

- `rt.cpp` completado (en la rama de la condición que corresponda del repo
  `aitl-raytracer`, o aquí en `sdd/` si la corrida es exploratoria).
- `image-normal.ppm` e `image-distance.ppm` generados (renombrar el `image.ppm` de
  cada modo), comparados contra las referencias.
- Registro de la corrida: `run_id` de `aitl run --project aitl-raytracer` (C0 con
  `--bare`, C2 sin él) y `aitl run-show <id>` archivado según `MANUAL.md`.
