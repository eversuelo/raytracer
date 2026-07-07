# Resumen del curso — celda `c0-bare@sonnet`

**Proyecto:** aitl-raytracer · **Condición:** C0-bare (sin memoria persistente ni orquestador) ·
**Modelo:** Claude Sonnet 5 · **Rama:** `curso/c0-bare-sonnet` · **Tope de la celda:** 60 min

## 1. Tabla por fase

| Fase | Proyecto | run_id | Gate | Duración |
|---|---|---|---|---|
| 0 | Proyecto 1 — intersección rayo-esfera + renders de depuración (normal/distance) | `a6012b9d-dd36-4eb9-b348-29cec5581595` | ✓ ok | 105 s |
| 1 (intento 1) | Proyecto 2, parte 1 — iluminación directa Monte Carlo (emisor esférico, 3 muestreadores) | *(sin run_id)* | ✗ fail | 32 s |
| 1 (intento 2) | Proyecto 2, parte 1 — iluminación directa Monte Carlo | *(sin run_id)* | ✗ fail | 31 s |
| 2 (intento 1) | Proyecto 2, parte 2 — fuente puntual (sombras duras) | *(sin run_id)* | ✗ fail | 8 s |
| 2 (intento 2) | Proyecto 2, parte 2 — fuente puntual | *(sin run_id)* | ✗ fail | 8 s |

Tiempo total usado según el resultado global de la celda: **427 s** (~7.1 min de un tope de
60 min). La suma de las cinco duraciones registradas en el CSV (105+32+31+8+8 = 184 s) queda
por debajo de esa cifra global — ver la discrepancia anotada en §4.

## 2. Hasta dónde llegó el curso y por qué se detuvo

Se completó únicamente el **Proyecto 1** (fase 0), con gate verde. El **Proyecto 2** quedó
en rojo en sus dos partes: la parte 1 (fase 1, iluminación directa por Monte Carlo) falló dos
veces, y la parte 2 (fase 2, fuente puntual) —intentada pese al fallo previo de la fase 1—
también falló dos veces. Como el curso avanza de forma **secuencial** (`run-course.sh` no
promueve a la fase siguiente si la actual no pasa su gate, para no construir sobre una base
rota), el resultado global de la celda reporta el corte en la fase 2: *"gate de la fase 2 en
rojo — el curso es secuencial, no se avanza sobre base rota"*. Las fases 3 a 5 (muestreo de
importancia de fuentes, aceleración/BVH/AA/texturas, path tracing) nunca se intentaron.

## 3. Fases completadas: qué se implementó y cómo se verificó

### Fase 0 — Proyecto 1 (único gate en verde)

Se completaron los cuatro bloques `PROYECTO 1` de `sdd/base/rt.cpp`:

- `Sphere::intersect`: resolución de `|o + t·d − p|² = r²` con `oc = o−p`, `b = oc·d`,
  `det = b² − (oc·oc − r²)`; sin raíz real → `0.0`; si no, la menor raíz positiva
  `t = −b ± √det` con épsilon `1e-4` para evitar auto-intersección.
- `intersect(r, t, id)`: recorrido lineal de `spheres[]` quedándose con el hit válido más
  cercano.
- `shade`: dos modos por argumento CLI (`./rt normal` / `./rt distance`) — normales
  `obj.c + n` sin reescalar, y distancia como gris
  `(t − 144.676098)/(304.110891 − 144.676098)`.
- Paralelización del bucle de renglones con `#pragma omp parallel for schedule(dynamic, 1)`,
  sin estado compartido mutable entre píxeles (determinista con 1 o N hilos).

Verificación: `make && bash sdd/phase-00/check.sh`, que cubre los 5 criterios de aceptación
de la spec — determinismo (`cmp` bit a bit entre corridas y entre `OMP_NUM_THREADS=1` vs.
default), que `normal` y `distance` produzcan imágenes distintas, presencia de
`#pragma omp parallel` y del `delete[]` original, y sondeo analítico de píxeles
(`sdd/phase-00/probe.py`, tolerancia ±10/255 tras gamma) contra los valores esperados de la
escena de referencia. Gate: **ok**, run_id `a6012b9d-dd36-4eb9-b348-29cec5581595`, 105 s.

### Fases no completadas (para contexto)

**Fase 1** (dos intentos, ambos fail): el `rt.cpp` que queda en el repo al final de la celda
sí implementa el alcance completo de la spec — `Sphere` con emisión `ke`, estimador Monte
Carlo de un rebote (`fr·Le·cosθ/pdf`), los tres muestreadores (uniforme esférico, uniforme
hemisférico, coseno hemisférico) con su base ortonormal, CLI `./rt <sampler> <spp>` y RNG
`erand48` sembrado determinísticamente por índice de píxel — pero el gate objetivo
(`sdd/phase-01/check.sh`) no llegó a quedar verde en ninguno de los dos intentos. No se
volvió a ejecutar el gate desde esta sesión (la tarea pide no modificar archivos), así que no
se puede señalar aquí el criterio numérico exacto que lo tumbó; por la estructura del código
es más probable un fallo de **tolerancia** (media de color / varianza / convergencia contra
la referencia) que un fallo estructural — ver §4.

