# Resumen de métricas — experimento raytracer (C0 vs C2)

> Generado por `scripts/resumen.sh` el 2026-07-07T12:53:48-06:00 — **no editar a mano**.
> Fuente: `data/metricas.csv` (última corrida por celda) + crudos en `data/runs/`.
> Definiciones y reglas de comparación: [`METRICAS.md`](METRICAS.md) (M1–M61).

## Matriz por celda (fase × condición)

| Fase | Cond | run_id | status | dur_ms (M1) | iters (M5) | tools (M6) | tok_out (M26) | tok_total (M29) | gates (M7) | superv_min (M10) | verify 1er (M52) | imagen_ok (M44) | ADR |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|

## Deltas C0 − C2 por fase (M49–M51; positivo = el harness ahorró)

| Fase | Δdur vs c2-memory | Δdur vs c2-orch | Δiters vs c2-memory | Δiters vs c2-orch | Δtok_total vs c2-memory | Δtok_total vs c2-orch |
|---|---|---|---|---|---|---|
| 0 | — | — | — | — | — | — |
| 1 | — | — | — | — | — | — |
| 2 | — | — | — | — | — | — |
| 3 | — | — | — | — | — | — |
| 4 | — | — | — | — | — | — |
| 5 | — | — | — | — | — | — |

## Recordatorios de validez

- `tokens_input` crudo no aparece aquí a propósito: no es comparable entre
  condiciones (caché). Usar las vistas M26–M29 desde los crudos de `data/runs/`.
- Celdas con corridas invalidadas: la historia queda en `data/metricas.csv`;
  esta tabla muestra la última corrida por celda.
- Columnas manuales (`verify_1er`, `imagen_ok`, `ADR`) se editan en el CSV,
  no aquí — este archivo se regenera.
