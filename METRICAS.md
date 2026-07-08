# Catálogo de métricas — experimento raytracer (C0 vs C2)

Catálogo completo de las métricas medibles con el harness **AITL-Harness-JS** para el
experimento raytracer de la tesis (*Loop + Harness Engineering*). Cada métrica se ancla a:

- la **Tabla 4.3** de la tesis (`tab:metrics`, `thesis-harnesss/sections/chapter-04/chapter.tex:221-242`) y su mapeo ISO/IEC 25010:2023;
- las **hipótesis H1–H11** (`sections/chapter-01/chapter.tex:164-204`);
- el **campo/colección exacto** del harness donde se captura (verificado en el código de `AITL-Harness-JS` y en el MCP `aitl-js`, proyecto `aitl-raytracer`).

> Nota de alcance: el raytracer aún no se menciona en el `.tex` de la tesis — es el
> vehículo de medición complementario a Schoolar (`docs/THESIS-STATE.md:13`). Por eso este
> catálogo **hereda la Tabla 4.3 tal cual** y adapta al raytracer las dos dimensiones que
> son específicas de Schoolar (§6). El registro maestro de corridas es
> [`start/sdd/TRAZABILIDAD.md`](start/sdd/TRAZABILIDAD.md); el protocolo, [`aitl-raytracer/MANUAL.md`](aitl-raytracer/MANUAL.md).

## 0. Diseño experimental del raytracer

- **Unidad experimental**: celda *fase × condición* — 6 fases (F0–F5, con 5a/5b) × 3 condiciones = **21 celdas** mínimas (ADR-0004).
- **Condiciones** (plantillas en `start/conditions/`): `c0-bare` = `aitl run --bare` (sin memoria, skills ni gates) · `c2-memory` = `aitl run` (harness como conocimiento) · `c2-orchestrator` = `aitl run --model lmstudio --mcp` (harness Max-Capacity: orquestador local + sub-agentes). El raytracer **no** corre C1 (esa condición intermedia es del diseño Schoolar, `tab:cond`).
- **Recolección automatizada**: `scripts/run-cell.sh <fase> <cond>` → `data/metricas.csv` → `RESUMEN-METRICAS.md` (generado; M49–M51 calculados).
- **Variable independiente**: nivel de estructura del flujo (C0 vs C2m vs C2o). Dimensión adicional H9: modelo/proveedor.
- **Controladas**: prompt de tarea idéntico (textual, desde la spec), mismo commit de partida por fase (ramas gemelas `fase-N/{c0-bare,c2-memory,c2-orchestrator}` en el repo raíz), mismo gate `--verify-cmd`, misma máquina.
- **Identidad MCP**: `project: "aitl-raytracer"` en toda llamada — un `run_id` durable por celda.

## 0.5 Set primario (el que realmente se llena) vs. catálogo de referencia

El CSV único `data/metricas.csv` guarda **solo el set primario** — las métricas que de
verdad se capturan bajo `run-host` (Cara B):

```
fecha,cell,fase,run_id,status,gate,dur_s,iters,tok_out,tok_total,costo_pond,verify_1er,imagen_ok,adr,notas
```

- `costo_pond` = **costo comparable en USD** = `host_meta.cost_usd` real que reporta Claude
  (ya pondera el caché a su tarifa con descuento). Reemplaza al `tokens_input` crudo, que
  **no** es comparable entre condiciones y queda solo en los JSON de `data/runs/`.
- `gate` (ok/fail) y `dur_s` (wall-clock de la fase) vienen de `run-course.sh`.
- `verify_1er`, `imagen_ok`, `adr`, `notas` son columnas de **juicio manual**.
- Se retiraron del CSV las columnas que bajo `run-host` salían **siempre 0**
  (`tool_calls`, `gate_denials`, `supervision_min`) y el `tokens_input` crudo.

Las métricas **M1–M61** de abajo son el **catálogo de referencia** (la superset ISO/25010
para la tesis): útiles para el análisis profundo desde los JSON crudos y para C1/loop
nativo, pero **la mayoría es aspiracional** respecto a lo que el pipeline automatiza hoy.

---

## 1. Métricas primarias por corrida (`aitl run-show <run_id>`)

Fuente: colección `runs` + agregados de `events` (`src/cli.ts:364-421`; esquema en
`src/models/run.model.ts:34-53`, campos dinámicos escritos en `src/orchestration/graph.ts:440-454`).

