# CLAUDE.md — aitl-raytracer (condición C2-local: harness 100 % local)

## Identidad del proyecto (leer primero)

Este repo es el laboratorio experimental del harness AITL: un raytracer C++/OpenMP
(curso IA7200-L) desarrollado por Spec Driven Development en fases. En esta condición
**el loop nativo del harness es el agente completo**: un solo modelo local de LM Studio
(ver `MODELO.md` de la plantilla) implementa las fases con sus tools nativas
(`read_file`, `edit_file`, `write_file`, `shell`) — **no hay sub-agentes Claude Code**.

| Campo | Valor |
|-------|-------|
| **Clave MCP canónica del proyecto** | `aitl-raytracer` |
| **Software (jerarquía MCP)** | `raytracer` |

**Regla para toda llamada al MCP `aitl-js`:** pasa `project: "aitl-raytracer"`.
NO uses `raytracer`, `rt` ni otra variante. El ledger de ADRs es PROPIO (desde 0001).

## Cómo usar el harness en esta condición

- **Antes de diseñar**: consulta `list_decisions` (ADRs de fases previas) y
  `search_memory` — las decisiones ya tomadas mandan; no las re-derives.
- **Al decidir arquitectura**: `record_decision` (siguiente id libre).
- **Al aprender algo durable**: `write_memory`.

## Reglas de trabajo

- **Lee la spec de la fase antes de escribir código** (`sdd/phase-0N/spec.md`).
- **Edita con `edit_file`** (reemplazos puntuales); `write_file` solo para archivos
  nuevos. Nunca imprimas el archivo completo en tu respuesta.
- **Verifica con `shell`**: `make` (+ `check.sh` de la fase si existe) debe quedar
  verde antes de darte por terminado.
- **No delegues**: no invoques `aitl run-host` ni otros agentes; el trabajo es tuyo.
- No leas ni uses nada bajo `../objective/` — es la referencia del gate, no tuya.
