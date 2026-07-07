# objective/ — renders de referencia del experimento (código del investigador)

Programas escritos por el investigador que **generan las imágenes objetivo** de cada
fase: la "verdad terreno" contra la que se evalúa el trabajo de los agentes
(métricas **M44** corrección de imagen, **M45** regresiones visuales y **M47**
convergencia Monte Carlo del catálogo [`../METRICAS.md`](../METRICAS.md)).

> ⛔ **Regla de validez: los agentes NUNCA leen este directorio.** Aquí vive la
> solución. Si un agente lo lee, la celda queda invalidada (anotar con
> `aitl intervene`). La comparación es siempre indirecta: las imágenes generadas aquí
> se copian como referencia a `../start/sdd/phase-0N/` y el `check.sh` de la fase
> compara por dimensiones + hash del PPM (o distancia RMS).

## Esquema de versionado

`v(No.Proyecto).(Cambios Matemáticos).(Cambios de Código)` — cada versión es un
directorio autocontenido con `rt.cpp + Makefile + image.ppm/jpg` (el render que produce).

## Clasificación: proyecto del curso → fase SDD

| Dir | Proyecto (curso IA7200-L) | Fase SDD | Estado |
|---|---|---|---|
| `Proyecto 1/` | Proyecto 1 — intersección, normales, OpenMP, render distancia | **phase-00** | ✅ cargado |
| `Proyecto 2/` | Proyecto 2 — iluminación directa MC + fuentes puntuales | phase-01 · phase-02 | pendiente |
| `Proyecto 3/` | Proyecto 3 — muestreo de importancia de fuentes (2A1P) | phase-03 | pendiente |
| `Proyecto 4/` | Proyecto 4 — microfacets (aluminio/oro α=0.3) | phase-04 | pendiente |
| `Proyecto Final/` | 5a MIS β=2 · 5b Path Tracing explícito | phase-05 | pendiente |

### Proyecto 1 → phase-00 (detalle de versiones)

| Versión | Hito | Aporta a la referencia de phase-00 |
|---|---|---|
| `v1.0.1` | Intersección correcta | geometría base |
| `v1.0.2` | Normal correcta | `image-normalcolor` (render de normales) |
| `v1.0.3` | Paralelización del for (OpenMP) | misma imagen, tiempo de render (M48) |
| `v1.0.4 Artesanal` / `v1.0.4 Refinado` | Render de la distancia (dos variantes de implementación) | `image-distance` |

Las referencias publicadas en `../start/sdd/phase-00/` (`image-distance.jpg`,
`image-normalcolor.jpg`) provienen de esta serie. **Pendiente**: fijar cuál variante de
`v1.0.4` es la referencia canónica del `check.sh` de phase-00 y archivar el `.ppm`
exacto (el hash se calcula sobre el PPM, no el JPG).

## Flujo de una referencia nueva

1. El investigador implementa/renderiza aquí (`make && ./rt`), versionando según el esquema.
2. El `.ppm` canónico (y su JPG de vista) se copian a `../start/sdd/phase-0N/`.
3. El `check.sh` de la fase (vive con la spec) registra dimensiones + hash esperados.
4. La spec referencia esas imágenes; recién entonces puede aprobarse (`status: approved`).
