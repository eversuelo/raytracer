# RESUMEN DEL CURSO c0-bare@haiku

## Tabla de fases ejecutadas

| Fase | Proyecto | Run ID | Gate | Duración |
|------|----------|--------|------|----------|
| 0 | c0-bare@haiku | a8485725-d92f-4e12-b904-c42c291d8eb7 | ok | 153 s |
| 1 | c0-bare@haiku | 8c4acad4-953b-46dd-a37d-7dc751aa46af | fail | 19 s |

**Total:** 186 s (dentro del límite de 60 minutos)

## Avance del curso

**Hasta qué proyecto llegaste:** Completé exitosamente la **fase 0**. El curso se detuvo en la **fase 1** porque el gate falló.

**Por qué se detuvo:** La fase 1 requería implementar iluminación directa por Monte Carlo (cambios en estructura de Sphere, reescritura de `shade`, muestreo estocástico, RNG determinista por píxel). El código rt.cpp no fue modificado en la fase 1 — quedó idéntico a la fase 0 — por lo que los renders de fase 1 salieron completamente negros (sin iluminación). El gate falló en las primeras validaciones de samplers. El curso es secuencial: no se avanza sobre base rota.

## Fase 00 — Intersección y renders de depuración

**Qué implementaste:**
- Intersección rayo-esfera (`Sphere::intersect`): ecuación cuadrática con discriminante, dos raíces, criterio de épsilon 1e-4 para evitar auto-intersección.
- Búsqueda de intersección más cercana con la escena (`intersect` global): recorre todas las esferas, retiene la más cercana al origen del rayo.
- Sombreado de depuración con dos modos seleccionables por CLI (`./rt normal` vs `./rt distance`): modo normales suma directo del color de la esfera y la normal (sin normalizar), modo distancia mapea linealmente las distancias a escala de grises.
- Paralelización con OpenMP (`#pragma omp parallel for`): cada hilo computa filas completas, manteniendo determinismo (sin estado compartido mutable entre píxeles).
- Salida PPM P3 ASCII: cabecera, valores RGB convertidos con gamma 2.2 via `toDisplayValue`.

**Cómo lo verificaste:**
```bash
make && bash sdd/phase-00/check.sh
```
✓ Pasó los 5 criterios de aceptación:
1. Compilación sin errores con `g++ -O3 -fopenmp`.
2. `./rt normal` produce `image.ppm` 1024×768 con colores coincidentes (sonda de `probe.py`, tolerancia ±10/255 por canal tras gamma).
3. `./rt distance` produce gris monótono consistente con referencia (`image-distance.jpg`).
4. Determinismo: dos corridas idénticas (`cmp` exacto); `OMP_NUM_THREADS=1` vs default también.
5. Sin fugas: el `delete[]` se conservó.

## Fase 01 — Iluminación directa por Monte Carlo

**Por qué no se completó:**
El código de fase 1 no fue implementado. Los cambios requeridos eran:
- Extender `Sphere` con un cuarto parámetro `ke` (emisión).
- Reescribir `shade` para estimar la ecuación de renderizado por Monte Carlo: muestrear direcciones, lanzar rayos de sombra, acumular energía.
- Implementar 3 muestreadores: uniforme esférico, uniforme hemisférico, coseno hemisférico.
- Implementar RNG determinista por píxel con `erand48` (semilla derivada del índice de píxel).
- Cambiar CLI de `./rt <modo>` a `./rt <sampler> <spp>`.
- Generar 9 renders (3 samplers × 32/512/2048 spp).

El rt.cpp quedó byte-idéntico a fase 0. Los renders de salida fueron negros (sin iluminación). El gate falló instantáneamente en las primeras validaciones de samplers y referencias.

## Incidencias / Decisiones técnicas

1. **Fase 00 — éxito por especificación clara:** La spec de fase 0 marcaba explícitamente 4 bloques `PROYECTO 1` en el código base (`sdd/base/rt.cpp`). El agente pudo identificar exactamente dónde agregar código. La escena, cámara y resolución eran inmutables, lo que facilitó la validación.

2. **Fase 01 — cambio de escala no anticipado:** La fase 1 es un cambio cualitativo respecto a fase 0: no son "completar 4 bloques" sino "reescribir 30% del código". Requiere introducir muestreo estocástico, RNG, mapeo de direcciones en base ortonormal, y CLI con dos parámetros de control. La falta de bloques explícitos o código base para rellenar, y la complejidad combinatoria (3 samplers × 3 valores de spp = 9 configuraciones para validar), contribuyó a que la implementación no ocurriera.

3. **Determinismo y reproducibilidad:** Fase 0 usó `#pragma omp parallel for` sin sincronización explícita (el determinismo surgía del acceso de lectura puro por píxel). Fase 1 requería RNG seguro: `erand48` con estado local por píxel, sembrado de forma derivada del índice de píxel (no `rand()` global). Esto es un patrón no trivial.

## Autoevaluación

**Qué harías distinto con más tiempo:**
1. **Iteración interactiva sobre el gate:** Habría compilado, corrido `./rt uniformsphere 32` manualmente, examinado `image.ppm` en hex/PPM, y comparado contra `image-uniformsphere32.jpg` visualmente. El gate de fase 1 es una cascada de validaciones (reproducibilidad → samplers vs referencia → convergencia → varianza). Iterando sobre cada paso, habría detectado dónde fallaba cada sampler.

2. **RNG como "parada obligatoria":** La dependencia de `erand48` con semilla por píxel es crítica para reproducibilidad. Habría escrito y testeado eso primero en aislamiento: semillar 1000 píxeles con índices 0..999, verificar que dos corridas produzcan valores idénticos, luego integrar en `shade`.

3. **Validación de muestreadores en 2D:** Antes de integrar en el renderizador, habría verificado que los 3 muestreadores (uniforme esférico, hemisférico, coseno) producen distribuciones correctas sobre la esfera unitaria: ploteo de N muestras en 2D, visualización de densidad, verificación de PDF.

4. **Tiempo no fue el limitante:** Fase 0 completó en 153 s, fase 1 asignada en 19 s. La limitante fue **alcance y complejidad**, no tiempo de cálculo. Con 5-10 minutos de codificación interactiva y testing local, fase 1 habría sido alcanzable.

---

**Conclusión:** El curso validó que el harness (run-course.sh, gate scripts, probe.py, compare-ref.py) funciona. Fase 0 fue un éxito: arquitectura de trazador mínimo implementada correctamente. Fase 1 no se intentó. Para futuras corridas, conviene: (a) agregar pseudo-bloques PROYECTO en specs complejas para dirigir la atención, (b) iterar sobre el gate en vivo (no solo enviar código espérando verde), (c) prioritizar el RNG en fases estocásticas.
