# Comparativo de campaña — c0-bare vs c2-memory vs c2-orchestrator (haiku, 2026-07-09)

> Resumen ejecutivo de la campaña limpia ("desde 0") con `claude-haiku-4-5`. Detalle
> forense en las tres bitácoras (`bitacora-c0-bare-haiku.md`, `bitacora-c2-memory-haiku.md`,
> `bitacora-c2-orchestrator-haiku.md`). Una corrida por celda (n=1): todo lo que sigue
> es evidencia preliminar, no significancia.

## Marcador global

| | **c0-bare** (sin harness) | **c2-memory** (harness como conocimiento) | **c2-orchestrator** (qwen 7B local orquesta, haiku implementa) |
|---|---|---|---|
| Fases con gate verde | **3/6** (0,1,2) | **5/6** (0,1,2,3,4) | **3/6** (0,1,2) |
| Fase de muerte | 3 (importance sampling) | 5 (path tracing + MIS) | 3 — **timeout**, no rendición (3 delegaciones usadas) |
| Causa de muerte | calibración `arealight` sin converger; terminó pidiendo permiso a stdin cerrado | entregable 512 spp ausente: esperó una "notificación" inexistente | mismo muro arealight; llegó a Δ=3.15 vs 2.00; el tope (agravado por 3.5 h de suspensión del equipo) lo cortó |
| Duración total (despierta) | 26 min 56 s | 36 min 49 s | ≈ 60 min (tope) |
| Turnos haiku totales | 180 | 223 | 244 (en sub-agentes) + 10-12 de qwen |
| Costo total (host) | ≈ $2.46 | ≈ $3.07 | ≈ $4.48 (orquestador local: $0) |
| **Costo por fase verde** | **$0.82** | **$0.61** | **$1.49** |

## Por fase (dur s · gate)

| Fase | c0-bare | c2-memory | c2-orchestrator |
|---|---|---|---|
| 0 | 150 ✅ | 85 ✅ | 280 ✅ |
| 1 | 282 ✅ | 219 ✅ | 441 ✅ |
| 2 | 166 ✅ | 185 ✅ | 300 ✅ |
| 3 | 962 ❌ | 517 ✅ | ≈2 520 despiertos ❌ (timeout) |
| 4 | — | 730 ✅ | — |
| 5 | — | 316 ❌ | — |

## El muro de la fase 3 como benchmark natural

Las tres celdas chocaron con el MISMO defecto (normalización/visibilidad del
muestreo `arealight`; firma del factor ~0.79 ≈ π/4). Cinco sesiones haiku distintas
lo atacaron:

- c0: 35 ediciones de thresholds, nunca convergió, murió preguntando (962 s, rojo).
- c2-memory: diagnóstico geométrico correcto a la primera ("la dirección va HACIA x′"),
  verde en 517 s. La única que cruzó.
- c2-orchestrator: 3 sub-agentes (69+62+62 iters, ≈$3.81 solo en esta fase);
  el 2º hizo debugging diferencial y llegó a Δ=3.15 (umbral 2.00); el 3º fue cortado
  por el tope. El loop externo detectó y re-delegó los dos cierres complacientes —
  la mecánica anti-"declarar victoria" funcionó; el feedback de error de qwen (7B)
  fue demasiado genérico para capitalizarla.

## Los tres hechos que importan

1. **La fase 3 fue el experimento natural**: ambas celdas chocaron con el MISMO bug
   (visibilidad/dirección del rayo de sombra en `arealight`; render ~negro o gris).
   c0 iteró thresholds ad-hoc 35 veces, nunca convergió y terminó preguntando
   "¿debería continuar?" a un stdin cerrado. c2 lo diagnosticó como geometría
   ("la dirección va HACIA x′"), lo corrigió una vez, calibró con renders de 4 spp
   y cerró en verde con −46 % de tiempo y −58 % de costo respecto al fallo de c0.

2. **PERO la ventaja de c2 no es atribuible a la memoria** (hallazgo que invalida la
   lectura ingenua): lo hidratado eran prefijos truncados de transcript (~2000 chars
   de narrativa de apertura por sesión — sin los diagnósticos, que ocurren profundo
   en cada sesión; corrección 2026-07-09: no estaban "vacíos" como reportó una
   versión previa, pero sí carecen del contenido útil), `decisions: 0` en las 6
   fases y **cero llamadas MCP** del agente pese a las instrucciones del CLAUDE.md.
   Con n=1, varianza entre corridas sigue siendo la explicación más parsimoniosa
   del 5/6 vs 3/6.

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

## Pendientes para la siguiente campaña

1. Mejorar `capture-session`: hoy guarda el PREFIJO truncado del transcript (~2000
   chars de apertura) en vez de una síntesis — los diagnósticos y soluciones quedan
   fuera. Neutralizó el valor del canal de memoria en toda la campaña haiku.
2. Orquestador: correr sin `--mcp` (solo usa shell; las 52 tools comen 80 % de su
   ventana de 20 k) o subir el contexto de qwen a 32 k; y enriquecer el resumen de
   error de la re-delegación (incluir valor medido, umbral y modo).
3. Timeout de fase debe matar el árbol completo de `run-host` (quedó un sub-agente
   huérfano quemando API tras el corte).
4. Homologar permisos git entre condiciones (el sub del orquestador commiteó solo;
   c0/c2-memory lo tienen bloqueado).
5. n≥3 por celda y sin suspensiones del equipo (la fase 3 del orquestador quedó
   con duración contaminada por ~3.5 h de sleep — anotado en CSV).
