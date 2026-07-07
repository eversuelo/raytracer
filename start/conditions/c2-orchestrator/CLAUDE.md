# CLAUDE.md — aitl-raytracer (condición C2-orquestador: harness Max-Capacity)

## Identidad del proyecto (leer primero)

Este repo es el laboratorio experimental del harness AITL: un raytracer C++/OpenMP
(curso IA7200-L) desarrollado por Spec Driven Development en fases. En esta condición
el **loop del harness es el orquestador** (modelo local de LM Studio, ver `MODELO.md`
de la plantilla): rutea la tarea, consulta el conocimiento durable y puede delegar en
sub-agentes (`run-host`). Este archivo lo leen los sub-agentes que el orquestador lance.

| Campo | Valor |
|-------|-------|
| **Clave MCP canónica del proyecto** | `aitl-raytracer` |
| **Software (jerarquía MCP)** | `raytracer` |

**Regla para toda llamada al MCP `aitl-js`:** pasa `project: "aitl-raytracer"`.
NO uses `raytracer`, `rt` ni otra variante. El ledger de ADRs es PROPIO (desde 0001).

## Cómo usar el harness en esta condición

- **Antes de diseñar**: consulta `list_decisions` (ADRs de fases previas) y
  `search_memory` — las decisiones ya tomadas mandan; no las re-derives.
- **Al decidir arquitectura**: `record_decision` (siguiente id libre) + espejo en `docs/adr/`.
- **Al aprender algo durable**: `write_memory`.
- Como sub-agente, cíñete a la sub-tarea que el orquestador te delegó; reporta el
  resultado y no expandas el alcance.

## Reglas de trabajo

- **Lee la spec de la fase antes de escribir código** (`sdd/phase-0N/spec.md`, p. ej.
  `sdd/phase-00/spec.md`). Si la spec no existe o está vacía, DETENTE y pídela.
- Implementa exactamente lo que la spec define — no toques escena, cámara ni parámetros
  que la spec no pida cambiar.
- Todo es **C++** compilado con `make`; algunos programas son **terminales
  interactivos** — al automatizar su ejecución alimenta stdin por pipe/heredoc, nunca
  esperes un TTY.
- `make` sin errores (ni warnings nuevos con `-Wall`); si la fase trae `check.sh`,
  `make && bash sdd/phase-0N/check.sh` debe quedar verde.
- El render sale como `image.ppm` en la raíz del proyecto (gitignored); renómbralo
  como pida la spec (p. ej. `image-normal.ppm`).
