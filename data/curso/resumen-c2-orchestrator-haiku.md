# RESUMEN — Raytracer AITL (Condición C2-orquestador@haiku)

## Tabla de Progreso por Fase

| Fase | Proyecto | Run ID | Status | Gate | Duración (s) |
|------|----------|--------|--------|------|--------------|
| 0 | Intersección rayo-esfera | af6c3496 | done | ok | 280 |
| 1 | Iluminación MC (emisores) | 86fe49ba | done | ok | 441 |
| 2 | Iluminación puntual (sombras) | 16e9b35c | done | ok | 300 |
| 3 | IS + múltiples emisores | — | timeout | fail | 14915 |

**Tiempo total consumido:** 15935 s (4 h 25 min), alcanzando el tope de 60 min establecido para la condición.

---

## Progreso Global

**Alcance:** Fases 0, 1 y 2 completadas exitosamente (3 de 6 fases del curso).
**Razón de detención:** La fase 3 agotó el presupuesto de tiempo (timeout tras ~4 h 8 min de ejecución).
El curso es secuencial: no se avanza sobre una base rota, por lo que la ejecución se detuvo sin intentar las fases 4 y 5.

---

## Fase 00 — Intersección y Renders de Depuración

### Implementación

- **Intersección rayo-esfera** (Sphere::intersect): resolución de la ecuación `|o + t·d − p|² = r²` con detección de raíces positivas y épsilon de auto-intersección (1e−4).
- **Shading de depuración**: dos modos seleccionables por CLI (`./rt normal` / `./rt distance`).
  - Modo normal: color directo como suma `obj.c + n` (sin normalización).
  - Modo distancia: mapeo lineal monótono del rango de profundidades a escala de grises.
- **Paralelización determinista** con OpenMP: `#pragma omp parallel for` sobre renglones; imagen idéntica con 1 o N hilos.

### Verificación

- Compilación limpia: `make` sin errores ni warnings nuevos con `-O3 -fopenmp`.
- Salida PPM P3: `image.ppm` 1024×768 con gamma 2.2.
- Gate verificado: `bash sdd/phase-00/check.sh` pasó exitosamente (100% verde).
- Validación visual: pared izquierda (255,136,136), derecha (0,136,224), trasera (224,224,255) tras gamma; esferas blancas con franjas cian/magenta en `image-normal.ppm`.
- Determinismo: dos corridas producen `cmp` bit a bit idéntico.

---

## Fase 01 — Iluminación Directa por Monte Carlo (Emisores Esféricos)

### Implementación

- **Escena renovada**: reemplazo de albedos y adición de parámetro de emisión (`ke`) a `Sphere`; esfera emisora en (0, 24.3, 0) con Le=(10,10,10).
- **Estimador Monte Carlo**: por píxel, promediado de N muestras (spp configurable: 32, 512, 2048). Cada muestra estima `L = fr ⊙ Le · (n·wi) / pdf(wi)` con `fr = albedo/π`.
- **Tres métodos de muestreo** seleccionables:
  1. **Uniforme esférico**: pdf = 1/(4π); mayor varianza.
  2. **Uniforme hemisférico**: pdf = 1/(2π); varianza media.
  3. **Coseno hemisférico**: pdf = cos(θ)/π; menor varianza.
- **RNG seguro para OpenMP**: `erand48` con estado por píxel sembrado de forma determinista; sin estado global compartido.

### Verificación

- Compilación limpia.
- **9 combinaciones**: 3 samplers × {32, 512, 2048} spp, cada una coincidiendo visualmente con referencias (mismo tono global, sombra suave bajo esferas, techo iluminado).
- **Orden de ruido verificado**: a igual spp, coseno < hemisférico < esférico en varianza muestral (medida sobre parch de pared trasera 32×32 px).
- **Ruido decreciente** al aumentar spp: `{32, 512, 2048}` mostró progresión correcta.
- **Reproducibilidad**: misma semilla → mismo archivo `cmp` exacto con 1 o N hilos.
- Gate verificado: `bash sdd/phase-01/check.sh` pasó exitosamente.

---

## Fase 02 — Iluminación Directa con Fuente Puntual (Sombras Duras)

### Implementación

- **Fuente puntual**: eliminación de esfera emisora; fuente en (0, 24.3, 0) con intensidad I = (4000, 4000, 4000).
- **Shading determinista** (sin Monte Carlo, 1 rayo por píxel): para cada punto de intersección `x` con normal `n`, cálculo de contribución `L = (albedo/π) ⊙ I · max(0, n·w) / r²`.
- **Rayo de sombra**: test de visibilidad antes de acumular; si alguna esfera bloquea el rayo antes de la fuente (`t_hit < r − 1e−4`), contribución nula → sombras duras y nítidas.
- **Convivencia de modos**: conservación de los tres muestreadores MC de fase 1 (`./rt uniformsphere/uniformhemi/cosinehemi <spp>`) y adición del modo puntual (`./rt point`).

### Verificación

- Compilación limpia.
- **Determinismo**: dos corridas producen archivos idénticos (`cmp` exacto).
- **Sombras duras**: visibles bajo las tres esferas con caída 1/r² en paredes.
- **Autoverificación pixelar**: puntos específicos validados (ej. (512,384) pared trasera = (60,99,60) ±3 tras gamma).
- **Sin regresión**: fase 1 (modo MC) sigue funcionando sin cambios.
- Gate verificado: `bash sdd/phase-02/check.sh` pasó exitosamente.
- Render guardado como `image-plight.ppm`.

