# Hallazgo — ¿Qué mejoró realmente en C2: el harness, la memoria… o nada de eso?

> Análisis transversal de la campaña haiku 2026-07-09 (celdas c0-bare y c2-memory
> completas; c2-orchestrator en curso). Fuentes: eventos `hydrate` y colecciones de
> Mongo, transcripts crudos de las 10 sesiones, diffs de las plantillas de condición,
> bitácoras forenses de ambas celdas. Pregunta del investigador: *¿hubo mejora real por
> el harness, o solo por el uso de memoria?*

## TL;DR

**Ninguna de las dos.** La ventaja de c2-memory (5/6 fases vs 3/6, fase 3 resuelta en
la mitad del tiempo) **no puede atribuirse ni al harness como conocimiento ni al uso
de memoria**: el canal activo (tools MCP) tuvo cero uso y el pasivo (hidratación)
solo inyectó prefijos de transcript sin los diagnósticos útiles. Los únicos diferenciales reales entre condiciones fueron ~21 líneas de
CLAUDE.md y la presencia (ignorada) de 52 tools MCP en el contexto. Con n=1, la
explicación más defendible del delta es **varianza entre corridas**. El hallazgo firme
de la campaña es otro: el modo de fallo compartido (protocolo interactivo filtrado a
headless) y que el único mecanismo que lo ataca es el cierre de loop **externo** al
agente — exactamente lo que mide la celda c2-orchestrator.

## 1. Qué difería *en papel* entre condiciones

| Canal del "harness como conocimiento" | c0-bare | c2-memory |
|---|---|---|
| CLAUDE.md | 18 líneas: reglas de trabajo | 39 líneas: reglas + identidad MCP + mandato de usar `list_decisions`/`search_memory`/`record_decision`/`write_memory` |
| Hidratación del prompt (`run-host`) | OFF (`--no-hydrate`, ADR-0068) | ON (1 evento por fase) |
| Hook `UserPromptSubmit: aitl hydrate` | no (plantilla c0 no lo trae) | sí (disparó en cada sesión) |
| Servidor MCP (`.mcp.json`, 52 tools) | no montado | montado en las 6 fases |
| Hook `Stop: capture-session` | sí (¡también en c0!) | sí |

## 2. Qué difería *en la práctica*: casi nada

Medición dura, fase por fase:

1. **Lo hidratado estaba vacío.** Los 6 eventos `hydrate` de c2 reportan
   `memory: 5-6, decisions: 0`. Esos 5-6 docs son resúmenes `session-<id>` escritos
   por `capture-session` con **0 caracteres de contenido** (bug del harness — en la
   campaña sonnet de julio-07 salían con detalle de implementación). El preámbulo
   inyectado al prompt era una lista de bullets vacíos.
2. **El agente nunca usó el MCP.** 0 llamadas a tools `mcp__*` en las 6 sesiones de
   c2-memory — ni lectura (`search_memory`, `list_decisions`) ni escritura
   (`record_decision`, `write_memory`) — pese al mandato explícito del CLAUDE.md.
   Consecuencia: `decisions: 0` toda la celda; el ledger de ADRs del proyecto quedó
   en blanco.
3. **Sin memoria con contenido y sin ADRs, la fase N no heredó de la N−1 nada** que
   c0 no heredara también (el código en el working tree). Los dos únicos vehículos de
   transferencia de conocimiento del diseño C2 estaban rotos (síntesis vacía) o sin
   usar (tools ignoradas).

Conclusión intermedia: **la condición C2-memoria degeneró, de facto, en
"c0 + CLAUDE.md más largo + tools presentes pero ignoradas"**.

## 3. ¿Entonces de dónde salió el 5/6 vs 3/6?

Hipótesis rivales, ordenadas por plausibilidad:

