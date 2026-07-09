# conditions/ — plantillas por condición experimental

Una carpeta por condición, con los archivos que definen el trato que recibe el agente.
`scripts/run-cell.sh` los instala en `start/` antes de lanzar la corrida. El contenido
es parte del diseño experimental: **no se edita entre celdas** de una misma fase
(cambios → ADR).

| | `c0-bare/` | `c2-memory/` | `c2-orchestrator/` | `c2-local/` (exploratoria) |
|---|---|---|---|---|
| **Tratamiento** | ninguno: el modelo solo | harness como **conocimiento** (memoria + ADRs + skills) | harness **Max-Capacity**: el loop orquesta con LLM local y delega en sub-agentes | harness Max-Capacity **100 % local**: el loop implementa con un solo LLM local |
| `CLAUDE.md` | describe proyecto y gate; **no menciona** harness/MCP | + instrucciones de leer el MCP, registrar ADRs, usar memoria | + rol de orquestador/sub-agente y delegación | + protocolo de tools nativas (edit_file, no volcados) |
| `.claude/settings.json` | MCP off, sin `hydrate`; solo `capture-session` | MCP on + `hydrate` + `capture-session` | MCP on + `hydrate` + `capture-session` | — (no hay agente claude-code) |
| **Ejecución (loop)** | `aitl run --bare` | `aitl run` | `aitl run --model lmstudio --mcp` | `aitl run --model lmstudio --mcp` |
| **Ejecución (host)** | `aitl run-host --host claude-code` con esta plantilla | ídem con la suya | orquestador = loop; sub-agentes vía `run-host` | no aplica |
| **Modelo** | el del provider primario | ídem | **Nemotron Orchestrator 8B DeepSeek Q4_K_S** — ver [`c2-orchestrator/MODELO.md`](c2-orchestrator/MODELO.md) | 2º arg de `run-course.sh` (recomendado **Qwen2.5 Coder 7B**) — ver [`c2-local/MODELO.md`](c2-local/MODELO.md) |

> **`c2-local` NO es una de las 3 condiciones oficiales** (ADR-0004): es exploratoria
> (smoke-test del pipeline sin gastar Claude + evidencia H9/M59). Sus celdas no se
> comparan 1:1 con las de Claude Code.

## Por qué C0 conserva `capture-session`

Es el **instrumento de medición**, no ayuda al agente: corre al terminar la sesión
(hook Stop), el modelo nunca ve su salida, y sin él la celda C0 no produciría métricas
comparables (M31–M35). Lo que C0 **no** tiene es el tratamiento: hidratación de
memoria/ADRs y acceso al MCP.

## Las dos versiones de C2

- **`c2-memory`** — mide el valor del harness como **capa de conocimiento pasiva**
  (H3, H7, H8): el agente implementa, el harness recuerda.
- **`c2-orchestrator`** — mide el harness como **orquestador activo** (H2, H11 además
  de las anteriores): el loop del harness dirige, consulta el MCP y reparte trabajo.

Cada versión es una celda propia (rama, `run_id` y fila de métricas separados).
