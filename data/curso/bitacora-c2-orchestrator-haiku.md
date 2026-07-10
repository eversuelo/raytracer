# Bitácora del investigador — celda `c2-orchestrator@haiku`

> Redactada por el orquestador de la campaña (Claude) el 2026-07-09. Fuentes: las de
> siempre (CSV, logs por fase + gate logs, runs de Mongo bajo `aitl-raytracer-orq`,
> transcripts de sub-agentes, resumen del propio agente) más los monitores en vivo de
> renders/delegaciones/escrituras MCP de esta sesión. Par de `bitacora-c0-bare-haiku.md`
> y `bitacora-c2-memory-haiku.md`.

## Condiciones de la corrida

- **Arquitectura**: loop nativo del harness (`aitl run --model lmstudio --mcp
  --verify-cmd`) con **qwen2.5-coder-7b-instruct** local (LM Studio, ctx 20 000) como
  ORQUESTADOR; implementación delegada a sub-agentes **claude-code haiku** vía
  `run-host` (máx. 3 delegaciones/fase). Protocolo del prompt: delegar → verificar
  gate → re-delegar citando el error.
- **Project MCP propio**: `aitl-raytracer-orq` (aislado del store de c0/c2-memory;
  registrado en la jerarquía software `raytracer` durante la corrida). El prompt de
  fase NO entra al contexto de qwen (viaja en `FASE-PROMPT.txt`).
- **Incidente ambiental**: la máquina se **suspendió ~3.5 h** (≈16:04→19:28) en plena
  fase 3. Los procesos sobrevivieron y retomaron al despertar, pero el deadline global
  (reloj real) ya había expirado → el curso cerró al terminar la fase 3. Todas las
  duraciones de fase 3 en el CSV están anotadas (columna notas) con el tiempo
  despierto estimado.
- **Fuga de hooks (corregida post-celda)**: `settings.json` de la plantilla llevaba
  `--project aitl-raytracer` hardcodeado en `capture-session`/`hydrate` — las
  escrituras vía hook de esta celda cayeron al store viejo (docs vacíos, impacto nulo).
  Fix commiteado en `47a655a` para futuras corridas.

## Resultado global

**3/6 fases en verde; fase 3 murió por tope (timeout), no por rendición.** Tiempo
despierto total ≈ 60 min (el tope); reloj de pared 266 min por la suspensión.

| Fase | orq (qwen) | sub-agentes haiku | gate | dur despierta |
|---|---|---|---|---|
| 0 | 2 iters, 32 k tok | `fe061794` (154 s, exit≠0 pero implementó) | ✅ | 280 s |
| 1 | 2 iters, 33 k tok | `a8e0c411` (298 s, 31 iters, $0.35) | ✅ | 441 s |
| 2 | 2 iters, 33 k tok | `8df78c3e` (141 s, 20 iters, $0.22) | ✅ | 300 s |
| 3 | 4+ iters (matado) | `cd9776f4` (893 s, 69 it, $1.38) + `f8b4d9a7` (760 s, 62 it, $1.16) + `b0ee8d06` (≈17 min despierto, 62 it, $1.27, matado a medio editar) | ❌ timeout | ≈42 min |

Costo sub-agentes: **≈ $4.38** (+ $0.10 del agente del resumen). El orquestador local
cuesta $0 en API (≈33 k tokens/fase de qwen).

## La fase 3: la mecánica del orquestador SÍ operó — y no alcanzó

Única celda de la campaña donde el ciclo completo delegar→verificar→re-delegar se
ejecutó de verdad:

1. **Delegación 1** (`cd9776f4`, 69 iters): implementó los dos métodos y chocó con el
   mismo muro que c0 y c2-memory — `arealight` con brillo sistemáticamente bajo
   (**factor ~0.79× ≈ π/4**, firma de un error de normalización del pdf). Cerró sin
   converger; qwen verificó el gate: rojo.
2. **Delegación 2** (`f8b4d9a7`, 62 iters): debugging diferencial (usó `solidangle`
   verde como referencia numérica), instrumentó y desinstrumentó el código; llevó
   arealight de Δ=23.47 → **3.15** (umbral 2.00) y cerró declarando el código
   "conceptualmente correcto" — otra autoevaluación complaciente. Gate: rojo.
