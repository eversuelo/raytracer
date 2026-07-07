# ADR-0003 — Catálogo de métricas M1–M61: Tabla 4.3 de la tesis anclada a la telemetría real del harness

- **Estado**: accepted · v1 · 2026-07-06
- **Proyecto**: `aitl-raytracer` (ledger propio)
- **Componentes**: `../METRICAS.md`, `../sdd/TRAZABILIDAD.md`, `MANUAL.md`

## Contexto

Antes de las primeras corridas C0/C2 hacía falta fijar **qué** se mide y **dónde** se
captura cada métrica, para que la matriz de `TRAZABILIDAD.md` se llene con datos
comparables. Se inventarió:

- **(a) la telemetría real de AITL-Harness-JS**: colección `runs` (`token_usage`, `iters`,
  `tool_calls`, `gate_denials`, `supervision_minutes`), 21 tipos de evento (`verify`,
  `hydrate`, `compaction`, `retry`, `approval`, `review`/`role_veto`/`deliberation`,
  `human_intervention`…), salida de `aitl run-show`, colecciones
  `prompts`/`messages`/`decisions`/`memory`/`mcp_tool_calls`/`audit`, hooks Cara B
  (`capture-session` con `host_meta.cost_usd` y desglose de caché) y
  `docs/token-accounting.md`;
- **(b) el marco de la tesis**: Tabla 4.3 (`tab:metrics`, 9 dimensiones con mapeo
  ISO/IEC 25010:2023), hipótesis H1–H11 y variables del cap. 4.

Hallazgo: el raytracer aún **no aparece en el `.tex` de la tesis** — es complemento de
medición de Schoolar, por lo que debe heredar `tab:metrics` sin inventar dimensiones nuevas.

## Decisión

1. El catálogo canónico vive en `metricas/raytracer/METRICAS.md`: **61 métricas** numeradas
   M1–M61 en 7 grupos — primarias de `run-show` (M1–M15), eventos del loop (M16–M25),
   contabilidad de tokens con costo ponderado `fresh×1.0 + cache_creation×1.25 +
   cache_read×0.1` (M26–M30), Cara B (M31–M35), conocimiento durable/trazabilidad MCP
   (M36–M42), específicas del raytracer (M43–M48) y derivadas comparativas C0 vs C2
   (M49–M61).
2. Adaptaciones de `tab:metrics` al dominio: calidad funcional = gate `make` + checks de
   imagen (hash/RMS vs referencia de la spec); estabilidad = re-ejecutar checks de fases
   0..N−1; mantenibilidad = warnings `-Wall` + tamaño de diff; seguridad (tenant) = **N/A**,
   solo Schoolar.
3. `tokens.input` crudo **nunca** se compara entre condiciones — se reportan las 4 vistas
   de token-accounting (output, input único, costo ponderado, throughput).
4. Cada dimensión de la Tabla 4.3 y cada hipótesis H1–H11 tiene métricas asignadas en el
   mapeo del §8 del catálogo; la hoja de métricas por celda (plantilla §9) alimenta
   `TRAZABILIDAD.md` y el cap. 4.
5. Los prompts usados para construir el catálogo quedaron en la colección `prompts` del
   proyecto (ids `6a4c51d6…`, `6a4c51e6…`, `6a4c51ec…`).

## Consecuencias

- Las corridas de F0 en adelante ya saben exactamente qué columnas llenar y de qué
  campo/colección sale cada valor; la comparación C0 vs C2 queda protegida del artefacto
  de caché en `tokens.input`.
- Las dos dimensiones Schoolar-específicas quedan explícitamente fuera (seguridad) o
  adaptadas (mantenibilidad), evitando celdas imposibles.
- **Pendiente**: escribir los `check.sh` de imagen por fase (M44) al aprobar cada spec, y
  decidir la tarifa de tokens para M28/M30 al fijar el modelo.
