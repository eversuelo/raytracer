# SDD — Spec Driven Development del raytracer (experimento AITL)

Track de especificaciones del laboratorio: el temario del curso **IA7200-L Laboratorio
de Graficación** (`material-curso/themes.pdf`) convertido en fases SDD que se desarrollan
bajo dos condiciones — **C0** (modelo solo) y **C2** (harness AITL completo) — para dejar
trazabilidad verificable de lo que el harness aporta. El registro maestro de corridas
está en [`TRAZABILIDAD.md`](TRAZABILIDAD.md); el protocolo completo en
`../../aitl-raytracer/MANUAL.md`.

## Estructura

```
sdd/
├── base/               código base del curso (rt.cpp + Makefile) — punto de partida de phase-00
├── material-curso/     temario (themes.pdf) + snapshot del Classroom con todas las referencias
├── e-learning/         material didáctico del curso (láminas/notas; en proceso de carga)
├── phase-0N/           una carpeta por fase:
│   ├── spec.md             la spec SDD (frontmatter type: spec; status draft→approved)
│   ├── README.md /         enunciado original del curso (manda sobre la spec en conflicto)
│   │   enunciado-original.md
│   └── image-*.jpg         renders de referencia de esa fase
└── TRAZABILIDAD.md     matriz fase × condición (ramas, run_ids, ADRs, veredictos)
```

## Fases

| Fase | Proyecto del curso | Tema | Estado spec |
|---|---|---|---|
| [phase-00](phase-00/spec.md) | Proyecto 1 | Intersección rayo-esfera, renders de depuración (distancia/normales), OpenMP | approved |
| [phase-01](phase-01/spec.md) | Proyecto 2 · parte 1 | Iluminación directa Monte Carlo, emisor esférico, 3 muestreos (esférico/hemisférico/coseno) | draft |
| [phase-02](phase-02/spec.md) | Proyecto 2 · parte 2 | Fuentes puntuales, sombras duras (I=4000, determinista) | draft |
| [phase-03](phase-03/spec.md) | Proyecto 3 | Muestreo de importancia de fuentes (área/ángulo sólido) + múltiples emisores (2A1P) | draft |
| [phase-04](phase-04/spec.md) | Proyecto 4 | Microfacets: conductores ásperos (aluminio/oro α=0.3), sample/eval/pdf | draft |
| [phase-05](phase-05/spec.md) | Proyecto final | 5a: MIS β=2 · 5b: Path Tracing explícito (10 rebotes) | draft |

## Flujo por fase

1. El investigador revisa/ajusta la `spec.md` y la aprueba (`status: approved`).
2. Corrida **C0**: rama `fase-N/c0-bare` del repo `aitl-raytracer`, `aitl run --bare`.
3. Corrida **C2**: rama `fase-N/c2-harness`, `aitl run` (memoria + ADRs + gates).
4. Métricas con `aitl run-show`, fila en `TRAZABILIDAD.md`, ADR de cierre de fase.

> Los prompts de tarea viven dentro de cada spec y son idénticos entre condiciones.
> Las specs se persisten en el MCP con `aitl sdd --project aitl-raytracer`.
