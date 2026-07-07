# CLAUDE.md — aitl-raytracer

## Identidad del proyecto (leer primero)

Este repo es el **laboratorio experimental** del harness AITL: un raytracer que se
desarrolla por Spec Driven Development en 5 fases para demostrar, en runtime, las
afirmaciones de la tesis (condiciones C0 vs C2). Estado durable en el MCP `aitl-js`.

| Campo | Valor |
|-------|-------|
| **Clave MCP canónica del proyecto** | `aitl-raytracer` |
| **Hash del proyecto** | `8d9c37d22aac48e8` (`sha256("aitl-raytracer")[:16]`) |
| **Software (jerarquía MCP)** | `raytracer` |

**Regla para toda llamada de agente/tool contra el MCP `aitl-js`:** pasa
`project: "aitl-raytracer"`. NO uses `raytracer`, `rt` ni otra variante — fragmentan la
historia. El ledger de ADRs de este proyecto es PROPIO (empieza en 0001) e independiente
del de `aitl-js`.

## Qué es este proyecto

Raytracer en **C++ / OpenMP** (curso IA7200-L; código base en `../start/sdd/base/rt.cpp`)
construido en 6 fases incrementales. Las specs SDD canónicas viven en
`../start/sdd/phase-0N/spec.md` (con los enunciados originales del curso y las imágenes de
referencia); `specs/fase-N/` en este repo son punteros.

| Fase | Proyecto del curso | Tema | Ramas |
|---|---|---|---|
| 0 | Proyecto 1 | Intersección rayo-esfera, renders de depuración, OpenMP | `fase-0/{c0-bare,c2-memory,c2-orchestrator}` |
| 1 | Proyecto 2 · p1 | Iluminación directa Monte Carlo: emisor esférico, 3 muestreos | `fase-1/{c0-bare,c2-memory,c2-orchestrator}` |
| 2 | Proyecto 2 · p2 | Fuentes puntuales (I=4000), sombras duras | `fase-2/{c0-bare,c2-memory,c2-orchestrator}` |
| 3 | Proyecto 3 | Muestreo de importancia de fuentes + múltiples emisores (2A1P) | `fase-3/{c0-bare,c2-memory,c2-orchestrator}` |
| 4 | Proyecto 4 | Microfacets: conductores ásperos (aluminio/oro α=0.3) | `fase-4/{c0-bare,c2-memory,c2-orchestrator}` |
| 5 | Proyecto final | 5a: MIS β=2 · 5b: Path Tracing explícito (10 rebotes) | `fase-5/{c0-bare,c2-memory,c2-orchestrator}` |

`main` guarda specs, manual y configuración — **nunca implementación**. La implementación
vive en las ramas de fase/condición para que las corridas sean comparables.

## Reglas para agentes

- **Lee la spec de la fase antes de escribir código** (`specs/fase-N/spec.md`). Si la spec
  no existe o está vacía, DETENTE y pídela — el experimento pierde validez sin ella.
- Registra decisiones de arquitectura con `record_decision` (project `aitl-raytracer`,
  siguiente id libre leído de la colección — no lo fijes en docs) y refleja el markdown
  en `docs/adr/`.
- `make` debe compilar sin errores antes de dar una fase por terminada; el loop del
  harness usa `--verify-cmd "make && ./check.sh"` como gate de calidad (ADR-0002).
- Las imágenes renderizadas van a `out/` (gitignored) — compara contra las referencias
  que la spec defina.
- El protocolo experimental completo está en `MANUAL.md` — síguelo al pie de la letra:
  es lo que hace verificable la tesis.
