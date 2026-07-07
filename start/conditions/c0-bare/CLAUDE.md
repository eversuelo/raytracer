# CLAUDE.md — aitl-raytracer

## Qué es este proyecto

Raytracer en **C++ / OpenMP** del curso IA7200-L, construido en fases incrementales.
El código base es `rt.cpp` (se compila con `make` → `g++ -O3 -fopenmp`).

## Reglas de trabajo

- **Lee la spec de la fase antes de escribir código** (`specs/fase-N/spec.md`). Si la
  spec no existe o está vacía, DETENTE y pídela.
- Implementa exactamente lo que la spec define — no toques escena, cámara ni parámetros
  que la spec no pida cambiar.
- `make` debe compilar sin errores (y sin warnings nuevos con `-Wall`) antes de dar la
  tarea por terminada; si la fase trae `check.sh`, `make && ./check.sh` debe quedar verde.
- Las imágenes renderizadas van a `out/` (gitignored).
