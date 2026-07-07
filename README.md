# metricas/raytracer — repo oficial de métricas del experimento raytracer

Experimento **C0 vs C2** de la tesis *Loop + Harness Engineering*: el raytracer del
curso IA7200-L (**C++/OpenMP** — ojo: algunos programas son terminales interactivos) se
desarrolla en 6 fases (F0–F5), cada fase bajo tres condiciones, y las métricas
([`METRICAS.md`](METRICAS.md), M1–M61) salen de la telemetría durable del harness AITL
(MCP `aitl-js`, proyecto `aitl-raytracer`).

**Este es el ÚNICO repo con ramas**: `main` = specs + protocolo + scripts (nunca
implementación); cada celda del experimento vive en su rama `fase-N/<condición>`.

## Cómo medir (TL;DR)

```bash
# 1. Corre una celda completa (rama + plantilla + prompt de la spec + corrida + métricas):
scripts/run-cell.sh 0 c0-bare            # C0    con `aitl run --bare`
scripts/run-cell.sh 0 c2-memory          # C2m   con `aitl run` (harness = conocimiento)
scripts/run-cell.sh 0 c2-orchestrator    # C2o   con `aitl run --model lmstudio --mcp`
scripts/run-cell.sh 0 c2-memory host     # variante: Claude Code como agente (run-host)

# 2. ¿Corrida hecha a mano? Recolecta igual:
scripts/collect-metrics.sh <run_id> <fase> <condición>

# 3. El resumen se regenera solo; también a demanda:
scripts/resumen.sh                       # → RESUMEN-METRICAS.md
```

Cada corrida deja: fila en `data/metricas.csv`, crudo en `data/runs/*.runshow.txt`,
log en `data/logs/` (gitignored) y el markdown [`RESUMEN-METRICAS.md`](RESUMEN-METRICAS.md)
regenerado con la matriz por celda y los deltas C0−C2 (M49–M51).

Al terminar cada celda: revisa la fila del CSV (tokens > 0, status), rellena a mano
las columnas de juicio (`verify_1er_intento`, `imagen_ok`, `adr`, `notas`) y commitea
`data/` + `start/` + `RESUMEN-METRICAS.md` **en la rama de la celda** (`fase-N/<cond>`).
`main` nunca lleva implementación. Corridas inválidas: `aitl intervene` + nota en el
CSV — jamás se borran.

## Gate objetivo por fase (`check.sh`)

Si existe `start/sdd/phase-0N/check.sh`, `run-cell.sh` convierte el verify de la
celda en `make && bash sdd/phase-0N/check.sh` — **idéntico en las tres condiciones**
(misma vara para C0 y C2). Sin `check.sh` el gate es solo `make`, y un `status: done`
NO acredita la tarea (F0 lo demostró: el `rt.cpp` base compila solo y el loop declaró
éxito sin tocar el código).

**Phase 00** (`check.sh` + `probe.py`, ~1 s en total) implementa los 5 criterios de
aceptación de la spec:

1. Compila — el `make` del verify.
2. `./rt normal` — 14 sondas de píxel (5 paredes + 3 puntos por esfera) contra el
   color **analítico exacto**: escena y cámara están fijas, así que `probe.py`
   calcula el valor esperado con la misma aritmética que `rt.cpp` (tolerancia
   ±10/255 por canal tras gamma, la de la spec).
3. `./rt distance` — gris real (r==g==b), gradiente no saturado (≥16 niveles),
   monótono respecto a t, y siluetas de las 3 esferas distinguibles de la pared
   trasera (Δ≥20).
4. Determinismo — dos corridas consecutivas y `OMP_NUM_THREADS=1` vs default,
   byte a byte (`cmp`).
5. `delete[]` conservado y `#pragma omp parallel` presente (RF-4).

Toda ejecución de `./rt` va con stdin cerrado (`</dev/null`): los programas
interactivos nunca esperan TTY (regla de validez 6). Para correrlo a mano, desde
`start/`:

```bash
make && bash sdd/phase-00/check.sh
```

Herramienta del investigador: `python3 sdd/phase-00/probe.py --map` imprime la tabla
analítica (qué objeto golpea cada sonda, su t y el RGB esperado). El gate se validó
contra tres implementaciones: base sin tocar → falla; correcta → pasa; bug sutil
(normal sin normalizar) → falla con mensaje puntual.

## Las tres condiciones (mismo prompt, mismo commit de partida)

| Condición | Tratamiento | CLAUDE.md instalado | MCP | hydrate | Ejecución |
|---|---|---|---|---|---|
| **c0-bare** | ninguno (baseline) | no menciona el harness | ✗ | ✗ | `aitl run --bare` |
| **c2-memory** | harness como conocimiento (memoria+ADRs+skills) | instruye leer el MCP | ✓ | ✓ | `aitl run` |
| **c2-orchestrator** | harness Max-Capacity: loop orquesta y delega en sub-agentes | rol orquestador/sub-agente | ✓ | ✓ | `aitl run --model lmstudio --mcp` — modelo local de LM Studio; el distill Nemotron quedó **descartado** ([evaluación](start/conditions/c2-orchestrator/MODELO.md)), F0 corre con `qwen/qwen3-4b-2507` (mismo modelo que las otras condiciones) |

Las tres conservan el hook `capture-session` (instrumento de medición, no tratamiento).
Detalle: [`start/conditions/README.md`](start/conditions/README.md).

## Mapa del directorio

```
metricas/raytracer/                ← EL repo (git init aquí; rama por celda)
├── README.md                      esta guía de medición
├── METRICAS.md                    catálogo canónico M1–M61 (ADR-0003)
├── RESUMEN-METRICAS.md            ← GENERADO por scripts/resumen.sh — no editar
├── scripts/
│   ├── run-cell.sh                corre una celda de punta a punta
│   ├── collect-metrics.sh         run-show → data/ + fila CSV
│   └── resumen.sh                 data/metricas.csv → RESUMEN-METRICAS.md
├── data/                          evidencia versionada (CSV + crudos de run-show)
├── start/                         PUNTO DE PARTIDA del agente — con o sin MCP
│   ├── sdd/                       specs por fase + base rt.cpp + referencias + TRAZABILIDAD.md
│   └── conditions/                plantillas c0-bare / c2-memory / c2-orchestrator
├── objective/                     renders de referencia del investigador — VEDADO a agentes
├── aitl-raytracer/                protocolo (MANUAL.md), ADRs espejo, substrate del laboratorio
└── state-of-art/                  temario del curso (PDFs)
```

## Los tres roles

| Dir | Rol | ¿El agente lo lee? |
|---|---|---|
| `start/` | de dónde parte: spec + código base + CLAUDE.md de su condición | Sí — es su mundo |
| `objective/` | a dónde debe llegar: renders de referencia (la solución) | **NO** — compara `check.sh` (hash/RMS) |
| `data/` + ramas | dónde queda la evidencia: un `run_id` por celda | No — es del investigador |

## Reglas de validez (resumen; completas en METRICAS.md §10 y MANUAL.md §5)

1. Ninguna celda corre con spec en `draft` (`run-cell.sh` lo verifica).
2. Prompt idéntico entre condiciones — el script lo extrae textual de la spec.
3. `tokens_input` crudo jamás se compara entre condiciones (M26–M29).
4. Corridas inválidas se anotan (`aitl intervene`), nunca se borran del CSV.
5. En C2 fase ≥1, verificar en el evento `hydrate` que entraron los ADRs previos.
6. Programas interactivos: sus inputs se scriptean en el `check.sh` de la fase
   (stdin por pipe/heredoc) — el gate corre sin TTY.