- **H-varianza (favorita):** haiku es estocástico y n=1 por celda. El curso entero
  pivotó en UN momento: el diagnóstico del bug de visibilidad/dirección en fase 3.
  c2 lo enunció como geometría ("la dirección va HACIA x′, no desde x′"), lo corrigió
  una vez y pasó (517 s); c0 enunció el diagnóstico correcto equivalente ("¿golpeó la
  esfera de luz?, no ¿golpeó exactamente x′?") **y lo abandonó**, degenerando en 35
  ediciones de thresholds (962 s, rojo). Misma capacidad, distinta suerte en qué
  insight se estabilizó. Nada del harness intervino en ese momento.
- **H-priming del CLAUDE.md:** las 21 líneas extra de c2 enmarcan el proyecto como
  laboratorio SDD con estado durable ("las decisiones ya tomadas mandan"). Aun sin
  usar las tools, ese framing pudo inducir un estilo más disciplinado — consistente
  con que c2 fue más eficiente en turnos en 5 de 6 fases (p. ej. fase 0: 13 turnos vs
  25). Indistinguible de H-varianza sin repeticiones.
- **H-tools-presentes:** 52 esquemas de tools MCP en contexto podrían influir (a favor
  o en contra). Sin señal medible: el agente ni las mencionó.

Con una corrida por celda no hay forma estadística de separar estas hipótesis. Lo que
sí se puede afirmar: **el mecanismo causal que la tesis propone para C2 (conocimiento
durable hidratado + registrado) no operó en esta campaña**, así que esta corrida no
es evidencia a favor de H1/H6 — es una corrida a repetir tras los fixes.

## 4. El hallazgo firme que la campaña sí dejó

Los DOS finales de celda fueron el mismo defecto, en fases distintas y con el trabajo
técnico esencialmente resuelto:

| | c0-bare (fase 3) | c2-memory (fase 5) |
|---|---|---|
| Estado técnico al morir | `solidangle` verde; `arealight` a 3.3 del umbral | física completa en verde; faltaba renombrar los 512 spp |
| Último acto del agente | *"¿Debería continuar debugeando…?"* a un stdin cerrado | *"Esperaré la notificación en ~10 min"* + un `<task-notification>` inventado |
| Naturaleza del fallo | protocolo interactivo en entorno headless | ídem (hasta alucinó la mecánica de background del Claude Code interactivo) |

El precursor estaba a la vista: ya en fase 1 de c2, el agente intentó `sleep 30` para
"esperar renders" y el sandbox se lo bloqueó. **Haiku no pierde contra la física del
raytracer; pierde contra la mecánica one-shot.** Ningún preámbulo de memoria arregla
eso: se arregla con un loop externo que verifique y re-delegue — el gate
`--verify-cmd` + delegaciones acotadas del harness nativo. Esa es precisamente la
condición c2-orchestrator (en curso al cierre de este documento: fase 0 verde en
280 s con el ciclo delegar→verificar→avanzar funcionando).

## 5. Qué hace falta para poder afirmar "mejora por el harness"

1. **Mejorar `capture-session`** (hoy guarda el prefijo truncado del transcript, no
   una síntesis): sin resúmenes con los diagnósticos/soluciones, C2-memoria no tiene
   mecanismo útil que hidratar.
2. **Forzar el uso del MCP en el prompt de fase** (imperativo, con verificación: "antes
   de codificar, cita el resultado de `list_decisions`") o aceptar que haiku no usa
   tools por iniciativa y medir esa condición con un modelo mayor.
3. **Decidir el hook `capture-session` en c0**: hoy también escribe al store desde el
   baseline (docs vacíos esta vez; con el fix serían fuga c0→c2).
4. **n≥3 por celda** antes de leer deltas de fases/tiempos como efecto.
5. **Comparar contra c2-orchestrator**, que ataca el modo de fallo real observado
   (cierre de loop externo), no el hipotético (falta de conocimiento).

## Anexo — números de referencia

| Métrica | c0-bare | c2-memory |
|---|---|---|
| Fases verdes | 3/6 | 5/6 |
| Duración total | 26 min 56 s | 36 min 49 s |
| Turnos / tokens | 180 / 13.6 M | 223 / 15.5 M |
| Costo total / por fase verde | $2.46 / $0.82 | $3.07 / $0.61 |
| Llamadas MCP del agente | n/a (sin MCP) | **0** |
| ADRs escritos | 0 | **0** |
| Contenido hidratado | n/a (`--no-hydrate`) | prefijos de transcript (~2000 chars c/u, sin diagnósticos) |
