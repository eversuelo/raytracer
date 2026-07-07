# Manual de uso — verificación en runtime de las afirmaciones de la tesis

Este laboratorio existe para una sola cosa: **demostrar con evidencia de runtime que la
arquitectura del harness AITL ayuda en el desarrollo**. Cada fase del raytracer se
desarrolla dos veces — sin harness (C0) y con harness (C2) — y las métricas salen de la
telemetría durable del propio harness, no de impresiones subjetivas.

## 0. Requisitos

```bash
which aitl            # CLI global instalado (npm i -g en AITL-Harness-JS)
aitl check-db         # MongoDB accesible (lee ~/.aitl/config.json)
aitl models           # al menos un LLM configurado para C0/C2 con `aitl run`
```

Para las sesiones Cara B (Claude Code como host), los hooks de `.claude/settings.json`
inyectan memoria (`hydrate`) y capturan la sesión (`capture-session`) automáticamente.

## 1. Las condiciones experimentales

> **Actualización (ADR-0004, 2026-07-06)**: C2 se desdobla en dos versiones y el ciclo
> por celda está automatizado en `../scripts/run-cell.sh` (plantillas de condición en
> `../start/conditions/`; resumen generado en `../RESUMEN-METRICAS.md`).

| Condición | Comando (lo lanza `run-cell.sh`) | Qué prueba |
|---|---|---|
| **C0** `c0-bare` | `aitl run "<tarea>" --project aitl-raytracer --bare --verify-cmd "make && ./check.sh"` | El modelo solo: sin hidratación de memoria, sin skills, sin gates |
| **C2m** `c2-memory` | `aitl run "<tarea>" --project aitl-raytracer --verify-cmd "make && ./check.sh"` | El harness como conocimiento: memoria + ADRs + skills + gates |
| **C2o** `c2-orchestrator` | `aitl run "<tarea>" --project aitl-raytracer --model lmstudio --mcp --verify-cmd "make && ./check.sh"` | El harness Max-Capacity: el loop orquesta (LLM local, ver `../start/conditions/c2-orchestrator/MODELO.md`) y delega en sub-agentes |
| **Cara B** (método `host`) | `aitl run-host "<tarea>" --project aitl-raytracer --host claude-code --cwd ../start` | El harness como memoria de un host externo (Claude Code como agente) |

El gate mínimo es `make` (compila); cada spec añade sus checks de imagen (dimensiones,
comparación contra referencia) — cuando la fase los defina como script, pásalo en
`--verify-cmd "make && ./check.sh"`.

## 2. Protocolo por fase (repetir para F0…F5)

1. **Spec primero (SDD)**: la spec canónica vive en `../start/sdd/phase-0N/spec.md` (junto al
   enunciado original del curso y sus imágenes de referencia). El investigador la revisa
   y aprueba (`status: approved`). Persístela:
   ```bash
   aitl sdd --project aitl-raytracer --spec ../start/sdd/phase-0N/spec.md
   ```
2. **Corrida C0**: `git checkout fase-N/c0-bare` (parte del resultado aceptado de la fase
   N-1) y lanza la tarea con `--bare`. Anota el `run_id`.
3. **Corrida C2**: `git checkout fase-N/c2-harness` y lanza la misma tarea sin `--bare`.
   En fases ≥2, verifica en el evento `hydrate` que los ADRs de fases previas entraron al
   contexto (esa es H3 en acción).
4. **Métricas**: `aitl run-show <run_id> --project aitl-raytracer` para ambos runs:
   iteraciones, tokens ↑↓, tool calls, vetos de gates, `supervision_minutes`, eventos
   `verify`/`compaction`/`retry`.
5. **Cierre de fase**: registra el ADR de la fase (`record_decision`, ledger propio de
   `aitl-raytracer`), espejo en `docs/adr/`, y mergea la rama ganadora según el criterio
   de la spec. Crea las ramas de la fase N+1 desde ese punto.

## 3. Qué afirmación de la tesis verifica cada cosa (H → runtime)

| Hipótesis | Verificación en este laboratorio |
|---|---|
| **H1** (harness mejora velocidad/calidad/trazabilidad) | Comparar `run-show` C0 vs C2 por fase: iteraciones, retrabajo, tests verdes al primer intento |
| **H3** (recuperación selectiva de ADRs reduce retrabajo) | En F2+, el evento `hydrate` de C2 debe traer los ADRs de fases previas; C0 re-explica todo desde cero — cuenta los tokens que cuesta |
| **H4** (gestión activa de contexto) | Eventos `compaction` en las fases largas (F4/F5) sin degradación de calidad |
| **H5** (verificación automatizada reduce regresiones) | `--verify-cmd "make"` (+ checks de imagen de la spec): eventos `verify` fallidos que el loop corrige solo vs. regresiones que C0 deja pasar |
| **H6** (terminación/presupuesto explícitos) | `maxIters` + deadline de streaming: ninguna corrida cuelga; iteraciones improductivas visibles en eventos |
| **H7** (feedforward+feedback) | CLAUDE.md/specs (feedforward) + memoria de fases previas (feedback) juntos en C2 |
| **H8** (fallos recurrentes → guías) | Cuando un bug se repita entre fases, escribir memoria tipo `bug` y verificar que la fase siguiente no lo repite |
| **H9** (contratos estables multi-modelo) | Repetir una fase con otro provider (`--model lmstudio` vs `anthropic`) sin cambiar specs ni tools |
| **H10** (supervisión humana) | Corridas con `--ask`: cada aprobación queda auditada; `aitl intervene` cuando el humano corrige; comparar `supervision_minutes` |
| **H11** (roles componibles) | Fases 4–5 con `--roles architect,qa`: el DecisionBrief debe objetar violaciones de la spec |

**Métricas de la tesis (cap. 4, `tab:metrics`)**: velocidad = duración del run; calidad
funcional = tests verdes/verify; eficiencia = tokens e iteraciones; supervisión =
`supervision_minutes`; trazabilidad = cadena spec→run→ADR→memoria visible con
`GET /api/runs/:id/graph` (UI del harness, pestaña Runs). El catálogo exhaustivo
(M1–M61, con campo/colección exacto por métrica y contabilidad de tokens) está en
`../METRICAS.md`.

## 4. Comparación de imágenes (gate de calidad específico del raytracer)

Cada spec de fase define renders de referencia (en `../start/sdd/phase-0N/`). El check de la
fase debe validar dimensiones + hash del PPM (o distancia RMS contra la referencia si la
spec lo pide) para que `--verify-cmd "make && ./check.sh"` funcione como gate objetivo
sin ojo humano.

## 5. Higiene experimental

- **Nunca** implementes en `main` — solo specs, manual, config.
- Un `run_id` por celda del diseño (fase × condición); si una corrida se invalida,
  regístralo (`aitl intervene --project aitl-raytracer`) en vez de borrarla.
- Los prompts de tarea deben ser IDÉNTICOS entre C0 y C2 (cópialos de la spec).
- El ledger de ADRs de este proyecto es propio (`aitl-raytracer`, empieza en 0001) —
  no mezclar con el de `aitl-js`.
