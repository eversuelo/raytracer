# Modelo del agente local (C2-local) — elección

A diferencia de C2-orquestador (donde el modelo local solo RUTEA y delega el código a
sub-agentes Claude), aquí el modelo local **escribe el C++ él mismo** con las tools
nativas del loop. El criterio dominante se invierte: primero **capacidad de código**,
después fidelidad de tool-calling (sigue haciendo falta: cada edición es una tool call
`edit_file`/`shell`).

## Candidatos (mismo inventario LM Studio que `c2-orchestrator/MODELO.md`)

| Modelo | Quant | Tamaño | Código | Veredicto para IMPLEMENTAR |
|---|---|---|---|---|
| **Qwen2.5 Coder 7B Instruct** | Q4_K_M | 4.36 GB | ✅ especialista | ✅ **ELEGIDO** — el único coder específico del inventario; la misma tabla que lo descartó como router lo recomienda en este asiento. Formato de tools qwen2 más frágil: el prompt de la celda le fija el protocolo (edit_file, no volcados) |
| Qwen3 4B 2507 | Q4_K_M | 2.33 GB | bueno p/ 4B | 🥈 fallback si la latencia o la RAM dominan — tool-calling más moderno (qwen3), menos músculo de código |
| Nemotron Orchestrator 8B | Q4_K_S | 4.47 GB | medio | ❌ entrenado para rutear, no para escribir C++ |
| Meta Llama 3.1 8B / Gemma 4 / R1 distill | — | — | — | ❌ mismos descartes que en `c2-orchestrator/MODELO.md` |

## Ajustes de runtime (RTX 4050, 6 GB)

- `LMSTUDIO_MAX_CONTEXT` ≥ 16384 si cabe (specs + código de rt.cpp crecen por fase);
  con Flash Attention + KV cache Q8_0 el 7B a 16k entra justo — si hay offload a RAM,
  bajar a 8192 antes que degradar tok/s.
- Registrar el id exacto del modelo en la columna `modelo` de la hoja de métricas: la
  celda se nombra `c2-local@<id>` y alimenta H9/M59 (portabilidad multi-modelo).

## Estatus experimental

**Condición exploratoria, NO es una de las 3 oficiales** (c0-bare / c2-memory /
c2-orchestrator, ADR-0004): sus celdas no son comparables 1:1 con las de Claude Code
(agente distinto, no solo harness distinto). Sirve para (a) smoke-test del pipeline
completo sin gastar uso de Claude y (b) el techo de lo que un local 7B logra con el
harness Max-Capacity — evidencia de H9.