3. **Delegación 3** (`b0ee8d06`): interrumpida por la suspensión de la máquina; al
   despertar retomó y llegó a probar la escena 2A1P, pero el timeout de la celda la
   mató **a medio editar rt.cpp** — por eso el gate final registró
   `'./rt solidangle 32 1L' terminó con error` (artefacto del kill, no regresión real).

**Límite observado del orquestador 7B**: su resumen del error al re-delegar fue
genérico — *"La diferencia media es mayor que lo permitido"* — sin el valor (3.15),
el umbral ni el modo afectado. Un resumen con esos datos habría ahorrado al segundo
y tercer sub-agente el redescubrimiento del contexto. Ésta es la mejora de prompt/
modelo más barata para la siguiente corrida.

## Presión de contexto (medida)

Cada turno de qwen envía ~16.0-16.5 k tokens de 20 000 (80-83 %): el grueso son los
esquemas de las **52 tools MCP montadas, de las que solo usa `shell`**. Con 2 turnos
por fase cabe; la fase 3 llegó a turnos 4+ (zona de desborde probable) — el loop
sobrevivió, pero es plausible que el recorte de contexto explique la pobreza del
resumen de error. Recomendaciones: correr el orquestador **sin `--mcp`** (solo
necesita shell) o cargar qwen con `--context-length 32768`.

## Otras observaciones del transcript/monitores

- El sub-agente de fase 0 (`fe061794`) terminó con exit≠0 y `status=error` **después
  de completar el trabajo** (el gate salió verde) — los tokens de esa sesión no se
  parsearon; en el CSV su fila va anotada.
- El sub-agente de fase 1 **commiteó por su cuenta** (`1ecc94e "Implementación y
  verificación de Fase 01…"`): la plantilla del orquestador pre-aprueba git, a
  diferencia de c0/c2-memory. Decidir si se quiere esa asimetría entre condiciones.
- Fuga interactiva de haiku también aquí: un *"¿Quieres que continúe con el commit y
  push?"* al vacío, dentro del log del orquestador.
- Los sub-agentes hicieron **0 llamadas MCP** y el store `-orq` cerró con 3 resúmenes
  de sesión **vacíos** y 0 ADRs — el bug de `capture-session` y la apatía MCP de haiku
  se replican tal cual en esta condición.
- Un sub-agente huérfano (la 3ª delegación, re-parentado tras el kill) siguió
  quemando API tras el fin del curso; se detectó por el monitor de renders y se mató
  a mano. El harness debería matar el árbol completo de `run-host` al expirar el
  timeout de fase (pendiente para aitl-js).

## Lecturas para la tesis

1. **El cierre de loop externo funcionó como mecanismo**: detectó los dos cierres
   complacientes (gates rojos objetivos) y re-delegó — exactamente lo que c0 y
   c2-memory no tuvieron. Que no bastara para cruzar el umbral habla de la dificultad
   del bug y de la calidad del feedback (resumen de error pobre), no de la mecánica.
2. **El muro de la fase 3 es real y reproducible**: tres celdas, cinco sesiones haiku
   distintas, mismo defecto (normalización del pdf de arealight). Solo c2-memory lo
   cruzó, con un diagnóstico geométrico afortunado. Es el mejor "benchmark natural"
   que dejó la campaña para comparar condiciones.
3. **Costo del aislamiento**: la orquestación duplicó la duración de las fases verdes
   (280/441/300 s vs 85/219/185 s de c2-memory) — el precio de verificación explícita
   + arranque de sub-agente por fase con un 7B local ruteando.
4. La celda demuestra la **viabilidad técnica** del patrón "LLM local orquesta,
   agentes en la nube implementan, todo persistido bajo un project aislado" —
   incluida la supervivencia a una suspensión de 3.5 h.

## Evidencia

- CSV: filas `c2-orchestrator@haiku` (loop qwen) + `c2-orchestrator@haiku-session`
  (sesiones sub-agente, auto-recolectadas; ahora integrado en
  `scripts/collect-sessions.mjs` + `run-course.sh`).
- Runs Mongo: project `aitl-raytracer-orq` (4 runs lmstudio + 6 host:claude-code).
- Tags git: `celda/c2-orchestrator-haiku/f0..f3` · resumen del agente:
  `data/curso/resumen-c2-orchestrator-haiku.md`.
