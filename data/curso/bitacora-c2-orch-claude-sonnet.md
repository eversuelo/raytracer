# Bitácora del investigador — celda `c2-orch-claude@sonnet`

> Redactada por el orquestador de la campaña (Claude) el 2026-07-09/10. Fuentes: CSV,
> logs por fase + gate logs, runs de Mongo bajo `aitl-raytracer-orq-sonnet`, monitores
> en vivo (renders, delegaciones, escrituras MCP). Par de las otras tres bitácoras.

## Condiciones de la corrida

- **Arquitectura**: orquestador = **Claude Code SONNET** vía `run-host` (condición
  nueva `c2-orch-claude`, commit `b675032`); sub-agentes = claude-code **HAIKU**
  clavados por argv (`env AITL_HOST_ARGS_CLAUDE_CODE='--model claude-haiku-4-5'`,
  ADR-0058 — gana sobre el settings.json compartido). Protocolo por fase: consultar
  memoria → delegar → verificar gate → re-delegar con error PRECISO → **write_memory**
  imperativo al quedar verde. Máx. 3 delegaciones.
- **Project MCP propio**: `aitl-raytracer-orq-sonnet` (hooks de la plantilla nacieron
  apuntando bien — sin la fuga de la celda qwen).
- Tope 60 min · mismos gates objetivos · misma base que todas las celdas.

## Resultado global

**Fases 0-2 verdes, todas a la PRIMERA delegación. Fase 3 INVÁLIDA por límite de
sesión de Claude (429), no por fallo del agente.** Total 25 min 14 s.

| Fase | run orq (sonnet) | sesión sub (haiku) | gate | dur |
|---|---|---|---|---|
| 0 | `f7075b34` · 17 turns · $0.86 | `7e0e94f5` · 88 s · 12 iters · $0.13 | ✅ | 220 s |
| 1 | `53723cbb` · 13 turns · $0.64 | `d75646cd` · 301 s · 8 iters · $0.31 | ✅ | 397 s |
| 2 | `0394ef5f` · 10 turns · $0.54 | `2a4ca76a` · 322 s · 25 iters · $0.24 | ✅ | 404 s |
| 3 | `30308445` · gate fail 438 s | **ninguna: las 2 delegaciones murieron con 429 (0 tokens)** | ⚠ INVALIDA | — |

Costo de lo válido (fases 0-2, ambos niveles): ≈ $2.72. El resumen de celda tampoco
corrió (429); los 3 runs víctima (0 tokens) fueron **eliminados de Mongo y del CSV** a
petición del usuario — error de infraestructura, no dato experimental.

## Lo que esta celda demostró (el dato más importante de la campaña)

1. **El canal de memoria por fin operó de verdad — primera vez en toda la campaña**:
   sonnet escribió `phase-00/01/02-gate-verde-primer-intento` con **cuerpo real**
   (qué se implementó, cómo se verificó, commit anclado; campo `body` con contenido
   semántico, no prefijos de transcript). Las fases 1 y 2 hidrataron esas memorias.
   Contraste directo con haiku: 0 escrituras deliberadas en 3 celdas.
2. **Cero correcciones necesarias**: 3/3 fases verdes con una sola delegación cada
   una. Comparar con la celda qwen (mismas fases: 1 delegación c/u pero 280/441/300 s
   con verificación más lenta) y con los one-shot.
3. **El orquestador sonnet siguió el protocolo completo** (consultar memoria →
   delegar → verificar → write_memory) sin desviarse — la instrucción imperativa de
   MCP en el prompt fue suficiente para un modelo grande, mientras el CLAUDE.md
   declarativo fue ignorado por haiku toda la campaña.

## Incidencia que cerró la celda: límite de sesión (429)

A las ~22:18, en plena fase 3, la cuenta tocó su **session limit** ("resets 12:20am").
Secuencia forense:

1. Delegación 1 de fase 3 (`671a076f`): murió al instante — `api_error_status: 429`,
   0 tokens. Reintento (`e39d2439`): ídem.
2. Sin sub-agente posible, el gate corrió sobre el rt.cpp que quedó a medio camino
   (error de compilación en `sample_area_light`, aridad incorrecta) → rojo.
   *Nota: el código roto sugiere que algo alcanzó a editar antes de agotarse el
   presupuesto — probablemente el propio orquestador o una edición parcial previa.*
3. El agente del resumen (`aea057c9`) también murió con 429 → sin RESUMEN-CURSO.md.
4. Los 3 runs víctima se borraron (decisión del usuario); la fila de fase 3 queda
   anotada INVALIDA en el CSV. El muro real de la fase 3 (arealight ~0.79×) quedó
   **sin medir** para esta celda.

Lección de instrumentación: el harness debería distinguir `429/límite` de un fallo
del agente (hoy ambos terminan en `status=error` o en gate rojo) y pausar/reintentar
en vez de consumir delegaciones. Anotado para aitl-js.

## Extensión de estado del arte (Tema 7) — la celda continúa

Con el temario 0-5 cubierto por la campaña, la celda se extendió con la **fase 06:
medios participantes** (Tema 7 — RTE, transmitancia homogénea `exp(−σt·s)`, ray
marching con single scattering, función de fase isotrópica 1/(4π)):

- Spec + gate analítico nuevos (`start/sdd/phase-06/`, commit `d7aea94`): sin render
  de referencia, la verificación es física — medio nulo debe reproducir la fase 2
  bit a bit, la absorción solo puede atenuar, el in-scattering debe crear glow
  alrededor de la fuente puntual, determinismo OpenMP.
- Lanzador `scripts/run-tema7.sh`: corre SOLO esa fase con esta condición, partiendo
  del tag `celda/c2-orch-claude-sonnet/f2` (el punto aceptado más alto de la celda).
- Resultado: (completar al cierre de la corrida — fila fase 6 en el CSV, tag
  `celda/c2-orch-claude-sonnet/f6`, renders `image-fog-*` en data/runs).

## Evidencia

- CSV: filas `c2-orch-claude@sonnet` (orquestador) + `c2-orch-claude@sonnet-session`
  (sesiones haiku; el recolector excluye los espejos de capture-session desde
  `ada3c99`).
- Memorias con contenido: `memory/phase-0{0,1,2}-gate-verde-primer-intento`
  (project `aitl-raytracer-orq-sonnet`).
- Tags git: `celda/c2-orch-claude-sonnet/f0..f3` (+ `f6` al cerrar el Tema 7).
- Renders PNG + código: `data/runs/fase{0..3,6}-c2-orch-claude@sonnet-*`.
