# RESUMEN — Experimento `c0-bare@haiku` (Raytracer curso IA7200-L)

## Tabla de fases

| Fase | Proyecto | Run ID | Gate | Duración |
|------|----------|--------|------|----------|
| 0 | Intersección y renders depuración | `030e1f87-4a0a-4afc-aac4-86c7d059ba5e` | ✓ ok | 150 s |
| 1 | Iluminación directa Monte Carlo | `767978b4-b69a-4bf3-8528-64fd46e1cd70` | ✓ ok | 282 s |
| 2 | Fuentes puntuales (sombras duras) | `5104e70e-ca82-40f3-9521-dd9ccea74025` | ✓ ok | 166 s |
| 3 | Muestreo importancia + múltiples emisores | `793072b1-f00c-48c0-8753-7c41026fb2f5` | ✗ fail | 962 s |

**Total**: 1,616 s (26.9 min); **Tope de sesión**: 3,600 s.

## Punto de parada

El curso se detuvo en **fase 3** (gate en rojo). Las fases 0–2 completaron exitosamente; la fase 3 no pasó los criterios de validación del `check.sh` tras 962 s de procesamiento.

### Motivo del fallo

La fase 3 requiere:
- Muestreo de importancia de fuentes esféricas (dos métodos: área y ángulo sólido).
- Soporte de múltiples emisores (2 esferas emisoras + 1 puntual).
- 5 renders específicos validados contra referencias (5 JPG).
- Validación de ruido (std del parche de pared trasera dentro de ±50% de la referencia).

El check.sh ejecuta estos renders con timeout de 600–1800 s y compara contra referencias usando `compare-ref.py` (tolerancias en media global, píxeles y desviación estándar). Aunque el compilado (`make`) completó sin errores, al menos uno de los 5 renders o sus validaciones no satisfizo los criterios de aceptación.

Como el curso es secuencial (no se avanza sobre base rota), el experimento terminó sin alcanzar la fase 4.

---

## Fases completadas

### Fase 0 — Intersección y renders de depuración (150 s)

**Implementado:**
- Resolución de `Sphere::intersect` (ecuación cuadrática 
  `|o + t·d − p|² = r²` con descarte de auto-intersección en `t ≤ 1e−4`).
- Función global `intersect(r, t, id)` que encuentra la intersección más cercana con la escena.
- Shading depuración con dos modos CLI:
  - Modo normales: `color = obj.c + n` (sin normalizar; el clamp corta a [0,1]).
  - Modo distancia: escala lineal de `t` entre los extremos visibles (gris monótono).
- Paralelización con OpenMP (pragma sobre renglones) manteniendo determinismo bit-a-bit.
- Salida PPM P3 ASCII con corrección gamma (1/2.2).

**Verificación:**
- `make` compiló sin errores ni warnings nuevos.
- `./rt normal` y `./rt distance` produjeron `image.ppm` 1024×768.
- Ejecuciones múltiples produjeron archivos idénticos (`cmp` exacto); `OMP_NUM_THREADS=1` vs default también.
- `sdd/phase-00/check.sh` verde: colores de normales en sondas (pared izq 255,136,136; derecha 0,136,224; etc.) y gris monótono en modo distancia coincidieron con referencias.

---

### Fase 1 — Iluminación directa por Monte Carlo (282 s)

**Implementado:**
- Esfera emisora en (0, 24.3, 0) con emisión `Le = (10,10,10)`.
- Estructura `Sphere` extendida con campo de emisión.
- Escena nueva (8 esferas: 5 paredes + 3 visibles; 8ª emisora).
- Tres muestreadores de direcciones (RNG local por píxel con `erand48`):
  1. **Uniforme esférico**: `pdf = 1/(4π)`.
  2. **Uniforme hemisférico** (alineado a normal): `pdf = 1/(2π)`.
  3. **Coseno hemisférico**: `pdf = cos(θ)/π` (menor varianza).
- Estimador Monte Carlo en `shade`: promediar N muestras por píxel; si toca emisor devolver su `Le`; si no, acumular `fr ⊙ Le · (n·wi) / pdf(wi)` con `fr = albedo/π`.
- CLI: `./rt <sampler> <spp>` con spp ∈ {32, 512, 2048}.
- RNG determinista (semilla por índice de píxel) — misma imagen con 1 o N hilos.

**Verificación:**
- `make` limpio.
- 9 renders (`image-<sampler><spp>.ppm`) generados y comparados contra referencias JPG.
- Ruido disminuyó al subir spp; a igual spp: coseno < hemisférico < esférico (varianza medida en parche de pared trasera).
- Reproducibilidad: dos corridas → archivos idénticos.
- `sdd/phase-01/check.sh` verde.

---

### Fase 2 — Fuentes puntuales (sombras duras) (166 s)

**Implementado:**
- Fuente puntual determinista en (0, 24.3, 0) con intensidad `I = (4000, 4000, 4000)`.
- Rayo de sombra: antes de acumular, lanzar rayo de `x` hacia la fuente; si otra esfera lo bloquea antes (t_hit < r − 1e−4), contribución = 0 (sombras duras).
- Cálculo directo (sin Monte Carlo): `Lo = fr · I · cos(θ) / r²` con `fr = albedo/π`, `r` = distancia, `θ` = ángulo normal-a-fuente.
- CLI: `./rt point` activa modo puntual; modos MC de fase 1 se conservan sin regresión.
- Render determinista (un rayo por píxel, sin jitter).

