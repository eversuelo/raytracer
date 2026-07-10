# THESIS-PROMPT — Caso de estudio `raytracer-metricas` (reemplaza a Schoolar)

> **Qué es este archivo**: el prompt maestro para que una sesión de Claude genere el
> capítulo/anexo comparativo de la tesis usando la campaña del raytracer como caso de
> estudio, EN LUGAR del caso Schoolar (slice T1/T3, ADR-0032). Todo lo que el caso
> Schoolar aportaba (condiciones C0/C2, métricas por corrida, supervisión humana) lo
> aporta ahora `raytracer-metricas` con mayor riqueza: gates objetivos, 4 condiciones,
> comparación multi-modelo y evidencia visual.
>
> **Cómo usarlo**: abrir una sesión de Claude en `metricas/raytracer/` y pegarle la
> sección "PROMPT" completa. Las rutas son relativas a esa raíz.

---

## PROMPT

Eres el analista de la tesis (harness AITL, hipótesis H1/H6: el harness como capa de
conocimiento/orquestación mejora costo, tiempo y tasa de éxito de agentes LLM en
tareas de ingeniería). Genera el documento
`data/curso/analisis-tesis-raytracer.md` (español, formato tesis: figuras numeradas,
tablas numeradas, citas a evidencia por ruta) con el análisis comparativo COMPLETO de
la campaña 2026-07-09 del curso raytracer. La meta final del sistema evaluado es UN
RAY TRACER FUNCIONAL que pase los gates de las 6 fases del curso IA7200-L.

### Fuentes de datos (todas en este repo, salvo Mongo)

1. `data/metricas.csv` — una fila por (celda, fase) + filas `<cell>-session` por
   sub-agente. Columna `notas` marca anomalías (suspensión, duplicados, resumen).
2. Bitácoras forenses y análisis: `data/curso/bitacora-*.md`,
   `data/curso/comparativo-c0-vs-c2-haiku.md`,
   `data/curso/hallazgo-claudemd-vs-mcp-haiku.md`,
   `data/curso/reporte-final-campana-2026-07-09.md` (consolidado; empieza por él).
3. Resúmenes escritos por los propios agentes: `data/curso/resumen-<celda>.md`.
4. Código generado por fase: `data/runs/fase<N>-<cell>-<run>.rt.cpp` y tags git
   `celda/<cell>/f<N>` (rama `curso`; `git show <tag>:start/rt.cpp`).
5. **Código humano de referencia**: `objective/Proyecto <N>/**/rt.cpp` (la solución
   del curso, escrita a mano — los agentes NUNCA la vieron; los gates solo comparan
   renders). Úsalo para la comparación código-generado vs código-humano.
6. Renders: generados en `data/runs/fase*-<cell>-*.png`; referencias del curso en
   `start/sdd/phase-0<N>/*.jpg`; renders de la solución humana en `objective/`.
7. Plantillas de condición (los CLAUDE.md que difieren entre celdas):
   `start/conditions/<cond>/CLAUDE.md` y sus `.claude/settings.json`.
8. Specs idénticas para todas las celdas: `start/sdd/phase-0<N>/spec.md` (el prompt
   de fase es la sección "## Prompt de tarea").
9. Telemetría durable en MongoDB Atlas (db `aitl`): projects `aitl-raytracer`
   (c0 + c2-memory), `aitl-raytracer-orq` (orquestador qwen),
   `aitl-raytracer-orq-sonnet` (orquestador sonnet). Colecciones: runs (tokens,
   costo, iters), events (`hydrate` con payload por fase), memory (campo **body** —
   ¡no `content`!), prompts, messages. Espejo local: `data/runs/*.runshow.txt`.

### Secciones requeridas del análisis

1. **Comparación de los Claude (y el local)** — tabla y lectura: haiku one-shot
   (c0), haiku+memoria (c2-memory), qwen2.5-coder-7b orquestando a haiku
   (c2-orchestrator), sonnet orquestando a haiku (c2-orch-claude). Ejes: fases
   verdes, duración por fase, iters/tokens/costo por nivel (orquestador vs
   sesiones), delegaciones usadas, $/fase verde. Cuidado: los `iters` de filas de
   orquestador cuentan el loop, no el trabajo haiku (usar filas `-session`).