| # | Métrica | Campo en `run-show` | Unidad | Dimensión Tabla 4.3 | Hipótesis |
|---|---|---|---|---|---|
| M1 | Duración de la corrida | `duration_ms` (= `ended_at − started_at`) | ms | 1 Velocidad | H1, H6 |
| M2 | Tokens de entrada (flujo acumulado) | `tokens.input` | tokens | 7 Eficiencia | H1, H4, H6 |
| M3 | Tokens de salida (esfuerzo real del modelo) | `tokens.output` | tokens | 7 Eficiencia | H1, H6 |
| M4 | Tokens totales (throughput del loop) | `tokens.total` | tokens | 7 Eficiencia | H6 |
| M5 | Iteraciones del loop | `iters` | conteo | 7 Eficiencia | H1, H6 |
| M6 | Llamadas a herramientas | `tool_calls` | conteo | 7 Eficiencia | H1 |
| M7 | Vetos de gates de permisos | `gate_denials` | conteo | 9 Trazabilidad | H5 |
| M8 | Intervenciones humanas | `human_interventions.count` / `.minutes` | conteo / min | 6 Supervisión | H10 |
| M9 | Aprobaciones interactivas (`--ask`) | `approvals.count` / `.ms` | conteo / ms | 6 Supervisión | H10 |
| M10 | **Supervisión total** | `supervision_minutes` = intervenciones + aprobaciones/60000 (`cli.ts:413`) | min | 6 Supervisión | H10 |
| M11 | Roles activos y vetos de rol | `roles`, `decision_blocked`, `review_events.{review,role_veto,deliberation}` | conteo | 9 Trazabilidad | H11 |
| M12 | Contexto hidratado al inicio | `hydrate` (secciones inyectadas) | secciones/bytes | 8 Memoria | H3, H7 |
| M13 | Clasificación SDD del prompt | `spec` (bool) | — | 9 Trazabilidad | H1 |
| M14 | Histograma de eventos | `event_counts` | conteo por tipo | — (transversal) | varias |
| M15 | Estado terminal | `status` ∈ `done\|error` | — | 2 Calidad funcional | H5, H6 |

## 2. Métricas de eventos del loop (colección `events`)

21 tipos (`src/models/event.model.ts:23-45`). Los que producen métrica directa
(consulta: `aitl run-show` → `event_counts`, o MCP `poll_events --project aitl-raytracer`):

| # | Evento | Payload útil | Métrica derivada | Hipótesis |
|---|---|---|---|---|
| M16 | `verify` | `{iter, ok, feedback}` (`graph.ts:317`) | nº de verificaciones fallidas antes del verde; **verde al primer intento** (bool) | H1, H5 |
| M17 | `retry` | `{iter, attempt, delay_ms, error}` | reintentos por errores de infraestructura | H6 |
| M18 | `compaction` | `{iter}` | compactaciones de contexto en sesiones largas (F4/F5) | H4 |
| M19 | `hydrate` | secciones (ADRs/memoria inyectados) | evidencia de recuperación selectiva: ¿entraron los ADRs de fases previas? | H3, H7 |
| M20 | `gate` | `{name, decision:"deny", reason}` | denegaciones de tools por política | H5 |
| M21 | `approval` | `{tool, decision, ms, interactive}` | latencia humana por aprobación | H10 |
| M22 | `review` / `role_veto` / `deliberation` | `{role, mode, severity, stance, findings}` (`roles/engine.ts:90-101`) | objeciones de roles y bloqueos en checkpoints (F4–F5 con `--roles architect,qa`) | H11 |
| M23 | `human_intervention` | `{reason, minutes}` (`cli.ts:359`) | correcciones humanas auditadas (`aitl intervene`) | H10 |
| M24 | `error` | `{iter, message}` | fallos del loop / del host | H6 |
| M25 | `loop_iter`, `tool_call`, `skills_route`, `session_summary`, `synthesis`, `resume`, `spawn` | — | actividad por iteración, ruteo de skills, síntesis de sesión | H1, H2 |

## 3. Contabilidad de tokens (obligatoria para comparar C0 vs C2)

`AITL-Harness-JS/docs/token-accounting.md`: `tokens.input` es **flujo acumulado**
(≈97.7 % dominado por `cache_read`), no debe compararse crudo. Reportar las cuatro vistas:

| # | Métrica | Fórmula / fuente | Interpretación |
|---|---|---|---|
| M26 | Output tokens | `tokens.output` | esfuerzo generativo real |
| M27 | Input único | `raw_input_tokens (fresh) + cache.creation` (en `host_meta`) | contexto genuinamente nuevo |
| M28 | **Costo ponderado** | `fresh×1.0 + cache_creation×1.25 + cache_read×0.1` + output×tarifa | costo económico comparable |
| M29 | Throughput acumulado | `tokens.total` | trabajo total del loop (la comparación C0 vs C2 canónica) |
| M30 | Costo USD (Cara B) | `host_meta.cost_usd` (lo reporta Claude Code) | costo directo por sesión host |