**Verificación:**
- Compilación limpia.
- `image-plight.ppm` generado; sombras duras nítidas bajo las tres esferas, caída 1/r² visible en paredes.
- Determinismo: dos corridas → `cmp` idéntico.
- Puntos de prueba (ej. (512,384)=(60,99,60), (512,60)=(121,121,74)) dentro de tolerancia ±3 por canal.
- Sin acné de sombra; techo no se ilumina a sí mismo.
- `sdd/phase-02/check.sh` verde.

---

### Fase 3 — Muestreo de importancia + múltiples emisores (962 s, FALLIDA)

**Intentado:**
- Dos métodos de muestreo de importancia para fuentes esféricas:
  1. **Muestreo de área**: `pdf_ω = pdf_área · d² / |cos(θ_luz)| = d² / (4πr²·cosθ′)`.
  2. **Muestreo de ángulo sólido**: cono hacia fuente, `pdf_ω = 1/(2π(1 − cosθ_max))`.
- Múltiples emisores: iterar todas las fuentes por muestra, acumular contribuciones:
  - Esféricas: muestreo de importancia + rayo de sombra.
  - Puntual (r=0): término determinista de fase 2 (nunca alcanzada por rayo aleatorio).
- Escena 1L (emisor único, 10.5 en origen) y 2A1P (2 emisoras + 1 puntual con nuevas posiciones e intensidades).
- CLI: `./rt <modo> <spp> <escena>` con modo ∈ {arealight, solidangle, coshemi} y escena ∈ {1L, 2A1P}.
- 5 renders esperados: `image-ISLightArea-32`, `image-ISLightSolidAngle-32`, `image-2A1P-ISLightArea-32`, `image-2A1P-ISSolidAngle-32`, `image-2A1P-cosinehemi-2048`.

**Fallo:**
- El check.sh ejecutó 4 renders en secuencia (solidangle 1L, arealight 1L, arealight 2A1P, solidangle 2A1P), cada uno con validación de media global, píxeles y ruido (std_band ±50%).
- Después de ≈962 s, al menos uno de estos checks no satisfizo los criterios: posible desviación en media global RGB ≥ 3, píxeles ≥ tolerancia, o ruido fuera de banda.
- El último render coshemi-2048 (escena 2A1P) no llegó a ejecutarse (timeout o fallo previo).

**Verificación (parcial):**
- `make` compiló sin errores.
- Reproducibilidad (criterio 5) se validó para solidangle 1L (dos corridas idénticas, OMP_NUM_THREADS=1 también).
- Criterio 1 (escena 1L): compare-ref.py falló en media global, píxeles o ruido para solidangle y/o arealight.

---

## Incidencias y decisiones técnicas

1. **RNG determinista**: Todas las fases usaron `erand48` con estado local por píxel, sembrado por índice (x + y*1024), asegurando reproducibilidad incluso con OpenMP.

2. **Paralelismo seguro (fase 0)**: El bucle de píxeles se paralelizó sobre renglones (no píxeles) para minimizar contención y garantizar determinismo bit-a-bit sin sincronización.

3. **Acné de sombra (fase 2)**: Uso consistente de épsilon `1e−4` (desde fase 0) en rayos de sombra para evitar auto-intersección.

4. **Muestreo de importancia (fase 3)**: La fase 3 requería dos métodos ortogonales (área vs. ángulo sólido) con validación de ruido diferencial. La desviación estándar esperada para solidangle es significativamente menor (~1.4–2.5 vs. área ~5–10), lo cual detecta confusiones de método.

5. **Escenas dinámicas**: Fase 1→2→3 cambiaron la escena progresivamente (escena → escena + puntual → múltiples emisores), requiriendo refactorización del loop de shading para soportar múltiples fuentes.

---

## Autoevaluación: qué se haría distinto con más tiempo

1. **Debugging iterativo de fase 3**: 
   - Ejecutar cada render individualmente y validar contra referencias antes de encadenarlos en check.sh.
   - Instrumentar `probe.py` o `compare-ref.py` con verbosidad para aislar qué criterio falló (media vs. píxeles vs. ruido).
   - Verificar que la escena 2A1P se inicialice correctamente (posiciones, emisiones, albedos).

2. **Validación de lógica de muestreo**:
   - Imprimir/renderizar histogramas de direcciones muestreadas para verificar que cada método produce la distribución esperada.
   - Comprobar que `cosθ_max = √(1 − (r/d)²)` en solidangle se comporta bien cerca de la superficie de la esfera.

3. **Pruebas unitarias simples**:
   - Función de test que verifique que `pdf_ω` integra a 1 (o al ángulo sólido esperado) sobre el dominio.
   - Sanity check: la contribución de un emisor visible debe ser positiva; de uno ocluido, cero.

4. **Perfilado de tiempo**:
   - Phase 3 toma 962 s (vs. 150+282+166=598 s en las tres primeras). Investigar si es:
     - Tiempo de render legítimo (2048 spp en coshemi es costoso).
     - Bucle infinito o convergencia lenta en muestreo de ángulo sólido.
     - Validación exuberante en check.sh.

5. **Modularidad del código**:
   - Extraer muestreo de importancia de fuentes a una función `sample_importance_light(...)` reutilizable, separada de la lógica de shading, para facilitar debugging.
   - Mantener modo "depuración" que rendice un solo píxel central con valores numéricos de `pdf`, `cosθ`, `Le`, etc.

---

## Conclusión

El experimento cubrió exitosamente **3 de 4 fases** (75 % del hito secuencial) en **26.9 minutos** de las 60 disponibles. Las fases 0–2 —geometría, iluminación directa MC, y fuentes puntuales— demostraron correctamente en su totalidad. La fase 3 (importancia + múltiples emisores) compiló pero no pasó la validación de referencia; una revisión de los parámetros de muestreo y las escenas habría sido necesaria para diagnosticar qué componente devió la integral.
