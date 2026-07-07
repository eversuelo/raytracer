# aitl-raytracer

Laboratorio experimental del harness **AITL**: el raytracer del curso IA7200-L
(C++/OpenMP) desarrollado por Spec Driven Development en 6 fases (intersección →
path tracing explícito), cada fase bajo dos condiciones — **C0** (modelo solo) y
**C2** (harness completo) — para verificar en runtime las afirmaciones de la tesis
*Loop + Harness Engineering*.

- **Protocolo y verificación**: [`MANUAL.md`](MANUAL.md)
- **Reglas para agentes e identidad MCP**: [`CLAUDE.md`](CLAUDE.md)
- **Specs canónicas + enunciados + referencias**: `../start/sdd/phase-0N/` (aquí `specs/fase-N/` son punteros)
- **Trazabilidad de corridas**: `../start/sdd/TRAZABILIDAD.md`
- **Ramas**: viven en el repo raíz `metricas/raytracer` — `main` (specs+scripts) · `fase-N/{c0-bare,c2-memory,c2-orchestrator}` (N = 0…5)

| Fase | Proyecto del curso | Alcance |
|---|---|---|
| 0 | Proyecto 1 | Intersección rayo-esfera, renders de depuración, OpenMP |
| 1 | Proyecto 2 · p1 | Iluminación directa Monte Carlo: emisor esférico, 3 muestreos |
| 2 | Proyecto 2 · p2 | Fuentes puntuales, sombras duras |
| 3 | Proyecto 3 | Muestreo de importancia de fuentes + múltiples emisores |
| 4 | Proyecto 4 | Microfacets: conductores ásperos (aluminio/oro) |
| 5 | Final | 5a: MIS β=2 · 5b: Path Tracing explícito |

```bash
aitl check-db && aitl models   # harness listo
# en cada rama de fase: make && ./rt ...   (código base: ../start/sdd/base/rt.cpp)
```
