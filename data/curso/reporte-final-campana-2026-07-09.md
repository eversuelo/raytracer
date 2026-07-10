# REPORTE FINAL — Campaña de métricas del raytracer (2026-07-09)

> Consolidado único de la campaña "desde 0" del curso IA7200-L (raytracer C++/OpenMP,
> 6 fases con gate objetivo, tope 60 min por celda). Reúne lo reportado en las
> bitácoras/comparativo/hallazgo **más todo lo no reportado hasta ahora**. Estado al
> redactarse: 3 celdas cerradas + `c2-orch-claude@sonnet` EN CURSO (fases 0-2 verdes;
> actualizar su fila final al cierre).
>
> Piezas de detalle: `bitacora-c0-bare-haiku.md` · `bitacora-c2-memory-haiku.md` ·
> `bitacora-c2-orchestrator-haiku.md` · `comparativo-c0-vs-c2-haiku.md` ·
> `hallazgo-claudemd-vs-mcp-haiku.md` · resúmenes escritos por cada celda
> (`resumen-*.md`).

## 1. Diseño ejecutado

| Celda | Quién orquesta | Quién implementa | Memoria/MCP | Project MCP |
|---|---|---|---|---|
| `c0-bare@haiku` | — (one-shot por fase) | claude-code haiku | NO (--no-hydrate, ADR-0068) | aitl-raytracer (solo telemetría) |
| `c2-memory@haiku` | — (one-shot por fase) | claude-code haiku | hidratación + MCP montado | aitl-raytracer |
| `c2-orchestrator@haiku` | **qwen2.5-coder-7b local** (loop nativo aitl) | sub-agentes claude-code haiku | hidratación + MCP | aitl-raytracer-orq |
| `c2-orch-claude@sonnet` | **claude-code SONNET** (run-host) | sub-agentes claude-code haiku | hidratación + MCP + **write_memory imperativo** | aitl-raytracer-orq-sonnet |

Todas las celdas parten de la MISMA base (`start/sdd/base/`), mismos prompts de fase
(extraídos de las specs) y mismos gates objetivos independientes del agente.

## 2. Marcador global

| | c0-bare | c2-memory | c2-orch (qwen) | c2-orch (sonnet)* |
|---|---|---|---|---|
| Fases verdes | 3/6 | **5/6** | 3/6 | 3/3 al corte* |
| Fase de muerte | 3 (rendición implícita) | 5 (esperó "notificación") | 3 (timeout, 3 delegaciones) | en fase 3* |
| Dur. despierta | 26m56s | 36m49s | ≈60m (tope) | en curso |
| Costo API | $2.46 | $3.07 | $4.48 (qwen $0) | en curso (2 niveles) |
| $/fase verde | $0.82 | $0.61 | $1.49 | — |
| Delegaciones/fase | — | — | 1,1,1,3 | 1,1,1,? |
| Memorias escritas con contenido útil | 0 | 0 | 0 | **1 por fase** (phase-NN-gate-verde) |

\* actualizar al cierre de la celda.

| Fase (dur s) | c0 | c2-mem | orq-qwen | orq-sonnet |
|---|---|---|---|---|
| 0 | 150 ✅ | 85 ✅ | 280 ✅ | 220 ✅ |
| 1 | 282 ✅ | 219 ✅ | 441 ✅ | 397 ✅ |
| 2 | 166 ✅ | 185 ✅ | 300 ✅ | 404 ✅ |
| 3 | 962 ❌ | 517 ✅ | ~2520 ❌ timeout | en curso |
| 4 | — | 730 ✅ | — | — |
| 5 | — | 316 ❌ | — | — |

## 3. Los hallazgos de la campaña (jerarquizados)

