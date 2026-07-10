# Bitácora del investigador — celda `c2-memory@haiku`

> Redactada por el orquestador de la campaña (Claude) el 2026-07-09, con las mismas
> fuentes que la bitácora de c0: `data/metricas.csv`, logs por fase
> (`data/logs/curso-c2-memory-haiku-*-20260709-135639.log` + `.gate.log`), transcripts
> crudos de las 6 sesiones de Claude Code, eventos `hydrate` en Mongo y el resumen del
> propio agente (`resumen-c2-memory-haiku.md`). Par de `bitacora-c0-bare-haiku.md`.

## Condiciones de la corrida

- Misma campaña limpia que c0 (store reseteado antes de c0; ver bitácora de c0).
- **C2-memoria**: hidratación de `run-host` ACTIVA (verificada: 1 evento `hydrate` por
  fase) + hook `UserPromptSubmit: aitl hydrate` + MCP `aitl-js` montado + CLAUDE.md con
  instrucciones de usar `list_decisions`/`search_memory`/`record_decision`/`write_memory`.
- Modelo agente: `claude-haiku-4-5-20251001` · tope global 60 min · mismos gates que c0.

## Resultado global

**Llegó a fase 5 de 5 y falló ahí: 5/6 fases en verde** (c0 logró 3/6). Total 36 min 49 s.

| Fase | run_id | turns | tokens | costo USD | gate | dur |
|---|---|---|---|---|---|---|
| 0 — intersección/debug | `3d88ff86` | 13 | 448 358 | 0.139 | ✅ | 85 s |
| 1 — MC directo | `7f225f63` | 30 | 1 621 677 | 0.322 | ✅ | 219 s |
| 2 — fuente puntual | `9bb4833c` | 26 | 1 215 675 | 0.260 | ✅ | 185 s |
| 3 — importance sampling | `e41f49e0` | 46 | 3 450 266 | 0.707 | ✅ | 517 s |
| 4 — IS de BRDF / combinado | `a4d962dc` | 69 | 5 835 585 | 1.062 | ✅ | 730 s |
| 5 — path tracing + MIS | `d7764605` | 39 | 2 884 524 | 0.585 | ❌ | 316 s |
| **Total** | | **223** | **15 456 085** | **≈ 3.07** | 5/6 | 2 052 s |

## ⚠ Hallazgo central: la capa de memoria estuvo funcionalmente INERTE

El resultado NO es atribuible al conocimiento durable, por tres datos duros:

1. **Lo hidratado era de bajo valor**: los eventos `hydrate` de las 6 fases traen
   `memory: 5-6, decisions: 0`. Esos docs `session-<id>` de `capture-session` llevan
   ~2000 chars de `body`, pero son el PREFIJO truncado del transcript de la sesión
   previa — la narrativa de apertura ("voy a leer la spec…"), NO una síntesis: los
   diagnósticos útiles (p. ej. el bug de visibilidad de fase 3) ocurren profundo en
   la sesión y quedan fuera del corte. [CORRECCIÓN 2026-07-09: una versión previa
   los reportó "0 chars" por leer el campo `content` en vez de `body`.] El preámbulo
   inyectaba ~10-12k chars de boilerplate de aperturas, incluidas las de sesiones de
   la celda c0 (fuga c0→c2 vía hook, de bajo valor por lo mismo).
2. **0 llamadas MCP en las 6 fases**: haiku nunca invocó `search_memory`,
   `record_decision` ni `write_memory`, pese a que el CLAUDE.md de la condición se lo
   instruye. `decisions: 0` toda la celda porque nadie escribió ADRs.
3. Sin ADRs ni memorias con contenido, **la fase N no heredó nada de la N-1** más que
   el propio código en el working tree (igual que en c0).

Lectura: en esta corrida c2-memory ≈ "c0 + ~21 líneas extra de CLAUDE.md + servidor
MCP montado pero ignorado". Con n=1, el 5/6 vs 3/6 se explica mejor por varianza
entre corridas que por el mecanismo de memoria. Para que la condición sea real hay
que (a) arreglar la síntesis vacía de `capture-session` y (b) lograr que el modelo
use las tools (instrucción imperativa en el prompt de fase, o modelo mayor).

## Errores y fricciones por fase (del transcript crudo)

### Fase 0 (5 ediciones, 3 bash, 3 fricciones) — impecable
La más corta de toda la campaña (85 s, 13 turnos). Sin errores de fondo.

### Fase 1 (7 ediciones, 19 bash, 9 fricciones)
- Error de compilación en `build_basis` (mismo tropiezo de referencias que c0 en su
  `localBasis` — el patrón de bug es reproducible entre celdas).