## 4. Métricas de sesiones Cara B (hooks `hydrate` / `capture-session`)

Una sesión de Claude Code capturada se persiste como run de primera clase
(`src/context/capture.ts:274-364`), con:

| # | Métrica | Campo |
|---|---|---|
| M31 | Turnos del host | `iters` (= `num_turns`) y `host_meta.num_turns` |
| M32 | Duración wall-clock | `host_meta.duration_ms` |
| M33 | Desglose de caché | `host_meta.cache.{creation,read}`, `raw_input_tokens` |
| M34 | Artefactos producidos en sesión | `artifacts.{decisions[],memories[],prompts[],interventions}` |
| M35 | Componentes tocados | tags `component:<dir>` (de `editedPaths`) |

## 5. Métricas de conocimiento durable y trazabilidad (MCP `aitl-js`)

| # | Métrica | Fuente | Dimensión / Hipótesis |
|---|---|---|---|
| M36 | ADRs por fase (nº, versión, ciclo de vida `accepted→deprecated/superseded`) | colección `decisions` (`list_decisions`) | 9 Trazabilidad / H3, H8 |
| M37 | Memorias escritas/recuperadas y su uso efectivo | colección `memory` + evento `hydrate` (recuerdos recuperados vs citados en la solución) | 8 Memoria / H3, H8 |
| M38 | Prompts registrados (con `metadata.tokens/cost_usd/num_turns`) | colección `prompts` (`list_prompts`) | 9 Trazabilidad |
| M39 | **Cadena spec → run → cambios → verify → ADR** completa (bool por celda) | `GET /api/runs/:id/graph` (grafo de sesión, `src/graph/session.ts`) | 9 Trazabilidad / H1 |
| M40 | Latencia y tasa de éxito de tools MCP | colección `mcp_tool_calls` (`{tool, ok, ms}`) | 7 Eficiencia / H2 |
| M41 | Auditoría de acciones por actor/rol | colección `audit` (`{actor_id, action, ok}`) | 6 Supervisión / H10 |
| M42 | Tokens por turno y tool calls por mensaje | colección `messages` (`tokens`, `tool_calls[]`) | 7 Eficiencia |

## 6. Métricas específicas del raytracer (gate de calidad objetivo)

Adaptación de las dimensiones Schoolar-específicas de la Tabla 4.3 (el raytracer no tiene
tenants ni `run_tests` de Schoolar):

| # | Métrica | Captura | Sustituye a (Tabla 4.3) |
|---|---|---|---|
| M43 | Compilación verde (`make`) | evento `verify` con `--verify-cmd "make"` | 2 Calidad funcional (`run_tests`) |
| M44 | **Corrección de imagen**: dimensiones + hash del PPM, o distancia RMS contra el render de referencia de la spec | `--verify-cmd "make && ./check.sh"` (MANUAL.md §4; referencias en `start/sdd/phase-0N/`) | 2 Calidad funcional |
| M45 | Regresiones visuales: renders de fases previas siguen correctos tras la fase N | re-ejecutar checks de fases 0..N−1 en la rama de la fase N | 3 Estabilidad |
| M46 | Higiene de compilación: warnings `-Wall` y tamaño del diff vs base | salida de `make` + `git diff --stat` | 4 Mantenibilidad (`check_dependencies`) |
| M47 | Convergencia Monte Carlo: varianza/ruido a spp fijo (32/512/2048) entre condiciones — p.ej. MIS vs muestreo simple en F5a | RMS entre render y referencia de alta muestra | 2 Calidad funcional (específica del dominio) |
| M48 | Tiempo de render (OpenMP) por imagen | wall-clock del check | 1 Velocidad (secundaria) |
| — | Seguridad (aislamiento por tenant) | **N/A en raytracer** — se mide solo en Schoolar | 5 Seguridad |

## 7. Métricas derivadas comparativas (la evidencia de la tesis)

Por fase, con las celdas C0 y C2 completas (Δ = C0 − C2; ratio = C0/C2):

