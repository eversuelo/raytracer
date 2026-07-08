# Resumen de métricas — experimento raytracer (C0 vs C2)

> Generado por `scripts/resumen.sh` el 2026-07-07T14:44:07-06:00 — **no editar a mano**.
> Fuente única: `data/metricas.csv` (última corrida por celda) + crudos JSON en `data/runs/`.
> `costo_pond` = costo comparable en USD (Cara B: host_meta.cost_usd real de Claude).

## Matriz por celda (fase × celda)

| Fase | Celda | run_id | status | gate | dur_s | iters | tok_out | tok_total | costo_pond (USD) | verify 1er | imagen_ok | ADR |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 0 | c2-memory@sonnet | 16de10c9… | done | ok | 162 | 18 | 8915 | 769494 | 0.6077 | — | — | — |
| 1 | c2-memory@sonnet | 7b4780dc… | done | ok | 698 | 41 | 45479 | 2880108 | 2.248 | — | — | — |
| 2 | c2-memory@sonnet | fabc15cd… | done | ok | 327 | 29 | 18007 | 1607965 | 1.4111 | — | — | — |
| 3 | c2-memory@sonnet | 70b39c38… | done | ok | 693 | 32 | 46337 | 2740608 | 2.2511 | — | — | — |

## Deltas C0 − C2 por fase y modelo (positivo = el harness ahorró)

| Fase | Modelo | Δdur_s | Δiters | Δcosto_pond (USD) |
|---|---|---|---|---|
| 0 | sonnet | — | — | — |
| 1 | sonnet | — | — | — |
| 2 | sonnet | — | — | — |
| 3 | sonnet | — | — | — |

## Recordatorios de validez

- Set mínimo a propósito: `tok_input` crudo (no comparable por caché) queda solo en
  los JSON de `data/runs/`; aquí manda `costo_pond` (USD real de Cara B).
- Corridas degradadas (Mongo caído): run_id vacío pero status/gate/dur_s registrados.
- Columnas de juicio (`verify_1er`, `imagen_ok`, `ADR`) se editan en el CSV, no aquí.
