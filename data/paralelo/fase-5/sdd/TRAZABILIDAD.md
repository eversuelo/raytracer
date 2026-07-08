# Trazabilidad SDD — con y sin harness (C0 vs C2)

Este documento es el **registro maestro** del experimento: cada fase del temario
(IA7200-L, `themes.pdf`) se desarrolla dos veces — **C0** (modelo solo, `aitl run --bare`)
y **C2** (harness completo) — y aquí queda la cadena verificable
**spec → rama → corrida → métricas → ADR → imagen**.

## Mapa temario → fases SDD

| Fase SDD | Proyecto del curso | Tema | Spec |
|---|---|---|---|
| phase-00 | Proyecto 1 (+ Práctica 1 OpenMP) | Intersección rayo-esfera, renders de depuración (distancia/normales) | `phase-00/spec.md` |
| phase-01 | Proyecto 2 · parte 1 | Iluminación directa Monte Carlo: emisor esférico, difusos, 3 muestreos | `phase-01/spec.md` |
| phase-02 | Proyecto 2 · parte 2 | Fuentes puntuales (I=4000), sombras duras, determinista | `phase-02/spec.md` |
| phase-03 | Proyecto 3 | Muestreo de importancia de fuentes (área / ángulo sólido) + múltiples emisores (2A1P) | `phase-03/spec.md` |
| phase-04 | Proyecto 4 | Microfacets: conductores ásperos (aluminio/oro α=0.3), sample/eval/pdf | `phase-04/spec.md` |
| phase-05 | Proyecto final | 5a: MIS heurística de potencia β=2 · 5b: Path Tracing explícito (10 rebotes) | `phase-05/spec.md` |

## Reglas de trazabilidad (no negociables)

1. **Spec primero**: ninguna corrida arranca con la spec en `status: draft`.
   El humano la aprueba (`status: approved`) y desde ese momento la spec es inmutable
   para esa fase (cambios → nueva versión anotada).
2. **Prompt idéntico**: las tres condiciones reciben el "Prompt de tarea" de la spec,
   textual (`scripts/run-cell.sh` lo extrae de la sección `## Prompt de tarea`).
3. **Ramas gemelas** en el repo raíz `metricas/raytracer`:
   `fase-N/{c0-bare,c2-memory,c2-orchestrator}`, todas nacen del mismo commit (el
   resultado aceptado de la fase N−1). Solo este repo tiene ramas.
4. **Corridas registradas**: cada celda produce un `run_id` durable (lo lanza
   `scripts/run-cell.sh`); sesiones Cara B (Claude Code, método `host`) quedan
   capturadas por el hook `capture-session`. Corridas inválidas se anotan con
   `aitl intervene`, nunca se borran (quedan como historia en `data/metricas.csv`).
5. **Cierre de fase**: ADR en el ledger de `aitl-raytracer` (espejo en `docs/adr/` del
   repo) + fila completada en la matriz de abajo + imágenes archivadas.
6. **Métricas**: `aitl run-show <id>` — iteraciones, tokens ↑↓, tool calls, vetos,
   eventos `verify`/`hydrate`/`compaction`, `supervision_minutes`. En C2 verificar
   (evento `hydrate`) que los ADRs de fases previas entraron al contexto.

## Matriz de corridas (los run_id se llenan solos en `data/metricas.csv`; aquí el veredicto)

Las ramas de cada fila son `fase-N/{c0-bare,c2-memory,c2-orchestrator}`; los `run_id`
por celda y sus métricas viven en `../../data/metricas.csv` (recolector) y el resumen
comparativo en `../../RESUMEN-METRICAS.md` (generado).

| Fase | run_id c0-bare | run_id c2-memory | run_id c2-orchestrator | ADR | Imágenes | Veredicto |
|---|---|---|---|---|---|---|
| 00 | — | — | — | — | `image-{normal,distance}.ppm` | — |
| 01 | — | — | — | — | 9 renders (3 muestreos × 3 spp) | — |
| 02 | — | — | — | — | `image-plight.ppm` | — |
| 03 | — | — | — | — | RC-AreaL-IS* + 2A1P-IS* | — |
| 04 | — | — | — | — | ISBRDF-{32,512,2048} | — |
| 05a MIS | — | — | — | — | comparativa de varianza | — |
| 05b PT | — | — | — | — | PTEXP-{PointL,AreaL,MultiL} | — |

> El protocolo detallado y el mapeo hipótesis→verificación (H1–H11) viven en
> `../../aitl-raytracer/MANUAL.md`. Este archivo solo registra la evidencia.
> El catálogo completo de métricas (M1–M61, mapeo Tabla 4.3 ↔ campos del harness y
> plantilla de hoja por celda) está en [`../METRICAS.md`](../../METRICAS.md).