- Primer gate-check falló por contrato (`./rt cosinehemi 32` no dejó `image.ppm`).
- **Precursor del fallo final**: intentó `sleep 30` para esperar renders y el sandbox
  de Claude Code se lo bloqueó ("To wait for a condition, use Monitor…") — la
  tendencia a *esperar en vez de verificar* ya asomaba aquí.

### Fase 2 (4 ediciones, 14 bash, 5 fricciones)
- Limpia; solo un `cp /tmp/test1.ppm` bloqueado por sandbox (mismo tipo de fricción
  que c0 con `/tmp`).

### Fase 3 (15 ediciones, 23 bash, 10 fricciones) — el muro de c0, superado
- Error de compilación inicial (`Sphere spheres[16]` sin constructor default).
- **El mismo bug que mató a c0**: render `arealight` casi negro (5.7, 5.7, 5.7). La
  diferencia: lo diagnosticó como geometría — *"la dirección debería ser HACIA x′,
  no desde x′"* — lo corrigió una vez y quedó (c0 iteró thresholds ad-hoc 35 veces
  sin converger).
- Flujo de depuración más eficiente que c0: calibró con renders de **4 spp**
  (segundos) y validó a 32 spp solo al final; corrió `check.sh` completo dos veces.
- 46 turnos vs 103 de c0 en la misma fase; $0.71 vs $1.69; 517 s vs 962 s (y verde).

### Fase 4 (21 ediciones, 39 bash, 14 fricciones) — la más cara de la celda
- Error de compilación en `DiffuseMaterial::sample` + un miss de calibración real
  (`RC-Point` Δ=2.69 vs 2.00 permitido) que corrigió antes del gate.
- Fricciones de sandbox (`find` fuera del working dir, `cd` compuesto bloqueado).
- 730 s y $1.06 — territorio que c0 nunca pisó.

### Fase 5 (5 ediciones, 19 bash, 8 fricciones) — muerte por mecánica agéntica, no por física
Cronología:
1. Implementó path tracing con MIS; los renders de 32 spp de las tres escenas
   pasaron compare-ref (PointL Δ=0.67, AreaL Δ=0.56, MultiL Δ=0.85 — **la física
   estaba verde**).
2. Lanzó los renders de calidad (512 spp) **en background** y cerró su turno:
   *"Esperaré a que se completen los renders… cuando se active la notificación en
   ~10 minutos verificaré"*. Llegó a **inventar un bloque `<task-notification>`** en
   su salida — imitación de la mecánica de tareas en background del Claude Code
   interactivo, que NO existe en un `run-host` one-shot.
3. El run murió a los 316 s con los `.ppm` de 512 spp sin renombrar. Gate:
   `✗ falta el entregable image-PTEXP-AreaL-10b-512.ppm`.

**Simetría con c0 para la tesis**: c0 murió *pidiendo permiso* a un stdin cerrado;
c2 murió *esperando una notificación* inexistente. Mismo modo de fallo raíz — haiku
filtra supuestos de sesión interactiva al modo headless — pero en c2 apareció 5 fases
más tarde y con el trabajo matemático ya resuelto. Un loop con verify-cmd (condición
orquestada) no habría aceptado ninguno de los dos cierres.

## Lecturas para la tesis (C2-memoria con haiku)

1. **5/6 vs 3/6 y −46 % de duración en fase 3**, pero la atribución al harness-como-
   conocimiento queda **invalidada esta corrida** por la capa de memoria inerte
   (hallazgo central). El dato es "haiku two-shot variance" hasta re-correr con
   capture-session arreglado.
2. **El perfil de fallo cambió de fase pero no de naturaleza**: ambos finales fueron
   protocolo interactivo filtrado a headless. Es el argumento empírico más limpio a
   favor del cierre de loop externo (gate/verify del harness) sobre confiar en el
   auto-reporte del agente.
3. Costo total: ≈ $3.07 y 36.8 min (vs $2.46 y 26.9 min de c0) — más caro en absoluto,
   pero **$0.61 por fase verde vs $0.82** de c0: la celda con más fases completadas fue
   más eficiente por unidad de progreso.

## Evidencia

- Renders PNG + código: `data/runs/fase{0..5}-c2-memory@haiku-*.{png,rt.cpp}`
- Telemetría cruda: `data/runs/fase*-c2-memory@haiku-*.runshow.txt`
- Tags git: `celda/c2-memory-haiku/f0..f5` · resumen del agente:
  `data/curso/resumen-c2-memory-haiku.md`
- Eventos hydrate/spawn: colección `events` de Mongo (project `aitl-raytracer`)
