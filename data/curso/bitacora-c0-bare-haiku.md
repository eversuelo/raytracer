# Bitácora del investigador — celda `c0-bare@haiku`

> Redactada por el orquestador de la campaña (Claude) el 2026-07-09, a partir de:
> `data/metricas.csv`, logs por fase (`data/logs/curso-c0-bare-haiku-*-20260709-132757.log`
> + `.gate.log`), los transcripts crudos de las 4 sesiones de Claude Code
> (`~/.claude/projects/...-start/*.jsonl`) y el resumen del propio agente
> (`resumen-c0-bare-haiku.md`). Complementa —no sustituye— ese resumen: aquí va el
> análisis forense de errores que el agente no cuenta de sí mismo.

## Condiciones de la corrida

- **Campaña limpia ("desde 0")**: CSV, evidencia y store Mongo de `aitl-raytracer`
  reseteados antes de la celda (respaldo en `metricas/archivo-raytracer-20260709/`).
- **C0 puro**: primera celda corrida con el fix ADR-0068 (`aitl run-host --no-hydrate
  --no-spec-synthesis`) — el agente NO recibió memoria/ADRs del store.
  Verificado en el run de fase 0: `event_counts = {spawn: 1}`, sin evento `hydrate`.
  Matiz: el hook `Stop: aitl capture-session` de la plantilla (presente en TODAS las
  condiciones por diseño) sí escribió un doc `session-<id>` por sesión al store, con
  ~2000 chars en el campo `body` — el PREFIJO truncado del transcript (narrativa de
  apertura "voy a leer la spec…"), no una síntesis. [CORRECCIÓN 2026-07-09: una
  versión previa de esta bitácora los reportó "vacíos (0 chars)" por leer el campo
  equivocado del esquema (content en vez de body).] La fuga c0→store existe pero se
  limita a ese prefijo de bajo valor. Mejora pendiente del harness: capture-session
  debería sintetizar, no truncar.
- Modelo agente: `claude-haiku-4-5-20251001` · tope global 60 min · gates objetivos
  independientes del agente (stdin cerrado).
- **Incidente previo (excluido de esta campaña)**: una corrida anterior del mismo día
  quedó INVÁLIDA porque `run-host` hidrataba el prompt con soluciones de fases previas
  (llegó a incluir la solución de fase 2 en el baseline). Quedó anotada con
  `aitl intervene` antes del reset; esta celda es la corrida válida.

## Resultado global

**Llegó a fase 3 de 5. Fases 0–2 en verde a la primera; fase 3 gate rojo por
calibración del modo `arealight`.** Total 26 min 56 s de 60 disponibles.

| Fase | run_id | turns | tokens | costo USD | gate | dur |
|---|---|---|---|---|---|---|
| 0 — intersección/debug | `030e1f87` | 25 | 1 086 189 | 0.234 | ✅ | 150 s |
| 1 — MC directo (3 muestreadores) | `767978b4` | 34 | 1 700 684 | 0.339 | ✅ | 282 s |
| 2 — fuente puntual | `5104e70e` | 18 | 660 791 | 0.196 | ✅ | 166 s |
| 3 — importance sampling + multi-emisor | `793072b1` | 103 | 10 195 955 | 1.692 | ❌ | 962 s |
| **Total** | | **180** | **13 643 619** | **≈ 2.46** | 3/4 | 1 560 s |

La fase 3 consumió **el 75 % de los tokens y el 69 % del costo** de toda la celda.

## Errores y fricciones por fase (del transcript crudo)

### Fase 0 (7 ediciones, 13 bash, 6 fricciones) — limpia
- Primer intento del gate falló por un detalle de contrato, no de matemáticas:
  `./rt normal` no dejaba `image.ppm` en la raíz (`✗ check phase-00`). Corregido al toque.
- Fricción de herramienta: intentó `Read` sobre un directorio (EISDIR) y un
  `git add && git commit` que el sandbox de permisos bloqueó (C0 no pre-aprueba git;
  el commit lo hace el script del curso, no el agente). No afectó el resultado.