1. **El muro de la fase 3 es un benchmark natural reproducible**: 6+ sesiones haiku
   distintas chocaron con el MISMO defecto — el estimador `arealight` con brillo
   sistemáticamente bajo (firma ~0.79 ≈ π/4: normalización del pdf / visibilidad del
   rayo de sombra). Solo c2-memory lo cruzó (diagnóstico geométrico "la dirección va
   HACIA x′"). c0 degeneró en 35 ediciones de thresholds y el orquestador-qwen gastó
   3 delegaciones llegando a Δ=3.15 (umbral 2.00) sin cruzar.
2. **El modo de fallo dominante de haiku NO es la física, es el protocolo one-shot**:
   c0 murió preguntando "¿debería continuar?" a un stdin cerrado; c2-memory murió
   "esperando una notificación en ~10 min" que no existe (llegó a inventar un bloque
   `<task-notification>`); el precursor (`sleep 30` bloqueado) apareció ya en fase 1.
   El cierre de loop EXTERNO (orquestador con gate objetivo) es el único mecanismo
   del diseño que ataca esto — y en la celda qwen demostró funcionar (detectó y
   re-delegó dos cierres complacientes).
3. **La atribución "mejora por memoria" NO quedó demostrada en las celdas haiku**:
   0 llamadas MCP en todas las sesiones haiku, 0 ADRs, y la hidratación solo llevó
   prefijos truncados de transcript (~2000 chars de aperturas, sin los diagnósticos).
   [Corrección metodológica registrada: una versión previa del análisis reportó los
   docs "vacíos" por leer `content` en vez de `body` en el esquema.] Con n=1, el
   5/6 de c2-memory se explica mejor por varianza.
4. **El orquestador-sonnet cierra las dos brechas a la vez** (evidencia parcial, celda
   en curso): escribe memorias con contenido real por fase (slug semántico, body con
   el qué/causa/solución, commit anclado) que las fases siguientes hidratan, y puede
   citar errores con precisión al re-delegar. Fases 0-2 verdes a la primera
   delegación cada una.
5. **Límites medidos del orquestador local 7B** (celda qwen): resumen de error
   genérico al re-delegar ("la diferencia media es mayor que lo permitido" — sin
   valor/umbral/modo) y presión de contexto: ~16.0-16.5k de una ventana de 20k por
   turno, 80 % consumido por los esquemas de 52 tools MCP de las que solo usa `shell`.

## 4. Lo no reportado hasta ahora (misceláneo de instrumentación)

- **Duplicados de capture-session**: con los hooks apuntando al project correcto, el
  hook `Stop: aitl capture-session` crea run-docs espejo (`harness_config.captured:
  true`, id = session_id, sin mensajes) de sesiones que run-host ya registró. El
  recolector de sesiones los excluye desde `ada3c99`; 4 filas duplicadas fueron
  depuradas del CSV.
- **Filas `-session` en el CSV**: cada sub-agente queda como fila propia
  (`<cell>-session`, fase mapeada por la ventana del run orquestador), automático vía
  `scripts/collect-sessions.mjs` + hook en `run-course.sh` (y con filtro de runs
  `running`). Los `iters` de las filas del orquestador cuentan el loop de arriba
  (qwen: 2/fase; sonnet: turnos de claude), no el trabajo haiku.
- **Fuga de hooks de la celda qwen** (corregida en `47a655a` DESPUÉS de esa celda):
  sus `capture-session`/`hydrate` llevaban `--project aitl-raytracer` hardcodeado —
  sus session-docs cayeron al store de c0/c2-memory (prefijos de transcript; impacto
  bajo). La celda sonnet ya nació con hooks correctos.
- **Sub-agente huérfano**: al expirar el timeout de fase de la celda qwen, la 3ª
  delegación quedó re-parentada y siguió quemando API; se detectó por el monitor de
  renders y se mató a mano. Pendiente aitl-js: tree-kill del árbol de run-host.
- **Gate final de fase 3 (qwen) es un artefacto**: registró
  `'./rt solidangle 32 1L' terminó con error` porque el kill agarró a rt.cpp a medio
  editar; el último estado real medido era arealight Δ=3.15.
- **Suspensión del equipo (~3.5 h)** durante la fase 3 de la celda qwen: procesos
  sobrevivieron y retomaron; el deadline (reloj real) expiró → curso cerrado ahí.
  Filas afectadas anotadas en el CSV (`dur incluye ~3.5h de suspensión`).
- **Asimetría de permisos git**: la plantilla del orquestador permite git al
  sub-agente (commit `1ecc94e` hecho por el propio haiku en fase 1 de la celda qwen);
  c0/c2-memory lo bloquean. Homologar en la próxima campaña.
- **Corridas inválidas previas al reset**: 2 runs c0 de la mañana (contaminación por
  hidratación, ADR-0068) anotados con `aitl intervene` y borrados con todo el reset
  ("comienza de 0") pedido por el usuario; respaldo completo en
  `metricas/archivo-raytracer-20260709/` (renders, dump JSON del store, logs).
- **Runs stale en Mongo**: `6c644f3a` (orquestador qwen fase 3, matado) quedó
  `status=running` para siempre; considerar un sweep de cierre en aitl-js.
- **Pendiente del harness (repo AITL-Harness-JS)**: el flag `--no-hydrate` de
  `run-host` (cli.ts) sigue SIN commitear, mezclado con el WIP de ADR-0067 en
  `feat/harness-v2`. ADR-0068 ya registrado en el ledger aitl-js.
- **Infra de observación usada** (reproducible): monitor de procesos `rt` (cada
  render, con args), monitor de delegaciones `run-host`, watcher de escrituras al
  store (sondeo de Atlas cada 5 s con tamaño de body) y recolector idempotente de
  sesiones. Los cuatro viven como scripts en scratchpad/`scripts/`.

## 5. Estado de la evidencia

- **CSV único**: `data/metricas.csv` — filas por fase de cada celda + filas
  `-session` por sub-agente, con `notas` para anomalías.
- **Renders**: `data/runs/fase*-<cell>-*.png` (comprimidos) por fase/celda;
  referencias del curso en `start/sdd/phase-0N/*.jpg`; solución humana en
  `objective/Proyecto N/` (incluye `rt.cpp` humano — NUNCA accesible a los agentes).
- **Código generado**: `data/runs/*.rt.cpp` + tags `celda/<cell>/f<N>` (un commit por
  fase en la rama `curso`).
- **Telemetría durable**: Atlas (db `aitl`), projects `aitl-raytracer`,
  `aitl-raytracer-orq`, `aitl-raytracer-orq-sonnet` (runs, messages, events, memory,
  prompts); `data/runs/*.runshow.txt` como espejo versionado.
- **Logs crudos**: `data/logs/` (master + por fase + gate) — gitignored, viven en el
  working tree.

## 6. Qué sigue (priorizado)

1. Cerrar `c2-orch-claude@sonnet` y actualizar §2 + su bitácora.
2. Fix aitl-js: síntesis real en `capture-session` (hoy trunca prefijo) + tree-kill
   de run-host + commitear `--no-hydrate`.
3. Decidir: hook capture-session en c0 (¿el baseline debe escribir al store?) y
   permisos git homogéneos.
4. n≥3 por celda para separar varianza de efecto (el punto 3 de §3 lo exige).
5. Generar el capítulo comparativo de la tesis con `THESIS-PROMPT.md` (raíz del repo).
