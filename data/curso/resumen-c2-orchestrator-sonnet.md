# Resumen del curso — celda `c2-orchestrator@sonnet`

> Condición: harness Max-Capacity (el loop del harness, con modelo local de LM
> Studio, actúa como orquestador y delega en sub-agentes `run-host`).
> Tiempo total usado: **11 s**. Resultado global: **gate de fase 0 en rojo** —
> el curso es secuencial, no se avanza sobre una base rota.

## 1. Tabla por fase

| Fase | Proyecto | run_id | Gate | Duración |
|---|---|---|---|---|
| 0 | Proyecto 1 — intersección rayo-esfera + renders de depuración | — (sin id) | ✗ fail | 9 s |
| 1 | Proyecto 2, parte 1 — iluminación Monte Carlo | no ejecutada | — | — |
| 2 | Proyecto 2, parte 2 — fuente puntual | no ejecutada | — | — |
| 3 | Proyecto 3 — emisores de área | no ejecutada | — | — |
| 4 | (pendiente en la especificación del curso) | no ejecutada | — | — |
| 5 | (pendiente en la especificación del curso) | no ejecutada | — | — |

## 2. Hasta qué proyecto se llegó y por qué se detuvo el curso

No se completó ningún proyecto. El curso se detuvo en la **fase 0 (Proyecto 1)**:
el gate objetivo de la fase (`make && bash sdd/phase-00/check.sh`, que a su vez
corre `sdd/phase-00/probe.py`) quedó en rojo. Como el protocolo del curso es
**secuencial** — no se avanza a la fase siguiente sobre una base que no pasó su
gate — las fases 1 a 5 (Proyectos 2 y 3) no se intentaron.

## 3. Fases completadas

Ninguna fase quedó completada. Al revisar `rt.cpp` en el estado final de la
celda, el archivo es **idéntico al esqueleto base** (`sdd/base/rt.cpp`, 172
líneas): ninguno de los cuatro huecos `PROYECTO 1` que pide la fase 0 fue
llenado:

- `Sphere::intersect` sigue devolviendo `0.0` sin resolver la ecuación
  cuadrática rayo-esfera (RF-1).
- La función global `intersect(r, t, id)` sigue devolviendo `false` sin
  recorrer `spheres[]` (RF-2).
- `shade` no calcula el punto de intersección `x`, la normal `n` ni
  `colorValue` — sigue devolviendo un `Color()` (negro) vacío (RF-3).
- El bucle de píxeles en `main` no tiene `#pragma omp parallel for` (RF-4).

No hay evidencia (en la duración de 9 s ni en el diff commiteado) de que se
haya ejecutado `make` o `sdd/phase-00/check.sh` antes de reportar el resultado
como `done`. Verificación: inspección directa de `rt.cpp` contra
`sdd/phase-00/spec.md` y comparación byte a byte con la copia de evidencia
`data/runs/fase0-c2-orchestrator@sonnet-sin-id.rt.cpp` — ambas coinciden con el
esqueleto sin resolver.

## 4. Incidencias y decisiones técnicas

- **Primer intento inválido (15 s, commit `56dcc42`).** El orquestador local
  activo no fue el definido en `conditions/c2-orchestrator/MODELO.md`
  (`nemotron-orchestrator-8b` o `qwen/qwen3-4b-2507` como orquestador), sino
  `qwen3-4b` leído desde `~/.aitl/config.json` — el modelo pensado para el rol
  de *agente de trabajo*, no el de orquestador. Esa fila se retiró de
  `data/metricas.csv` (commit `719f019`) por no ser comparable.
- **Corrección del arranque (commit `c682af8`).** Se mejoró el chequeo de
  orquestación de LM Studio y se aisló el patrón glob de `git add` para evitar
  fallos al commitear evidencia.
- **Segundo intento, el que cuenta (9 s, commit `6573bd9`).** Con el
  orquestador corregido, el loop reportó `status=done` en 9 s pero sin ningún
  cambio de código sobre el esqueleto base, y sin `run_id` registrado (columna
  vacía, evidencia archivada como "sin-id"). Esto indica que el orquestador
  cerró el loop sin haber delegado en un sub-agente `run-host` que
  efectivamente escribiera C++, o delegó pero el sub-agente no llegó a tocar
  `rt.cpp` en ese tiempo.
- El repositorio venía con renders (`image.ppm`, `image-normal.ppm`,
  `image-distance.ppm`) de 1024×768 ya presentes en `start/`; son evidencia
  física de celdas previas (`.ppm` está gitignored y no viaja con
  `git checkout` entre ramas/fases — ver ADR 0004) y no corresponden a esta
  celda, cuyo `rt.cpp` nunca llegó a generar un render válido.

## 5. Autoevaluación

Con más tiempo/margen de diagnóstico, lo que cambiaría en el diseño de esta
condición:

- Que el orquestador **no reporte `done`** sin verificar que el gate objetivo
  (`make && bash sdd/phase-0N/check.sh`) corrió realmente y quedó verde —
  aquí bastaron 9 s para declarar cierre sin tocar código.
- Registrar explícitamente cuándo el orquestador **no delega** ningún
  sub-agente `run-host` en una fase, en vez de dejarlo implícito en un
  `run_id` vacío — sería una incidencia de primera clase, no un detalle que
  hay que reconstruir por inspección de `rt.cpp`.
- Verificar el modelo activo (`aitl models`) al inicio de cada celda antes de
  lanzar el loop, para no repetir el error del primer intento (orquestador
  equivocado tomado de `~/.aitl/config.json`).