| # | Métrica derivada | Fórmula | Hipótesis que respalda |
|---|---|---|---|
| M49 | Δ duración | `duration_ms(C0) − duration_ms(C2)` | H1 |
| M50 | Δ iteraciones y Δ tool calls | M5, M6 entre condiciones | H1, H6 |
| M51 | Δ costo ponderado de tokens | M28 entre condiciones | H1, H6 |
| M52 | Tasa de verde al primer intento | `verify.ok` en el primer evento `verify`, por condición | H1, H5 |
| M53 | Retrabajo | nº de eventos `verify` fallidos + iteraciones posteriores al primer fallo | H3, H5 |
| M54 | Costo de reconstrucción de contexto en C0 | tokens de input único (M27) que C0 gasta re-explicando lo que en C2 entra por `hydrate` (M19) | H3 |
| M55 | Calidad sostenida en sesiones largas | M18 (compactions) sin caída de M44/M47 en F4–F5 | H4 |
| M56 | Regresiones que C0 deja pasar | M45 en C0 vs C2 | H5 |
| M57 | Corridas colgadas / fuera de presupuesto | runs con `status:error` por `maxIters`/deadline; debe ser 0 | H6 |
| M58 | Reincidencia de bugs entre fases | bug registrado como memoria tipo `bug` en fase N que reaparece (o no) en N+1 | H8 |
| M59 | Portabilidad multi-modelo | repetir una fase con otro provider (`--model lmstudio` vs `anthropic`): mismas specs/tools, comparar M1–M5, M44 | H9 |
| M60 | Δ supervisión humana | M10 entre condiciones | H10 |
| M61 | Cumplimiento arquitectónico con roles | M22 en F4–F5: objeciones del DecisionBrief que evitan violaciones de la spec | H11 |

## 8. Mapeo completo Tabla 4.3 → métricas de este catálogo

| Dimensión (Tabla 4.3) | ISO 25010 | Métricas |
|---|---|---|
| 1 Velocidad | n/a | M1, M48, M49 |
| 2 Calidad funcional | Adecuación funcional | M15, M16, M43, M44, M47, M52 |
| 3 Estabilidad | Fiabilidad | M45, M56 |
| 4 Mantenibilidad | Mantenibilidad | M46 |
| 5 Seguridad | Seguridad | N/A raytracer (solo Schoolar) |
| 6 Supervisión humana | n/a | M8–M10, M21, M23, M41, M60 |
| 7 Eficiencia del agente | n/a | M2–M6, M26–M30, M40, M42, M50, M51 |
| 8 Memoria | n/a | M12, M19, M37, M54 |
| 9 Trazabilidad | n/a | M7, M11, M13, M36, M38, M39, M61 |

## 9. Recolección — comandos por celda (fase × condición)

```bash
# 1. Corrida (anota el run_id que imprime)
aitl run "<prompt de la spec>" --project aitl-raytracer [--bare] --verify-cmd "make && ./check.sh"

# 2. Métricas primarias (M1–M15) + histograma de eventos
aitl run-show <run_id> --project aitl-raytracer

# 3. Supervisión humana fuera de --ask (M23)
aitl intervene <run_id> --project aitl-raytracer --reason "<qué corrigió el humano>" --minutes <n>

# 4. Trazabilidad (M39): grafo spec→run→ADR→memoria
#    UI del harness, pestaña Runs, o:
curl "http://localhost:<puerto>/api/runs/<run_id>/graph?project=aitl-raytracer"
```

Vía MCP `aitl-js` (equivalentes durables): `poll_events` (M16–M25), `list_decisions` (M36),
`search_memory` (M37), `list_prompts` (M38), `record_human_intervention` (M23).

### Plantilla de hoja de métricas (una fila por celda; alimenta `start/sdd/TRAZABILIDAD.md` y `tab:metrics`)

| Fase | Cond | run_id | dur_ms | iters | tool_calls | tok_out | tok_in_único | costo_pond | verify_1er_intento | verify_fallidos | gate_denials | superv_min | compactions | hydrate_ADRs | imagen_ok (hash/RMS) | regresiones | ADR |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 00 | C0 | | | | | | | | | | | | | n/a | | | |
| 00 | C2 | | | | | | | | | | | | | | | | |

## 10. Reglas de validez (resumen de MANUAL.md §5)

1. Prompt idéntico entre C0 y C2 (textual, desde la spec `approved`).
2. Un `run_id` por celda; corridas inválidas se anotan con `aitl intervene`, nunca se borran.
3. En C2 con fase ≥ 1, verificar en el evento `hydrate` que entraron los ADRs previos (si no, la celda no mide H3).
4. `tokens.input` crudo no se compara entre condiciones — usar M26–M29 (token-accounting).
5. Toda llamada MCP con `project: "aitl-raytracer"`; ledger de ADRs propio (0001…).