### Fase 1 (7 ediciones, 20 bash, 9 fricciones) — un bug real, bien resuelto
- Error de compilación inicial en `localBasis` (binding de referencia no-const).
- Primer render fuera de tolerancia: media (48.1, 49.2, 46.6) vs (36.4, 36.2, 35.0)±4 —
  el estimador sobreiluminaba ~33 %. Lo diagnosticó y corrigió; el gate cerró verde.

### Fase 2 (5 ediciones, 9 bash, 3 fricciones) — la más limpia
- Sin errores de fondo; solo el bloqueo rutinario de `git add` por permisos.
  18 turnos, la fase más barata ($0.20).

### Fase 3 (35 ediciones, 51 bash, 24 fricciones) — el muro
Cronología del fallo:
1. Implementó los dos métodos de muestreo (área y ángulo sólido) y los 5 renders.
2. **Bug central: el test de visibilidad del rayo de sombra en `arealight`.** Primera
   versión comprobaba "¿el rayo golpeó exactamente el punto x′?" con un threshold
   arbitrario → renders oscuros (media ~64). Lo relajó a 0.01 → sobreiluminó (~99,
   ratio 1.55×). Él mismo lo articuló: *"mi check de visibilidad debe ser '¿el rayo
   golpeó la esfera de luz?' no '¿golpeó exactamente xp?'"*.
3. **Sube-y-baja entre modos**: cada ajuste que acercaba `arealight` a la referencia
   descalibraba `solidangle` o viceversa (llegó a ver SA-1L en (105.7,105.7,105.7) —
   gris plano, señal de visibilidad rota — vs (74,77,72)±3 esperado). Iteró thresholds
   por modo en vez de derivar la geometría correcta.
4. Se acercó mucho: en sus últimas corridas `arealight 32 1L` daba media (72,75,72) —
   dentro de ±2 — y `solidangle` pasaba completo. Pero nunca logró **ambos modos verdes
   en el mismo binario**.
5. Fricciones de entorno que le costaron turnos: escritura a `/tmp` bloqueada por el
   sandbox (resuelto inline), un `Edit` fallido por string duplicado (2 matches), otro
   por string inexistente (desincronización con el archivo), un output de 60 KB
   persistido a disco, y varios `git commit` denegados por permisos.
6. **Final revelador del modo C0**: su último mensaje fue *"¿Debería continuar
   debugeando arealight o prefieres que enfoque en otras mejoras?"* — pidió permiso a
   un stdin cerrado en vez de seguir iterando. El run terminó `done` con el trabajo
   incompleto.

Gate final de fase 3: `solidangle` **verde** (Δ=0.39, ruido 1.23 vs ref 1.38),
`arealight` **rojo** — media |Δ| = 5.33 con máximo permitido 2.00.

## Lecturas para la tesis (C0 con haiku)

1. **Perfil de fallo**: no fue falta de conocimiento (la estructura, los pdf y los
   muestreadores estaban bien) sino **incapacidad de cerrar una calibración numérica
   acoplada** (visibilidad compartida entre dos estimadores) dentro de un run one-shot.
2. **Sin memoria no hay segunda oportunidad**: el diagnóstico correcto (paso 2 de la
   cronología) se enunció y luego se diluyó entre 35 ediciones — nada lo persistió.
   Es exactamente la hipótesis que c2-memory pone a prueba.
3. **El modo interactivo se filtra**: haiku terminó la fase 3 pidiendo instrucciones,
   comportamiento inútil en headless. Un `verify-cmd`/loop con gate automático (C2
   orquestado) no le habría aceptado ese cierre.
4. Costo total de la celda: ≈ $2.46 y 27 min — la fase 3 sola costó más que las otras
   tres juntas (×2.2 en dólares, ×1.6 en tiempo).

## Evidencia

- Renders PNG + código por fase: `data/runs/fase{0..3}-c0-bare@haiku-*.{png,rt.cpp}`
- Telemetría cruda: `data/runs/fase*-c0-bare@haiku-*.runshow.txt`
- Tags git: `celda/c0-bare-haiku/f0..f3` (un commit por fase, rama `curso`)
- Resumen del propio agente: `data/curso/resumen-c0-bare-haiku.md`
