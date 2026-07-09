# CLAUDE.md — aitl-raytracer (condición C2-memoria: harness como conocimiento)

## Identidad del proyecto (leer primero)

Este repo es el laboratorio experimental del harness AITL: un raytracer C++/OpenMP
(curso IA7200-L) desarrollado por Spec Driven Development en fases. El estado durable
(memoria, ADRs, skills, prompts) vive en el MCP `aitl-js` — **úsalo**.

| Campo | Valor |
|-------|-------|
| **Clave MCP canónica del proyecto** | `aitl-raytracer` |
| **Software (jerarquía MCP)** | `raytracer` |

**Regla para toda llamada al MCP `aitl-js`:** pasa `project: "aitl-raytracer"`.
NO uses `raytracer`, `rt` ni otra variante — fragmentan la historia. El ledger de ADRs
de este proyecto es PROPIO (empieza en 0001).

## Cómo usar el harness en esta condición

- **Antes de diseñar**: consulta `list_decisions` (ADRs de fases previas) y
  `search_memory` — las decisiones ya tomadas mandan; no las re-derives.
- **Al decidir arquitectura**: regístrala con `record_decision` (siguiente id libre
  leído de la colección) y refleja el markdown en `docs/adr/`.
- **Al aprender algo durable** (bug recurrente, convención, truco de build):
  `write_memory`.
- En esta condición el harness es **capa de conocimiento**: NO lances sub-agentes ni
  delegues la tarea — tú implementas.

## Reglas de trabajo

- **Lee la spec de la fase antes de escribir código** (`sdd/phase-0N/spec.md`, p. ej.
  `sdd/phase-00/spec.md`). Si la spec no existe o está vacía, DETENTE y pídela.
- Implementa exactamente lo que la spec define — no toques escena, cámara ni parámetros
  que la spec no pida cambiar.
- `make` debe compilar sin errores (y sin warnings nuevos con `-Wall`) antes de dar la
  tarea por terminada; si la fase trae `check.sh`,
  `make && bash sdd/phase-0N/check.sh` debe quedar verde.
- El render sale como `image.ppm` en la raíz del proyecto (gitignored); renómbralo
  como pida la spec (p. ej. `image-normal.ppm`).
