# Modelo del orquestador (C2-orquestador) — evaluación y elección

El orquestador NO escribe el C++ (eso lo hacen los sub-agentes): su trabajo es rutear
la tarea, emitir **tool calls válidos** contra el MCP `aitl-js`, decidir cuándo delegar
(`run-host`) y cerrar el loop cuando el gate (`--verify-cmd`) queda verde. El criterio
dominante es por tanto **fidelidad de function-calling y seguimiento de protocolo**,
no capacidad de código.

## Candidatos (LM Studio local, `Models.png` 2026-07-06)

| Modelo | Quant | Tamaño | Tool-calling | Veredicto para orquestar |
|---|---|---|---|---|
| **Nemotron Orchestrator 8B DeepS.** (mradermacher) | **Q4_K_S** | 4.47 GB | ✅ nativo | ✅ **ELEGIDO** — entrenado específicamente para orquestación (ruteo de tareas y herramientas) sobre base qwen3; el mejor quant de los dos builds Nemotron |
| Nemotron Orchestrator 8B (MaziyarPanahi) | Q3_K_M | 3.84 GB | ✅ nativo | ⚠️ mismo modelo pero Q3: en 8B el Q3 degrada visiblemente la validez del JSON de tool calls — solo como fallback si falta RAM |
| Qwen3 4B 2507 | Q4_K_M | 2.33 GB | ✅ nativo | 🥈 **Runner-up** — generación reciente, tool-calling sólido, 2× más rápido y la mitad de memoria; úsalo si la latencia del orquestador domina M1/M48. Riesgo: profundidad de planeación a 4B en F4/F5 |
| Qwen2.5 Coder 7B Instruct | Q4_K_M | 4.36 GB | parcial | ❌ especializado en ESCRIBIR código — talento desperdiciado en el asiento del router; generación anterior (qwen2) con formato de tools más frágil |
| DeepSeek R1 0528 Qwen3 8B | Q3_K_L | 4.13 GB | frágil | ❌ modelo de razonamiento: las trazas `<think>` inflan tokens y latencia en cada vuelta del loop (contamina M2–M4/M26) y el tool-calling en GGUF razonadores es errático; además Q3 |
| Meta Llama 3.1 8B Instruct | Q4_K_M | 4.58 GB | básico | ❌ generación 2024; tool-calling notablemente más débil que la línea qwen3 |
| Gemma 4 E4B | Q4_K_M | 5.89 GB | débil | ❌ su gracia es la visión (podría MIRAR renders), pero el gate del experimento es hash/RMS a propósito (objetivo, sin ojo); function-calling débil para orquestar |
| Nomic Embed Text v1.5 | — | 80 MB | — | N/A — es un modelo de embeddings, no un LLM (útil en otra silla: vector search de la memoria del harness) |

## Decisión

**Nemotron Orchestrator 8B DeepSeek — Q4_K_S (mradermacher, 4.47 GB).**
Es el único candidato entrenado para exactamente este rol, en el quant que preserva la
fidelidad de las tool calls, con footprint razonable. Registrar el modelo usado en cada
corrida (columna `modelo` de la hoja de métricas) — es además la palanca de **H9/M59**
(portabilidad multi-modelo: orquestador local + sub-agentes Claude).

## Configuración

```bash
lms server start                                # LM Studio en http://localhost:1234/v1
# id exacto (ya fijado en ~/.aitl/config.json → global para el CLI):
export LMSTUDIO_MODEL="nemotron-orchestrator-8b-deepseek-v3.2-speciale-distill"
aitl models                                     # debe mostrar ● lmstudio ← activo
```

> Nota de medición: si LM Studio es viejo y omite el chunk de usage en streaming, los
> turnos streameados reportan 0 tokens (nota del CLI) — para el experimento corre SIN
> `--stream` y verifica que `run-show` reporte tokens > 0 en la primera celda.
