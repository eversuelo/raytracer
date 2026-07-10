# Resumen de métricas — experimento raytracer (C0 vs C2)

> Generado por `scripts/resumen.sh` el 2026-07-09T22:11:36-06:00 — **no editar a mano**.
> Fuente única: `data/metricas.csv` (última corrida por celda) + crudos JSON en `data/runs/`.
> `costo_pond` = costo comparable en USD (Cara B: host_meta.cost_usd real de Claude).

## Matriz por celda (fase × celda)

| Fase | Celda | run_id | status | gate | dur_s | iters | tok_out | tok_total | costo_pond (USD) | verify 1er | imagen_ok | ADR |
|---|---|---|---|---|---|---|---|---|---|---|---|---|

## Deltas C0 − C2 por fase y modelo (positivo = el harness ahorró)

| Fase | Modelo | Δdur_s | Δiters | Δcosto_pond (USD) |
|---|---|---|---|---|

## Recordatorios de validez

- Set mínimo a propósito: `tok_input` crudo (no comparable por caché) queda solo en
  los JSON de `data/runs/`; aquí manda `costo_pond` (USD real de Cara B).
- Corridas degradadas (Mongo caído): run_id vacío pero status/gate/dur_s registrados.
- Columnas de juicio (`verify_1er`, `imagen_ok`, `ADR`) se editan en el CSV, no aquí.
