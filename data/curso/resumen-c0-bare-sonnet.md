# Resumen del curso — celda c0-bare@sonnet

Reconstrucción a partir de `git log --oneline`, las specs en `sdd/`, los gates
(`check.sh`/`probe.py`) y el estado actual de `rt.cpp` en este working tree.

## 1. Tabla por fase

| Fase | Proyecto (curso IA7200-L) | run_id | Gate | Duración |
|---|---|---|---|---|
| 0 | Proyecto 1 — intersección rayo-esfera y renders de depuración (normal/distancia) | `a6012b9d-dd36-4eb9-b348-29cec5581595` | ✅ ok | 105 s |
| 1 | Proyecto 2, parte 1 — iluminación directa Monte Carlo (emisor esférico, 3 muestreos) | *(sin registrar)* | ❌ fail | 32 s |

Tiempo total usado por el curso: **141 s** (de un tope de 60 min).

## 2. Hasta dónde llegó el curso y por qué se detuvo

El curso completó **Proyecto 1 (fase 0)** con gate en verde y se detuvo al intentar
**Proyecto 2 parte 1 (fase 1)**: el gate de esa fase quedó en rojo. `run-course.sh` es
secuencial por diseño — no arranca la fase siguiente si la anterior no cerró en verde —
así que el curso terminó ahí sin llegar a fases 02–05.

Evidencia de que la fase 1 no llegó a implementarse: `rt.cpp` en este directorio es
byte a byte el mismo que se commiteó para la fase 0 (commit `313e36a`, `git status`
limpio) — sigue teniendo únicamente los dos modos de depuración `normal`/`distance` de
`argv[1]`, sin el cuarto parámetro `Color ke` de `Sphere`, sin estimador Monte Carlo ni
muestreadores. La imagen `image-uniformsphere32.ppm` que quedó del intento de fase 1 es
**idéntica** (`cmp` exacto) a `image-normal.ppm`: al no reconocer `"uniformsphere"` como
argumento, `rt.cpp` cayó por defecto en `MODE_NORMAL`, es decir la fase 1 nunca modificó
el binario antes de que el gate la evaluara y fallara contra las referencias del
profesor.

## 3. Fases completadas: qué se implementó y cómo se verificó

### Fase 0 — Proyecto 1 (gate ok)

Se completaron los cuatro bloques `PROYECTO 1` de `sdd/base/rt.cpp`:

1. **`Sphere::intersect`**: resuelve `|o+t·d−p|²=r²` con `oc = o−p`, `b = oc·d`,
   `det = b² − (oc·oc − r²)`; si `det<0` devuelve `0.0`, si no prueba primero
   `t1 = −b−√det` y luego `t2 = −b+√det`, quedándose con la primera raíz `> 1e-4`.
2. **`intersect(r, t, id)`** global: recorrido lineal de `spheres[]` quedándose con la
   intersección válida más cercana (`t` y `id` de salida), devuelve `bool`.
3. **`shade`** con dos modos seleccionados por `argv[1]`: `normal` (`color = obj.c + n`,
   sin reescalar) y `distance` (`gray = (t−144.676098)/(304.110891−144.676098)`, con las
   dos constantes de la spec fijas en el código).
4. **Paralelización**: `#pragma omp parallel for schedule(dynamic, 1)` sobre el bucle de
   renglones (`y`), cada hilo escribe filas disjuntas de `pixelColors` → determinista.

Verificación: `make && bash sdd/phase-00/check.sh`, que corre los 5 criterios de
aceptación de `sdd/phase-00/spec.md` — determinismo (`cmp -s` entre corridas repetidas y
entre `OMP_NUM_THREADS=1` vs. default), que `normal` y `distance` produzcan imágenes
distintas, presencia de `#pragma omp parallel` y del `delete[]` original, y
`sdd/phase-00/probe.py` (14 sondas analíticas de color con tolerancia ±10/255 tras
gamma, más gris monótono y siluetas visibles en modo distancia). Los 5 criterios
quedaron en verde; se re-ejecutó el gate durante esta sesión y sigue en verde. Entregables
`image-normal.ppm` e `image-distance.ppm` generados como copia de cada modo.

### Fase 1 — Proyecto 2 parte 1 (gate fail)

No hay nada que reportar como implementado: el código sigue siendo el de la fase 0. El
gate falló porque las comparaciones contra las referencias del profesor
(`sdd/tools/compare-ref.py` en `sdd/phase-01/check.sh`) no pueden pasar cuando el binario
ni siquiera reconoce el sampler ni produce iluminación Monte Carlo.

## 4. Incidencias y decisiones técnicas

- **Diseño secuencial del curso**: `run-course.sh` no avanza a la fase N+1 si la fase N
  no cerró con gate verde — evita construir sobre una base rota. Es la causa directa de
  que el curso se detuviera en fase 1 sin intentar 02–05.
- **`run_id` ausente en fase 1**: a diferencia de fase 0, la fila de fase 1 no tiene
  `run_id` registrado en el ledger, con una duración de solo 32 s — muy por debajo de lo
  que toma implementar el estimador Monte Carlo, el RNG por hilo y los tres
  muestreadores de la spec. Esto sugiere que la corrida terminó (por error o aborto) antes
  de que el pipeline alcanzara a registrar el identificador de run, más que una falla de
  diseño en la spec de fase 1 (que llegó aprobada y con prompt embebido idéntico al de
  fase 0).
- **Consistencia del working tree**: `git status` no muestra cambios pendientes en
  `rt.cpp` ni en `sdd/`; el único artefacto del intento de fase 1 es
  `image-uniformsphere32.ppm`, coherente con la hipótesis anterior.

## 5. Autoevaluación

Con más tiempo, lo primero sería diagnosticar **por qué la corrida de fase 1 no dejó
`run_id`** — ese es el bloqueo real para destrabar 02–05, no la dificultad del C++ en sí
(la spec de fase 1 llegó aprobada, con fórmulas y CLI ya especificadas en el prompt
embebido). Después implementaría fase 1 tal como la pide la spec: `Sphere` con `Color ke`,
`shade` devolviendo la emisión directa si el objeto golpeado emite o si no una sola
muestra del estimador `fr⊙Le·(n·wi)/pdf(wi)`, los tres muestreadores con `erand48` por
píxel, y CLI `./rt <sampler> <spp>`. También vigilaría el presupuesto de tiempo del gate
de fase 1: renderiza varias imágenes completas (incluida una a 512 spp con `timeout`
de 600 s cada una), notablemente más caro que los renders casi instantáneos de fase 0,
así que conviene reservarle más margen dentro del tope global de 60 min.
