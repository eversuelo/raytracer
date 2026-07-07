# start/ — punto de partida de los agentes (con o sin MCP)

Todo agente del experimento **empieza aquí**, sea cual sea su condición. La única
diferencia entre condiciones es la plantilla de `conditions/` que
`../scripts/run-cell.sh` instala: el prompt de tarea, el código base y la spec son
**idénticos**.

```
start/
├── sdd/                fuente de verdad: specs por fase (draft→approved), código base
│   │                   del curso (base/rt.cpp + Makefile), enunciados originales,
│   │                   renders de referencia y TRAZABILIDAD.md (registro maestro)
├── conditions/         plantillas c0-bare / c2-memory / c2-orchestrator (ver su README)
├── rt.cpp, Makefile    ← aparecen POR RAMA: sembrados desde sdd/base por run-cell.sh
├── CLAUDE.md, .claude/ ← aparecen POR RAMA: la plantilla de la condición de la celda
└── out/                renders del agente (gitignored)
```

En `main` este directorio solo tiene `sdd/` y `conditions/`. El working tree del agente
(`rt.cpp`, `CLAUDE.md`, `.claude/`, `.mcp.json`) existe únicamente en las ramas
`fase-N/<condición>` — así cada celda es reproducible y comparable.

```bash
# no lo hagas a mano — una celda completa es:
../scripts/run-cell.sh <fase> <condición> [loop|host]
```

Reglas: el agente **no** lee `../objective/` (ahí está la solución); el gate es
`make` (+ `check.sh` de la fase cuando exista, con stdin scripteado — hay programas
interactivos). Protocolo completo: [`../aitl-raytracer/MANUAL.md`](../aitl-raytracer/MANUAL.md).