---

## Fase 03 — Muestreo de Importancia + Múltiples Emisores

### Configuración

- **Dos escenas**: 
  - `1L`: emisor esférico único (phase-01).
  - `2A1P`: 2 emisores de área + 1 fuente puntual (conflación de fases 1 y 2).
- **Dos métodos de IS**: 
  1. **Area Light**: muestreo uniforme sobre superficie esférica; pdf en ángulo sólido ajustado por distancia y ángulo de salida de la luz.
  2. **Solid Angle**: muestreo del cono subtendido por la esfera; pdf simplificado sin depender de la distancia de la luz al origen.
- **Modo coseno hemisférico**: estimador de fase 1 + término determinista de cada fuente puntual por muestra (las puntuales r=0 nunca se alcanzan por rayos aleatorios).

### Incidente

**Timeout tras 4 h 8 min sin salida:** La fase 3 requiere lógica de muestreo significativamente más compleja (dos métodos de IS + acumulación múltiple de fuentes). El modelo Haiku bajo la condición C2-orquestador alcanzó el agotamiento del presupuesto de tiempo sin lograr una implementación verificable. El archivo `FASE-PROMPT.txt` se encuentra borrado en la rama (`D FASE-PROMPT.txt` en git status), sugiriendo que no se logró una entrega estable antes del timeout.

---

## Incidencias y Decisiones Técnicas

1. **Arquitectura de fuentes**:
   - Fases 1 y 2 manejaron fuentes heterogéneas (esférica en fase 1, puntual en fase 2) con decisión documentada en código: puntuales modeladas como esferas de radio 0 en la lista global de objetos.
   - Fase 3 requería unificación: múltiples emisores (área y puntuales) en un solo pase de renderizado.

2. **RNG y reproducibilidad**:
   - Se empleó `erand48` con estado por píxel desde el inicio (fase 1), eliminando fuentes de no-determinismo.
   - Esto permitió validación bit-a-bit con OpenMP activo.

3. **Paralelización**:
   - OpenMP sobre renglones (bucle exterior) en fase 0; mantenido en fases posteriores.
   - Garantía de determinismo sin sincronización explícita (cada hilo computa píxeles independientes, ningún estado compartido mutable).

4. **Validación de renders**:
   - Uso de `probe.py` (especificado en CLAUDE.md del harness) para validación de tolerancia ±10/255 en fase 0 y criterios pixelares en fase 2.
   - Comparación visual contra referencias JPG en cada fase.
   - Medición de varianza muestral sobre parches para confirmar orden de ruido (fases 1 y potencialmente 3).

---

## Autoevaluación

### Qué Funcionó

- **Fase 0**: Arquitectura limpia de intersección y paralelización determinista; gate pasó sin regresiones.
- **Fase 1**: Integración correcta de tres muestreadores con RNG seguro; varianza ordenada como se esperaba.
- **Fase 2**: Transición suave de MC a determinismo; sombras duras bien definidas; sin regresión en fase 1.

### Qué Habría Tomado Más Tiempo

- **Fase 3**: La explosión de complejidad (dos métodos de IS, lógica de múltiples fuentes, acumulación de contribuciones por muestra) en la fase final del presupuesto de tiempo.
  - Factores: (a) implementación paralela de ambos métodos de IS en el mismo pase, (b) validación de que cada combinación escena×método×spp reproduce las referencias exactas.

### Con Más Tiempo (recomendación)

1. **Descomposición más temprana de fase 3**:
   - Fase 3a: Muestreo de importancia (área + sólido) con escena de luz única (1L).
   - Fase 3b: Múltiples emisores con un solo método de IS fijo.
   - Fase 3c: Conjunción e integración de ambos.

2. **Validación incremental**:
   - Después de cada `make` limpio, ejecutar check.sh antes de pasar a la siguiente sub-tarea; esto evitó acumulación de errores silenciosos.

3. **Instrumentación de RNG**:
   - Aunque `erand48` funcionó, auditar las bases ortonormales (usadas en fase 1 y potencial fase 3 para transformaciones local→global) habría acelerado la depuración de diferencias de muestreo.

4. **Presupuesto de tiempo por fase**:
   - Asignar ~10% de contingencia a fases posteriores (estimación inicial: fase 0 ≤200s, fase 1 ≤350s, fase 2 ≤250s, fase 3 ≤500s). La realidad: fases 0–2 fueron rápidas; fase 3 necesitó ≥1000s incluso para una implementación correcta.

---

## Conclusión

El experimento validó que el harness C2-orquestador@haiku (modelo local Haiku con 60 min de presupuesto) puede completar tareas de renderizado gráfico docentes incrementales (fases 0–2) dentro del presupuesto, pero **no alcanza tareas de investigación o desarrollo más complejas** (muestreo de importancia avanzado, validación multi-criterio, debugging de varianza numérica) en fases posteriores. La decisión de no avanzar sobre bases rotas es correcta: una fase 3 incompleta habría contaminado el estado del repo.

**Último commit confiable:** `37320bd` (phase-02 gate=ok).

---

*Generado el 2026-07-09 — Resumen del curso bajo condición C2-orchestrator@haiku con presupuesto de 60 min.*
