# ADR-0001 — Fundación del laboratorio: raytracer SDD en 5 fases bajo condiciones C0/C2

- **Estado**: accepted · 2026-07-05
- **Proyecto MCP**: `aitl-raytracer`

## Contexto

La tesis (Loop + Harness Engineering) necesita evidencia de runtime de que la arquitectura
del harness ayuda en desarrollo real, complementando el caso de estudio Schoolar
(`metricas/schoolmx`). Se eligió un raytracer por su progresión natural verificable: cada
fase tiene salida objetiva (imagen) comparable por hash/RMS, lo que hace el gate de calidad
(`--verify-cmd`) independiente del juicio humano.

## Decisión

1. **Cinco fases SDD**: F1 raytracer básico (esferas, pinhole, PPM) → F2 iluminación local
   (Lambert/Phong, sombras) → F3 Whitted (reflexión/refracción) → F4 aceleración/calidad
   (BVH, AA, texturas) → F5 path tracing (Monte Carlo, GI). Las specs las escribe el humano
   en `specs/fase-N/spec.md` **antes** de cualquier corrida (`status: draft` bloquea la
   implementación).
2. **Ramas**: `main` = specs+manual+config, nunca implementación; por fase,
   `fase-N/c0-bare` (C0, `aitl run --bare`) y `fase-N/c2-harness` (C2, harness completo);
   prompts idénticos entre condiciones; las ramas de N+1 nacen del resultado aceptado de N.
3. **Identidad MCP**: project `aitl-raytracer` (hash `8d9c37d22aac48e8`), software
   `raytracer`, ledger de ADRs propio desde 0001.
4. **Telemetría**: `run-show` por celda (fase×condición); Cara B vía hooks
   hydrate/capture; mapeo H1–H11 → verificación en `MANUAL.md`.
5. **Substrate** TS mínimo con `npm test` verde desde el commit 0 para que el gate funcione
   desde la primera corrida.

## Consecuencias

- Cada afirmación de la tesis tiene una celda experimental que la respalda o la refuta;
  las métricas salen de `events`/`runs`, no de impresiones.
- La comparación C0 vs C2 es limpia: mismo prompt, mismo punto de partida, misma verificación.
- Costo: disciplina de protocolo (`MANUAL.md`) y specs humanas antes de correr.
- El ledger de `aitl-raytracer` es independiente del de `aitl-js`: sin riesgo de colisión
  con el trabajo paralelo del plan F1–F9.
