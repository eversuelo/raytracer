# Resumen del curso — celda `c0-bare@sonnet`

**Proyecto:** aitl-raytracer · **Condición:** C0-bare (sin memoria persistente ni orquestador) · **Modelo:** Claude Sonnet 5
**Rama:** `curso/c0-bare-sonnet` · **Tope de la celda:** 60 min

## 1. Tabla por fase

| Fase | Proyecto | run_id | Gate | Duración |
|---|---|---|---|---|
| 0 | Proyecto 1 — intersección rayo-esfera + renders de depuración (normal/distance) | `a6012b9d-dd36-4eb9-b348-29cec5581595` | ✓ ok | 105 s |
| 1 (intento 1) | Proyecto 2, parte 1 — iluminación directa Monte Carlo | *(sin run_id)* | ✗ fail | 32 s |
| 1 (intento 2) | Proyecto 2, parte 1 — iluminación directa Monte Carlo | *(sin run_id)* | ✗ fail | 31 s |

Suma de duraciones registradas: 168 s (~2.8 min) de un tope de 60 min — muy por debajo del
límite global; el curso no se detuvo por tiempo sino por el gate de la fase 1 (ver §4).

## 2. Hasta dónde llegó el curso y por qué se detuvo

Se completó únicamente el **Proyecto 1** (fase 0), con gate verde. El **Proyecto 2, parte 1**
(fase 1: iluminación directa por Monte Carlo con emisor esférico y tres muestreadores) quedó
en rojo tras dos intentos consecutivos. Como el curso avanza de forma **secuencial** —
`run-course.sh` no promueve a la fase siguiente si la fase actual no pasa su gate, para no
construir sobre una base rota — el curso se detuvo ahí: las fases 2 a 5 (luces puntuales,
muestreo de importancia, BVH/AA/texturas, path tracing) nunca se intentaron.

## 3. Fases completadas: qué se implementó y cómo se verificó

### Fase 0 — Proyecto 1 (único gate en verde)

Se completaron los cuatro bloques `PROYECTO 1` de `sdd/base/rt.cpp`:

- `Sphere::intersect`: resolución de `|o + t·d − p|² = r²` con `oc = o−p`, `b = oc·d`,
  `det = b² − (oc·oc − r²)`; sin raíz real → `0.0`; si no, la menor raíz positiva
  `t = −b ± √det` con épsilon `1e-4` para evitar auto-intersección.
- `intersect(r, t, id)`: recorrido lineal de `spheres[]` quedándose con el hit válido más
  cercano.
- `shade`: dos modos por argumento CLI (`./rt normal` / `./rt distance`) — normales
  `obj.c + n` sin reescalar, y distancia como gris `(t − 144.676098)/(304.110891 − 144.676098)`.
- Paralelización del bucle de renglones con `#pragma omp parallel for schedule(dynamic, 1)`,
  sin estado compartido mutable entre píxeles (determinista con 1 o N hilos).

Verificación: `make && bash sdd/phase-00/check.sh`, que cubre los 5 criterios de aceptación
de la spec — determinismo (`cmp` bit a bit entre corridas y entre `OMP_NUM_THREADS=1` vs
default), que `normal` y `distance` produzcan imágenes distintas, presencia de
`#pragma omp parallel` y del `delete[]` original, y sondeo analítico de píxeles
(`sdd/phase-00/probe.py`, tolerancia ±10/255 tras gamma) contra los valores esperados de la
escena de referencia. Gate: **ok**, run_id `a6012b9d-dd36-4eb9-b348-29cec5581595`, 105 s.

### Fase 1 — Proyecto 2 parte 1 (no completada)

`rt.cpp` en esta rama es **byte-idéntico** al de la fase 0 (`git diff` entre el commit de
fase 0 y el estado actual no muestra cambios en el archivo): la clase `Sphere` no ganó el
campo de emisión `ke`, `shade` no implementa el estimador Monte Carlo, y no existe el CLI
`./rt <sampler> <spp>` que pide la spec. Es decir, la implementación de iluminación directa
nunca llegó a escribirse en esta rama — no es un caso de "código incorrecto que no pasó el
gate", sino de que el gate de fase 1 nunca vio código nuevo que evaluar.

No hay verificación que reportar para esta fase: `sdd/phase-01/check.sh` (reproducibilidad
RNG, comparación de los 3 muestreadores a 32 spp contra referencia, convergencia a 512 spp,
orden de varianza cosinehemi < uniformhemi < uniformsphere) no llegó a ejecutarse sobre una
implementación real.

## 4. Incidencias y decisiones técnicas relevantes

- **Curso secuencial confirmado**: el propio resultado global de la celda indica "el curso
  es secuencial, no se avanza sobre base rota" — el gate de fase 1 en rojo bloqueó el acceso
  a fases 2-5 en esta corrida.
- **Fase 1 falló sin generar `run_id` en ninguno de los dos intentos**, con duraciones muy
  cortas (31-32 s) para el alcance de la fase (estimador Monte Carlo + 3 muestreadores + RNG
  por hilo + 9 combinaciones sampler×spp). Combinado con que `rt.cpp` no cambió, esto apunta
  a una falla temprana en el arranque de esa corrida (agente u orquestador) más que a un
  intento de implementación que luego fue rechazado por el gate. Los logs de esas corridas
  están en `data/curso/evidencia/c0-bare-sonnet/fase-1/`, fuera del directorio de trabajo
  permitido para este agente (`start/`), por lo que no se pudieron inspeccionar desde aquí.
- **Posible discrepancia de métricas**: el mensaje de resultado global reporta "tiempo total
  usado: 32 s", pero la suma real de las tres duraciones registradas en el CSV
  (`fases-c0-bare-sonnet.csv`) es 105 + 32 + 31 = 168 s. Vale la pena revisar si el agregador
  de `run-course.sh` / `collect-metrics` está tomando solo la duración del último intento en
  vez de la suma acumulada.
- **Permisos de la celda** (`.claude/settings.json`): modo `acceptEdits` con allowlist de
  Bash acotada a herramientas de build/verificación (`make`, `g++`, `python3`, `cmp`, `diff`,
  etc.) y deny explícito de lectura/mención de `../objective/` (donde vive la solución del
  investigador) — coherente con que el agente no debe ver la referencia antes de implementar.

## 5. Autoevaluación breve

Con más tiempo/visibilidad, lo primero sería instrumentar fase 1 para que, aun si el gate
falla, queden capturados stdout/stderr y un `run_id` de la corrida — los dos intentos
actuales no dejan rastro utilizable para diagnosticar si el fallo fue de arranque, de
permisos, o del propio modelo. Antes de escribir el estimador Monte Carlo completo, valdría
la pena un chequeo de humo mínimo (`make` limpio + `./rt uniformsphere 1` sin crashear) para
aislar errores de compilación/CLI de errores de lógica de muestreo. Y dado que el curso es
estrictamente secuencial, un fallo no diagnosticado en una fase temprana tiene un costo alto
(bloquea las cuatro fases restantes de la escalera), así que priorizaría cerrar ese gap de
observabilidad antes de reintentar la celda.