**Fase 2** (dos intentos, ambos fail, 8 s cada uno): el `rt.cpp` de evidencia de esta fase es
**byte-idéntico** al de la fase 1 (mismo hash) — no se agregó código para la fuente puntual
(`./rt point`, contribución `(albedo/π)·I·cosθ/r²`, rayo de sombra). Con el binario de la
fase 1, `./rt point` cae en la rama `else` del selector de muestreador y corre
`cosinehemi` a 32 spp por defecto, así que el gate falla rápido en el criterio de
comparación contra `image-plight.jpg` (consistente con los 8 s de duración, muy por debajo de
los 31-32 s que tomó la fase 1). Es decir: la fase 2 no llegó a tener una implementación
propia que evaluar.

## 4. Incidencias y decisiones técnicas relevantes

- **Curso secuencial confirmado**: el propio resultado global de la celda señala el corte en
  la fase 2 (no en la 1), lo que indica que la celda sí reintentó avanzar más allá del primer
  fallo de la fase 1 antes de detenerse definitivamente.
- **Fase 1 sin `run_id` en ambos intentos**, pese a que en el estado final del repo sí hay una
  implementación sustancial (estimador MC + 3 muestreadores + RNG por hilo). Esto contrasta
  con corridas previas de esta misma celda en las que se documentó una fase 1 sin cambios de
  código; el estado actual muestra que en algún momento sí se escribió la implementación, pero
  la ausencia de `run_id` en el CSV impide correlacionar qué intento fue cuál.
- **Fase 2 sin implementación propia**: los 8 s por intento y el hash idéntico al `rt.cpp` de
  la fase 1 indican que el agente no llegó a escribir el modo `point` en ninguno de los dos
  intentos — más cercano a una falla temprana de arranque (agente/orquestador) que a un
  intento de implementación rechazado por el gate.
- **Discrepancia de tiempos**: el resultado global reporta 427 s totales, pero la suma de las
  cinco duraciones del CSV es 184 s (una diferencia de ~243 s / 4 min). Vale la pena revisar
  si el agregador de `run-course.sh`/`collect-metrics` está sumando tiempos de otras etapas
  (preflight, checkout de rama, copiado de evidencia, commits) que no quedan reflejados por
  fase — esta misma discrepancia ya se había señalado en una corrida anterior de esta celda
  (con otras cifras) y sigue sin resolverse.
- **Logs fuera de alcance**: existe un `runshow.txt` de un intento de la fase 1
  (`data/runs/fase1-c0-bare@sonnet-633383e0-…`) y evidencia cruda en
  `data/curso/evidencia/c0-bare-sonnet/`, pero ambos quedan fuera del directorio de trabajo
  permitido para este agente (`start/`), por lo que no se pudieron inspeccionar desde aquí
  para diagnosticar la causa raíz de los fallos de las fases 1 y 2.
- **Permisos de la celda** (`.claude/settings.json`): modo `acceptEdits` con allowlist de
  Bash acotada a herramientas de build/verificación (`make`, `g++`, `python3`, `cmp`, `diff`,
  etc.) y deny explícito de lectura/mención de `../objective/` (donde vive la solución del
  investigador) — coherente con que el agente no debe ver la referencia antes de implementar.

## 5. Autoevaluación breve

Con más tiempo/visibilidad, lo primero sería instrumentar las fases 1 y 2 para que, aun si el
gate falla, quede capturado un `run_id` y el stdout/stderr del agente — los cuatro intentos
fallidos (dos por fase) no dejan rastro utilizable desde este directorio para diferenciar un
fallo de tolerancia numérica (fase 1) de un fallo de arranque sin implementación (fase 2).
Para la fase 1 en concreto, antes de aceptar el código como terminado habría valido la pena
un chequeo de humo (`./rt uniformsphere 1` sin crashear, media de color aproximada a ojo)
antes de depender solo del gate objetivo con sus tolerancias exactas. Para la fase 2, el
patrón de 8 s por intento sugiere revisar primero si el agente llegó siquiera a leer
`sdd/phase-02/spec.md` antes de que la corrida terminara. Y dado que el curso es estrictamente
secuencial, un fallo no diagnosticado en una fase temprana tiene un costo alto porque bloquea
todas las fases restantes de la escalera — priorizaría cerrar ese gap de observabilidad antes
de reintentar la celda.
