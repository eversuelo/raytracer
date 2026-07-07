# Resumen final del curso — matriz condición × modelo (PARCIAL: 1 de 4 celdas)

> ⚠️ **Este resumen es preliminar.** Solo hay una celda corrida completa
> (`c0-bare@sonnet`). Faltan `c0-bare@opus`, `c2-memory@sonnet` y `c2-memory@opus`
> (ver `GUIA-CURSO.md`). Cuando esas tres terminen, vuelve a pedir este análisis para
> completar la comparación real C0 vs C2 y sonnet vs opus — este archivo se
> reemplazará entonces por la versión completa.

## 1. Estado de la matriz

| Celda | Estado |
|---|---|
| `c0-bare@sonnet` | ✅ corrida completa — detenida en fase 5 (gate rojo) |
| `c0-bare@opus` | ⬜ no corrida |
| `c2-memory@sonnet` | ⬜ no corrida |
| `c2-memory@opus` | ⬜ no corrida |

## 2. Resultado de `c0-bare@sonnet`

Llegó hasta el **Proyecto 4 (fase 4)** con gate verde en cada fase aceptada; la
**fase 5** (proyecto final, MIS/path tracing explícito) falló el gate y detuvo el
curso por la regla de secuencialidad (no se avanza sobre base rota). El corte fue por
**fallo de gate, no por tope de tiempo**: 1322 de 3600 s usados (~37%).

| Fase | Proyecto | Intentos | Gate final | Dur. (aceptado/último intento) |
|---|---|---|---|---|
| 0 | Intersección + debug | 1 | ✅ ok | 105 s |
| 1 | MC directa, emisor esférico | 3 (2 fail + 1 ok) | ✅ ok | 43 s |
| 2 | Fuente puntual | 1 | ✅ ok | 234 s |
| 3 | IS de fuentes + múltiples emisores | 1 | ✅ ok | 450 s |
| 4 | Microfacets (conductores ásperos) | 1 | ✅ ok | 338 s |
| 5 | MIS β=2 / path tracing explícito | 1 | ❌ fail | 141 s |

**Totales (fases 0–5, incluye el intento fallido de fase 5; excluye los 2 intentos
fallidos de fase 1 porque `data/metricas.csv` no les asignó fila propia):**

- Duración agregada de agente: **1 294 683 ms (~21.6 min)**, de 3600 s de tope.
- Iteraciones (M5): 18 + 6 + 32 + 25 + 31 + 1 = **113**.
- Tokens totales (M29): **≈ 6.79 M** (806 823 + 169 676 + 1 685 839 + 1 805 280 +
  2 282 625 + 39 560).
- Costo (`cost_usd` de cada `runshow`, no está en `metricas.csv` — se tomó de
  `data/runs/`): **≈ US$5.66** sumando las 6 fases registradas. No incluye el costo de
  los 2 intentos fallidos de fase 1 (sin `run_id`/`runshow` capturado) ni el costo de
  las 2 corridas de "resumen de la celda" repetidas por interrupciones manuales
  (~US$0.65 + US$0.88 adicionales, fuera del alcance de "métricas del curso").

## 3. Pendiente antes del análisis comparativo real

1. Correr las 3 celdas restantes: `scripts/run-course.sh c0-bare opus`,
   `scripts/run-course.sh c2-memory sonnet`, `scripts/run-course.sh c2-memory opus`.
2. Completar a mano en `data/metricas.csv` las columnas `verify_1er_intento`,
   `imagen_ok`, `adr` — hoy están vacías en las 6 filas existentes.
3. Volver a pedir el resumen comparativo final (tabla condición × modelo completa) y
   commitear `data/` en `main`.

## 4. Hallazgo aparte: `scripts/resumen.sh` genera la matriz vacía

`RESUMEN-METRICAS.md` (la tabla mecánica por fase×celda) sale **vacía** — no es un
problema de datos, es un bug: el script busca `cond` en `{c0-bare, c2-memory,
c2-orchestrator}` (sin sufijo de modelo), pero `data/metricas.csv` guarda `cond` como
`c0-bare@sonnet`/`c0-bare@opus`/etc. (tal como documenta `GUIA-CURSO.md`). Como
ninguna clave calza, la tabla y los deltas quedan en blanco aunque haya 6 filas de
datos reales. No lo toqué — avísame si quieres que lo corrija.