2. **Las hidrataciones** — por celda y fase: payload de los eventos `hydrate`
   (memory/decisions), QUÉ contenido viajó realmente (prefijos de transcript de
   capture-session vs memorias `phase-NN-gate-verde-*` con cuerpo real de la celda
   sonnet), y si hay señal de que el contenido hidratado cambió el comportamiento
   (comparar fase 3 entre celdas). Documenta la corrección content→body.
3. **Las corridas** — cronología de la campaña con incidencias: reset "desde 0",
   fix ADR-0068 (--no-hydrate para C0), suspensión de 3.5 h, huérfano post-timeout,
   duplicados de capture-session. Regla: corridas inválidas se anotan, no se borran.
4. **Comparativa con imágenes (OBLIGATORIA, incrustada)** — para cada fase, una
   fila de figuras: render generado por celda (`data/runs/...png`) vs referencia
   (`start/sdd/phase-0N/*.jpg`) con la métrica del gate (media global / |Δ|) al pie.
   Dedica una figura grande a la fase 3: el render `arealight` fallido de c0
   (~0.79× oscuro) junto al de c2-memory verde y la referencia — es la evidencia
   visual del "muro". Usa Markdown `![...](ruta relativa)`.
5. **Código generado vs código humano** — para 2-3 fases clave (0, 3 y la más alta
   alcanzada): diff conceptual entre el `rt.cpp` generado (mejor celda) y el
   `objective/Proyecto N/rt.cpp` humano: estructura, idioma del muestreo (pdf,
   bases locales, visibilidad), estilo (macros/clases), LOC, y dónde el humano
   resolvió lo que tumbó a los agentes (el término de visibilidad/normalización de
   arealight en Proyecto 3). NO evalúes elegancia subjetiva: ancla cada afirmación
   a líneas concretas.
6. **En qué sesiones se equivocó el agente y por qué** — tabla de errores por
   sesión (usa las bitácoras + transcripts citados en ellas): fase, celda, sesión,
   error (compilación/calibración/protocolo), causa raíz, cómo terminó (resuelto /
   rendición / timeout). Destaca los dos fallos de protocolo one-shot (stdin
   cerrado / notificación inventada) y el sube-y-baja de thresholds de c0.
7. **Qué cambió en los CLAUDE.md** — diff comentado entre las 4 plantillas de
   condición (18 líneas de c0 → 39 de c2-memory → variantes orquestador): qué
   instrucciones agrega cada condición, cuáles siguió el agente y cuáles ignoró
   (0 llamadas MCP de haiku vs write_memory de sonnet), y los permisos/hooks de
   settings.json (incluida la asimetría git).
8. **Amenazas a la validez** — n=1 por celda, suspensión del equipo, corrección
   content→body del análisis intermedio, contaminación c0→store vía hooks (prefijos),
   gates estocásticos con tolerancias, costo no comparable entre condiciones con
   distinto número de niveles.
9. **Veredicto sobre H1/H6 con esta evidencia** — qué se puede afirmar (el loop
   externo ataca el modo de fallo real; la memoria con contenido solo existió en la
   celda sonnet), qué no (atribución de c2-memory), y el diseño mínimo de la
   siguiente campaña (n≥3, fixes de capture-session/tree-kill, condiciones
   homologadas).

### Reglas

- Cita SIEMPRE la evidencia por ruta (archivo/tag/run_id corto); no inventes números:
  todo dato debe salir del CSV, runshow, bitácoras o Mongo.
- Las imágenes van incrustadas con rutas relativas del repo (el doc vive en
  `data/curso/`, así que los renders son `../runs/...png` y las referencias
  `../../start/sdd/phase-0N/*.jpg`).
- Si la celda `c2-orch-claude@sonnet` tiene datos más recientes que el reporte
  consolidado, el CSV y Mongo mandan.
- Español técnico, tono de tesis; tablas para lo enumerable, prosa para la lectura.
- Extensión objetivo: 1500-3000 líneas de markdown incluyendo tablas y figuras.
