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
- **Modo normales**: `color = obj.c + n` — suma directa del color de la esfera y la
  normal, SIN reescalar (el clamp del pipeline recorta a [0,1]); es la convención de
  los renders de referencia del curso. Verificación rápida tras gamma: pared
  izquierda (255,136,136); derecha (0,136,224); trasera (224,224,255); suelo
  (224,255,224); techo (224,0,224); las tres esferas se ven blancas con franja cian
  a la izquierda y franja magenta abajo.
- **Modo distancia**: gris `g = (t − 144.676098) / (304.110891 − 144.676098)`,
  devolver `Color(g,g,g)` (mismo valor en RGB; fuera de [0,1] recorta el clamp).
  Mapeo lineal monótono acotado a los t extremos visibles de la escena; reproduce
  la referencia: pared trasera casi blanca, esferas en gris medio, esquinas
  cercanas oscuras.

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
2. `./rt normal` produce un `image.ppm` de 1024×768 cuyos colores coinciden con
   `obj.c + n` de RF-3 (sondas de `probe.py`: tolerancia ±10/255 por canal tras
   gamma) y las tres esferas aparecen blancas con franjas cian/magenta en las
   posiciones de `image-normalcolor.jpg`.
3. `./rt distance` produce un gris monótono consistente con `image-distance.jpg`
   (siluetas de las tres esferas visibles contra la pared trasera).
4. Determinismo: dos corridas consecutivas producen archivos idénticos (`cmp` exacto);
   `OMP_NUM_THREADS=1` vs default también.
5. Sin fugas: el `delete[]` existente se conserva; no hay `new` sin liberar añadidos.

## No-objetivos (quedan para fases posteriores)

Iluminación directa, sombras, materiales, reflexión/refracción, antialiasing,
muestreo — nada de eso entra aquí. Phase 00 termina cuando la geometría se ve.

## Prompt de tarea (idéntico para C0 y C2)

> Completa los cuatro bloques marcados `PROYECTO 1` en `rt.cpp`.
> (1) `Sphere::intersect`: con `oc = o − p`, `b = oc·d`, `det = b² − (oc·oc − r²)`;
> si `det < 0` devuelve `0.0`; si no, devuelve la menor raíz `t = −b ± √det` que sea
> mayor que `1e−4`, o `0.0` si ninguna lo es.
> (2) `intersect(r, t, id)`: recorre `spheres[]` y deja en `t`/`id` la intersección
> válida más cercana; devuelve `true`/`false`.
> (3) `shade` con dos modos seleccionables por argumento de línea de comandos
> (`./rt normal` y `./rt distance`). Con `x = r.o + r.d·t` y
> `n = (x − obj.p).normalize()`:
>   - `normal`: el color es la suma directa `obj.c + n` — SIN reescalar con
>     `(n+1)*0.5`; el clamp del pipeline recorta a [0,1]. Autoverificación tras
>     gamma: pared izquierda (255,136,136), derecha (0,136,224), trasera
>     (224,224,255), suelo (224,255,224), techo (224,0,224); las tres esferas se ven
>     blancas con franja cian a la izquierda y franja magenta abajo.
>   - `distance`: gris `g = (t − 144.676098) / (304.110891 − 144.676098)`, devuelve
>     `Color(g, g, g)` (fuera de [0,1] lo recorta el clamp): pared trasera casi
>     blanca, esferas en gris medio, esquinas cercanas oscuras.
> (4) Paraleliza el bucle de renglones con OpenMP manteniendo la imagen determinista
> (idéntica bit a bit con 1 o N hilos).
> No modifiques la escena, la cámara, la resolución (1024×768) ni la salida
> (`image.ppm` P3 ASCII, gamma 2.2). Genera `image-normal.ppm` e
> `image-distance.ppm` (copia del `image.ppm` de cada modo). Antes de dar por
> terminado, `make && bash sdd/phase-00/check.sh` debe quedar verde.

## Entregables

- `rt.cpp` completado (en la rama de la condición que corresponda del repo
  `aitl-raytracer`, o aquí en `sdd/` si la corrida es exploratoria).
- `image-normal.ppm` e `image-distance.ppm` generados (renombrar el `image.ppm` de
  cada modo), comparados contra las referencias.
- Registro de la corrida: `run_id` de `aitl run --project aitl-raytracer` (C0 con
  `--bare`, C2 sin él) y `aitl run-show <id>` archivado según `MANUAL.md`.
