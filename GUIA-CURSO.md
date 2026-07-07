# Guía — Evaluación de curso completo (condición × modelo)

Cada **celda** es un agente (Claude Code) que intenta completar el curso del raytracer
—fases 00→05, los 5 proyectos— **de corrido**, con tope global de tiempo. La matriz:

| Celda | Condición | Modelo |
|---|---|---|
| `c0-bare@sonnet` | sin harness | `claude-sonnet-5` |
| `c0-bare@opus` | sin harness | `claude-opus-4-8` |
| `c2-memory@sonnet` | harness como conocimiento (MCP + memoria/ADRs) | `claude-sonnet-5` |
| `c2-memory@opus` | ídem | `claude-opus-4-8` |

La métrica primaria es **hasta dónde llega y en cuánto tiempo**: fases completadas con
gate verde dentro del tope, duración por fase y total. Los prompts por fase están
incrustados en las specs (`start/sdd/phase-0N/spec.md`, sección `## Prompt de tarea`),
validados contra `objective/`; el prompt y el gate son idénticos para las 4 celdas —
solo cambian condición y modelo.

## Prerrequisitos (una vez)

1. `claude` (Claude Code) logueado y `aitl` en PATH — verifica: `aitl models`.
2. Para las celdas c2: `aitl-raytracer/.mcp.json` presente (credenciales del MCP).
3. `python3` con Pillow (los gates de fases 1–5 comparan contra los JPG de referencia
   con `sdd/tools/compare-ref.py`) — verifica: `python3 -c "import PIL"`.
4. Working tree limpio en `main` (`git status`).

## Lanzar las celdas — TÚ decides cuándo

**Una celda a la vez** (comparten el working tree de `start/`). Orden sugerido —
primero las c0 (sin memoria) y después las c2, para que lo que el harness memorice en
las c2 no exista aún al correr las c0:

```bash
scripts/run-course.sh c0-bare sonnet
scripts/run-course.sh c0-bare opus
scripts/run-course.sh c2-memory sonnet
scripts/run-course.sh c2-memory opus
```

- Tope por celda: `COURSE_MIN=60` (minutos). Cámbialo por corrida:
  `COURSE_MIN=90 scripts/run-course.sh c0-bare sonnet`.
- Cada celda tarda hasta el tope + ~10 min del resumen final. Costo estimado por
  celda: en el orden de $5–20 según hasta qué fase llegue.

## Qué hace el script por dentro

1. Crea la rama `curso/<condición>-<modelo>` **desde `main`** (mismo punto de partida
   para las 4) y siembra `start/` con `sdd/base/rt.cpp` + la plantilla de la condición,
   inyectando el modelo en `start/.claude/settings.json`.
2. Corre las fases 00→05 en orden. Por fase: pasa el prompt de la spec a
   `aitl run-host --host claude-code`, corre el **gate objetivo** él mismo
   (`make` + `check.sh` de la fase, stdin cerrado) y commitea el avance en la rama.
3. **Se detiene** si el gate de una fase queda rojo (el curso es secuencial) o si el
   tope global se agota.
4. Al final, el **propio Claude de la celda escribe el resumen** de su corrida.

## Dónde queda todo (por celda)

| Artefacto | Ruta |
|---|---|
| Resumen escrito por Claude | `data/curso/resumen-<condición>-<modelo>.md` |
| Fases y tiempos (CSV) | `data/curso/fases-<condición>-<modelo>.csv` |
| Métricas por fase (M1–M15) | `data/metricas.csv` (columna `cond` = `c0-bare@sonnet`, …) |
| Telemetría cruda por corrida | `data/runs/faseN-<cond@modelo>-<run_id>.runshow.txt` |
| Logs en vivo | `data/logs/curso-<condición>-<modelo>-faseN-*.log` (+ `.gate.log`) |
| Código por fase | rama `curso/<condición>-<modelo>`, un commit por fase |
| Renders | `start/*.ppm` (gitignored; quedan los de la última fase corrida) |

`data/curso/`, `data/metricas.csv` y `data/runs/` quedan **sin commitear** a propósito:
acumulan las 4 celdas en el working tree; consolídalos en `main` cuando termine la
matriz (junto con las columnas manuales `verify_1er_intento`, `imagen_ok`, `adr`).

## Advertencias

- **No corras dos celdas en paralelo** — misma carpeta `start/`, mismas rutas de log.
- **Contaminación c2↔c2**: las dos celdas c2 comparten el store de memoria/ADRs del
  proyecto `aitl-raytracer`. La segunda celda c2 puede hidratar memorias que escribió
  la primera (ventaja no atribuible al modelo). Si quieres celdas c2 puras, resetea el
  store entre ambas según `aitl-raytracer/MANUAL.md`, o interpreta la segunda como
  "modelo + memoria acumulada".
- **Relanzar una celda desde cero**: `git branch -D curso/<condición>-<modelo>`, borra
  su CSV en `data/curso/` y su fila en `data/metricas.csv`, y vuelve a lanzar.
- Si una fase muere por tope (`status=timeout`), la celda termina ahí; el resumen y el
  CSV lo registran — eso ES el dato (no relances "para que termine").

## Después de las 4 celdas

1. Revisa los 4 resúmenes de `data/curso/` y compara los CSV de fases.
2. Completa a mano en `data/metricas.csv`: `verify_1er_intento`, `imagen_ok`, `adr`.
3. Pide en una sesión de Claude el análisis comparativo final (tabla condición × modelo:
   fases completadas, tiempo total, tokens, costo) y commitea `data/` en `main`.
