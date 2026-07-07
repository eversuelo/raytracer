# ADR-0004 — Repo oficial de métricas: start/objective, C2 desdoblada y pipeline automatizado

- **Estado**: accepted · 2026-07-06
- **Proyecto MCP**: `aitl-raytracer` (versión durable en la colección `decisions`)
- **Componentes**: `scripts/`, `start/conditions/`, `objective/`, `data/`, `METRICAS.md`, `start/sdd/TRAZABILIDAD.md`
- **Enmienda a**: ADR-0001 (ramas), ADR-0003 (diseño de celdas)

## Contexto

`metricas/raytracer/` pasó a ser el repo git principal del experimento (único con
ramas; commit `21bb142`), incorporando `objective/` (programas C++ del investigador que
generan los renders de referencia, versionados `v(Proyecto).(Mat).(Código)`) y `start/`
(punto de partida común de los agentes, specs SDD en `start/sdd/`). Se pidió que C2
tuviera dos modos (orquestador con sub-agentes vs solo memoria/skills/ADR), test
automatizado por script con README de medición, y resumen markdown generado. Todo es
C++ y algunos programas son terminales interactivos.

## Decisión

1. **Un solo repo con ramas**: `main` = specs+protocolo+scripts; una rama por celda
   `fase-N/{c0-bare,c2-memory,c2-orchestrator}` → **21 celdas** mínimas.
2. **Tres roles**: `start/` (de dónde parte el agente), `objective/` (a dónde debe
   llegar — VEDADO a agentes; comparación por hash/RMS vía `check.sh`), `data/`+ramas
   (evidencia).
3. **C2 desdoblada**: `c2-memory` (harness como conocimiento) y `c2-orchestrator`
   (Max-Capacity: `aitl run --model lmstudio --mcp`, delega vía `run-host`). C0
   conserva `capture-session` (instrumento, no tratamiento).
4. **Modelo orquestador**: **Nemotron Orchestrator 8B DeepSeek Q4_K_S** — evaluación
   completa en `start/conditions/c2-orchestrator/MODELO.md`.
5. **Pipeline**: `scripts/run-cell.sh` → `scripts/collect-metrics.sh`
   (`data/metricas.csv` + crudos) → `scripts/resumen.sh` (`RESUMEN-METRICAS.md`).
6. **.gitignore** protege credenciales (`.mcp.json`), builds y renders de agentes.

## Consecuencias

- Celda = rama = `run_id` = fila del CSV; los run_ids salen de TRAZABILIDAD.md y viven
  en `data/metricas.csv`.
- 14 → 21 celdas (+50% costo) a cambio de separar efecto conocimiento (H3/H7/H8) del
  efecto orquestación (H2/H11).
- Pendientes: `check.sh` por fase (stdin scripteado — programas interactivos), fijar
  `LMSTUDIO_MODEL` y validar tokens>0 en streaming, variante canónica de v1.0.4 para
  phase-00, archivar los `.ppm` exactos.
