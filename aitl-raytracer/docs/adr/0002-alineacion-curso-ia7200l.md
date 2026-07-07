# ADR-0002 — Alineación de las fases al curso IA7200-L

- **Estado**: accepted · 2026-07-06 · Enmienda a [ADR-0001](0001-fundacion-laboratorio.md)
- **Proyecto MCP**: `aitl-raytracer` · Commit: `da84354`

## Contexto

El material real del experimento apareció en `../sdd/`: código base C++/OpenMP del curso
**IA7200-L Laboratorio de Graficación** (`base/rt.cpp` con huecos "PROYECTO 1"), temario,
enunciados originales de los proyectos escritos por el investigador, y renders de
referencia por fase. La escalera planeada en ADR-0001 (básico→Whitted→BVH→PT, TypeScript)
no correspondía al curso real, y duplicar specs fragmentaría la fuente de verdad.

## Decisión

1. **Escalera = proyectos del curso**: F0 intersección + renders de depuración ·
   F1 iluminación directa Monte Carlo (emisor esférico, 3 muestreos, 32/512/2048 spp) ·
   F2 fuentes puntuales (I=4000) · F3 muestreo de importancia de fuentes + múltiples
   emisores (2A1P) · F4 microfacets conductores ásperos (aluminio/oro α=0.3,
   sample/eval/pdf) · F5 final con ambas opciones: **5a MIS β=2** y **5b Path Tracing
   explícito** (10 rebotes).
2. **Fuente única de specs**: `sdd/phase-0N/spec.md` (draft→approved), junto al enunciado
   original — que manda en conflicto — y las imágenes de referencia; `specs/fase-N/` de
   este repo son punteros.
3. **C++/OpenMP** sobre `sdd/base/rt.cpp`; el gate mínimo de `--verify-cmd` pasa de
   `npm test` a `make`, extensible con checks de imagen (hash/RMS) por spec.
4. Ramas `fase-0/{c0-bare,c2-harness}` añadidas; trazabilidad en `sdd/TRAZABILIDAD.md`
   (filas 5a/5b separadas).
5. Material didáctico del curso → `sdd/e-learning/` (en carga).

## Consecuencias

- Las specs describen exactamente lo que el curso evalúa, con referencias reales por fase.
- El esqueleto TS del repo queda como tooling del experimento, no lenguaje del raytracer.
- Los agentes parten de `sdd/base/rt.cpp` en la rama de cada condición; escena/cámara
  intocables salvo lo que la spec de fase indique.
- `phase-00` approved; 01–05 en draft hasta revisión del investigador.
- Pendiente: scripts `check.sh` de comparación de imagen por fase (al aprobar cada spec).
