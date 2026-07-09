# Comparativo de campaña — c0-bare vs c2-memory (haiku, 2026-07-09)

> Resumen ejecutivo de la campaña limpia ("desde 0") con `claude-haiku-4-5`. Detalle
> forense en `bitacora-c0-bare-haiku.md` y `bitacora-c2-memory-haiku.md`. Una corrida
> por celda (n=1): todo lo que sigue es evidencia preliminar, no significancia.

## Marcador global

| | **c0-bare** (sin harness) | **c2-memory** (harness como conocimiento) |
|---|---|---|
| Fases con gate verde | **3/6** (0,1,2) | **5/6** (0,1,2,3,4) |
| Fase de muerte | 3 (importance sampling) | 5 (path tracing + MIS) |
| Causa de muerte | calibración `arealight` sin converger (35 ediciones de thresholds) | entregable 512 spp ausente: esperó una "notificación" inexistente |
| Duración total del curso | 26 min 56 s | 36 min 49 s |
| Turnos totales | 180 | 223 |
| Tokens totales | 13 643 619 | 15 456 085 |
| Costo total (host) | ≈ $2.46 | ≈ $3.07 |
| **Costo por fase verde** | **$0.82** | **$0.61** |

## Por fase (dur s · gate)

| Fase | c0-bare | c2-memory | Δ dur |
|---|---|---|---|
| 0 | 150 ✅ | 85 ✅ | −43 % |
| 1 | 282 ✅ | 219 ✅ | −22 % |
| 2 | 166 ✅ | 185 ✅ | +11 % |
| 3 | 962 ❌ | 517 ✅ | −46 % y verde |
| 4 | — | 730 ✅ | c0 nunca llegó |
| 5 | — | 316 ❌ | ídem |

## Los tres hechos que importan

1. **La fase 3 fue el experimento natural**: ambas celdas chocaron con el MISMO bug
   (visibilidad/dirección del rayo de sombra en `arealight`; render ~negro o gris).
   c0 iteró thresholds ad-hoc 35 veces, nunca convergió y terminó preguntando
   "¿debería continuar?" a un stdin cerrado. c2 lo diagnosticó como geometría
   ("la dirección va HACIA x′"), lo corrigió una vez, calibró con renders de 4 spp
   y cerró en verde con −46 % de tiempo y −58 % de costo respecto al fallo de c0.

2. **PERO la ventaja de c2 no es atribuible a la memoria** (hallazgo que invalida la
   lectura ingenua): la capa de conocimiento estuvo inerte toda la celda —
   hidratación con docs de sesión VACÍOS (bug de `capture-session`: resúmenes de
   0 chars), `decisions: 0` en las 6 fases y **cero llamadas MCP** del agente pese a
   las instrucciones del CLAUDE.md. La diferencia efectiva entre condiciones fue
   ~21 líneas de CLAUDE.md + un preámbulo vacío. Con n=1, varianza entre corridas es
   la explicación más parsimoniosa del 5/6 vs 3/6.

3. **Ambas muertes fueron el mismo modo de fallo, no falta de capacidad**: protocolo
   interactivo filtrado a headless (pedir permiso / esperar notificaciones). En ambos
   casos el trabajo técnico estaba resuelto o casi: c0 tenía `solidangle` verde y
   `arealight` a 3.3 unidades de la tolerancia; c2 tenía la física de fase 5 en verde
   y solo le faltó renombrar los renders de 512 spp. Un cierre de loop externo
   (verify-cmd del harness, condición c2-orchestrator) es exactamente el mecanismo
   que ataca este modo de fallo.

## Incidencias de instrumentación de la campaña (registradas)

- Fix ADR-0068 (`run-host --no-hydrate`): el baseline C0 corría contaminado con el
  store; 2 corridas previas anotadas INVÁLIDAS (`aitl intervene`) y campaña reseteada.
- Bug abierto: `capture-session` sintetiza resúmenes de sesión vacíos → neutralizó la
  condición c2-memory esta campaña. Re-correr c2 tras el fix para el dato real.
- Los session-docs vacíos de c0 se cuelan al hydrate de c2 (hook presente en todas
  las condiciones): sin efecto esta vez (0 chars), pero con el fix de capture-session
  habría contaminación c0→c2 — decidir si el hook debe correr en c0.

## Pendiente

- Celda `c2-orchestrator@haiku` (project propio `aitl-raytracer-orq`, orquestador
  local `qwen2.5-coder-7b-instruct`): mide el cierre de loop externo del punto 3.
