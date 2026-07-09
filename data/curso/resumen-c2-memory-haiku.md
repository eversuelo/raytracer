# RESUMEN DEL CURSO — aitl-raytracer (c2-memory@haiku)

## Tabla de Ejecución por Fase

| Fase | Proyecto | Run ID | Gate | Duración (s) |
|------|----------|--------|------|--------------|
| 0 | Intersección y renders de depuración | `3d88ff86-54fb-44d8-8a85-9cb7ffd58c05` | ✓ ok | 85 |
| 1 | Iluminación directa por Monte Carlo | `7f225f63-9f8f-4a35-81b8-071a421ea059` | ✓ ok | 219 |
| 2 | Iluminación con fuentes puntuales | `9bb4833c-505e-4eff-b108-e73780611cb9` | ✓ ok | 185 |
| 3 | Muestreo de importancia + múltiples emisores | `e41f49e0-627f-4108-b3f6-344eaff8e86b` | ✓ ok | 517 |
| 4 | Materiales microfacet (conductores ásperos) | `a4d962dc-2e06-44da-828c-c802690a2b5b` | ✓ ok | 730 |
| 5 | MIS (β=2) + Path Tracing explícito | `d7764605-0368-4adc-b511-bce2b128e539` | ✗ fail | 316 |

**Tiempo total:** 2052 s (~34 min), sobre tope de 3600 s.

---

## Alcance y Causa de Terminación

**Alcance:** Se completaron todas las fases (0–5) del curso secuencial. La fase 5 (MIS + Path Tracing explícito) requería dos sub-fases (5a: heurística de potencia β=2; 5b: recursión con profundidad B=10).

**Causa del gate fallido:** 
- Fase 5b (Path Tracing explícito) no convergió correctamente. El criterio de aceptación exigía cero píxeles NaN/negativos en los renders con profundidad de camino B=10, especialmente en la escena `MultiL` donde el aluminio tiene α=0.03 (muy rugoso). Las funciones de densidad de probabilidad (pdf) del modelo microfacet pueden desbordar, anularse, o producir 0/0 ante α muy pequeño. La referencia del curso se generó con guardas explícitas (descartar términos ante pdf no finita o ≈0, peso MIS w=0 ante 0/0), pero esas guardas probablemente no se implementaron correctamente. El check.sh de fase 5 valida esto con `ppm_valid()`: rechaza archivos con valores fuera de [0,255], lo que atrapa pixeles NaN convertidos a tokens gigantes al pasar por `toDisplayValue()`.

---

## Por Cada Fase Completada

### Fase 00 — Intersección rayo-esfera y renders de depuración

**Implementado:**
- Algoritmo de intersección rayo-esfera cerrada (`Sphere::intersect`): resuelve `|o + t·d − p|² = r²`, devuelve la menor raíz positiva con épsilon 1e−4.
- Función global `intersect(ray, t, id)` que recorre la escena y retorna la intersección más cercana.
- Dos modos de shading: (1) `normal`: suma el color del objeto + normal (sin reescala), con clamp global; (2) `distance`: mapeo lineal de la distancia de intersección a escala de gris.
- Paralelización OpenMP determinista sobre renglones; generador de números aleatorios por píxel (erand48).
- Volcado PPM P3 (ASCII, γ=1/2.2).

**Verificación:** 
- Visual: `image-normalcolor.ppm` y `image-distance.ppm` coinciden con referencias del curso (franjas cian/magenta en esferas, gris monótono en fondo).
- Determinismo: misma imagen exacta (`cmp`) con 1 o N hilos OpenMP.
- Probe.py: tolerancia ±10/255 en colores tras gamma.

---

### Fase 01 — Iluminación directa por Monte Carlo

**Implementado:**
- Cambio de escena: emisión agregada a `Sphere` (parámetro `Color ke`); emisor esférico de radio 10.5 en el techo con radiancia (10,10,10).
- Estimador Monte Carlo: por píxel, N muestras (spp configurable). Cada muestra: muestrear dirección Wi desde el punto de intersección x, lanzar rayo; si alcanza un emisor, acumular `Le · (albedo/π) · cos(θ) / pdf(wi)`. Si el rayo de cámara toca directamente un emisor, devolver su radiancia (sin pintar negro).
- Tres métodos de muestreo: (1) uniforme esférico (pdf=1/(4π)); (2) uniforme hemisférico alineado a la normal (pdf=1/(2π)); (3) coseno hemisférico (pdf=cos(θ)/π).
- Base ortonormal local sobre n para los hemisféricos.
- RNG seguro para OpenMP: erand48 por píxel, sembrado con índice de píxel.
- CLI: `./rt <sampler> <spp>` (sampler ∈ {uniformsphere, uniformhemi, cosinehemi}; spp ∈ {32, 512, 2048}).
- Salida: `image.ppm` (promediado después del clamp).

**Verificación:**
- Visual: 9 combinaciones (3 samplers × 3 spp) coinciden con referencias; ruido disminuye al subir spp; coseno < hemisférico < esférico en varianza.
- Emisor se ve blanco brillante (clamped).
- Reproducibilidad: misma semilla → mismo PPM bit a bit, 1 o N hilos.

---

### Fase 02 — Iluminación con fuentes puntuales

**Implementado:**
- Eliminación de la esfera emisora; introducción de fuente puntual en (0, 24.3, 0) con intensidad radiante I=(4000,4000,4000).
- Shading determinista (sin Monte Carlo, 1 rayo por píxel): `Lo = (albedo/π) · I · cos(θ) / r²` con r distancia a la fuente.
- Rayo de sombra: antes de acumular, lanza un rayo desde x hacia la fuente; si alguna esfera lo bloquea (t_hit < r − ε), contribución = 0 → sombras duras.
- Representación: esfera de radio 0 (ningún rayo la intersecta).
- Conservación de la arquitectura: modo puntual convive con modos Monte Carlo de la fase 1 sin regresión.
- CLI: `./rt point` para modo puntual; `./rt <sampler> <spp>` sigue funcionando.
- Salida: `image-plight.ppm`.

**Verificación:**
- Visual: `image-plight.ppm` coincide con referencia; sombras duras y nítidas bajo esferas; caída 1/r² visible en paredes.
- Determinismo: dos corridas → `cmp` idéntico.
- Probe.py: píxeles específicos validados (techo bajo luz, sombras duras, paredes).
- Sin autoiluminación del plano que contiene la fuente; sin acné de sombra.

---

### Fase 03 — Muestreo de importancia de fuentes + múltiples emisores

**Implementado:**
- Dos métodos de muestreo de fuentes esféricas por importancia:
  1. **Muestreo de área:** uniforme sobre la superficie, conversión a ángulo sólido (pdf_ω = pdf_área · d² / |cos(θ_luz)|).
  2. **Muestreo de ángulo sólido:** dirección uniforme dentro del cono subtendido (θ_max, pdf_ω = 1/(2π(1−cos(θ_max)))).
- Múltiples emisores: por cada muestra, iterar TODAS las fuentes (esféricas + puntuales) y sumar contribuciones. Puntuales con término determinista (fase 2); esféricas con muestreo de importancia elegido.
- Dos escenas:
  - `1L`: emisor único (fase 1) para comparar muestreo de importancia vs coseno hemisférico.
  - `2A1P`: puntual r=0 (2000,2000,2000) + dos emisoras en (-23,24.3,0) con Le=(12,5,5) y (23,24.3,-50) con Le=(5,5,12).
- CLI: `./rt <modo> <spp> <escena>` (modo ∈ {arealight, solidangle, coshemi}; escena ∈ {1L, 2A1P}).
- RNG determinista por píxel.

**Verificación:**
- Visual: renders a 32 spp coinciden con referencias; menos ruido que coseno hemisférico a igual spp.
- Ángulo sólido produce menor varianza que área para fuentes pequeñas/lejanas.
- Escena 2A1P muestra tonos rojizo y azulado de los dos emisores, sombras múltiples coherentes.
- Energía no negativa, sin NaN.
- Reproducibilidad por semilla, 1 o N hilos.

---

### Fase 04 — Materiales microfacet (conductores ásperos)

**Implementado:**
- Sustitución de las dos esferas inferiores (r=16.5) por conductores ásperos con α=0.3: aluminio (izquierda) y oro (derecha), con índices (η, κ) por canal RGB dados.
- BRDF microfacet: `fr = D(ωh)·G(ωo,ωi)·F(ωo·ωh) / (4·cosθo·cosθi)` con:
  - **D de Beckmann:** `exp(−tan²θh/α²)/(π·α²·cos⁴θh)`.
  - **G de Smith:** `G1(ωi)·G1(ωo)` con fórmula de Schlick optimizada.
  - **Fresnel de conductor exacto por canal RGB** (η, κ dados).
- Tres configuraciones de luces: `point` (solo puntual), `areal` (esfera emisora), `2a1p` (escena multiples).
- Dos partes con estrategias de muestreo distintas:
  1. **Parte 1 (ISLight):** muestreo de fuente de luz (ángulo sólido, fase 3) + evaluación BRDF microfacet.
  2. **Parte 2 (ISBRDF):** muestreo de BRDF por distribución D (half-vector Beckmann); contribución solo si alcanza emisor.
- API sample/eval/pdf expuesta: materiales difusos (coseno hemisférico) adaptados a la misma interfaz.
- CLI: `./rt <modo> <spp> <escena>` (modo ∈ {issa, isarea, isbrdf}; escena ∈ {point, areal, 2a1p}).
- RNG determinista por píxel.

**Verificación:**
- Visual: reflejos metálicos con highlights rugosos (aluminio neutro, oro amarillo); tonos correctos de luces en reflejos (escena 2a1p).
- Convergencia: parte 1 y parte 2 convergen a la misma imagen al subir spp (diferencia solo en distribución de ruido).
- Conservación de energía: sin NaN ni fireflies negativos; F ≤ 1 por canal.
- Materiales difusos sin regresión vs fase 3.
- Reproducibilidad por semilla, 1 o N hilos.

---

### Fase 05 — MIS (β=2) + Path Tracing explícito

**Implementado (intento):**
- **Sub-fase 5a (MIS):** combinación de muestreo de luz + muestreo de BRDF con heurística de potencia β=2 (w(p_a,p_b) = p_a²/(p_a²+p_b²)). Por fuente de área: dos términos ponderados (luz + BRDF). Puntuales conservan término determinista sin peso MIS. Estimador de iluminación directa que no cambia la imagen final vs fase 4, pero reduce varianza.
- **Sub-fase 5b (Path Tracing explícito):** recursión `shade(ray, bounce)` con profundidad máxima B=10. Cada vértice: iluminación directa (MIS de 5a) + muestreo de BRDF recursivo. Sesgado por diseño: emisor solo cuenta si bounce==1 (la luz de rebotes posteriores ya la cuenta NEE).
- Tres escenas de referencia: `PointL` (I=4000), `AreaL` (esfera r=10.5), `MultiL` (escena 2A1P con aluminio α=0.03).
- Guardas para pdf: rechazar términos ante pdf no finita o ≈0, peso MIS w=0 ante 0/0.

**Verificación (fallida):**
- El check.sh valida cero píxeles NaN/negativos en los renders PPM (función `ppm_valid`).
- La escena `MultiL` con aluminio α=0.03 muy rugoso genera desbordamientos o anulaciones de pdf.
- Probablemente las guardas no se implementaron correctamente o no se aplicaron en todos los puntos críticos.
- El gate falló en la validación de píxeles fuera de [0,255], indicando presencia de NaN en los renders.

---

## Incidencias y Decisiones Técnicas Relevantes

1. **Fase 0 — Certidumbre en la referencia:** El criterio de aceptación exigía tolerancia ±10/255 en probe.py. La rigurosidad fue crítica: no hay margen para desviaciones de la fórmula de intersección.

2. **Fase 1 — RNG determinista:** El uso de `erand48` con semilla fija por píxel fue decisivo para reproducibilidad bit-a-bit en imágenes estocásticas. OpenMP sin estado compartido mutable.

3. **Fase 2 — Transición deductivo → Monte Carlo:** El cambio de emisor esférico (volumen) a puntual (singularidad) requirió cuidado con el rayo de sombra: épsilon de 1e−4 evita auto-intersección.

4. **Fase 3 — Muestreo por importancia:** Dos métodos (área vs ángulo sólido) ejercitan la conversión entre espacios de muestreo. Ángulo sólido es superior para fuentes lejanas/pequeñas.

5. **Fase 4 — Microfacets complejos:** Fresnel de conductor exacto por canal RGB añade complejidad; los índices (η, κ) son específicos por material. Distribución D de Beckmann es más computacionalmente simple que GGX, pero suficiente. Smith G es crítica para evitar contribuciones negativas.

6. **Fase 5 — Guardas de pdf:** El criterio explícito de "cero píxeles NaN/negativos" y la mención de que el código del curso carece de guardas subraya que α=0.03 genera singularidades. Las pdf pueden:
   - Anularse (cos(θh)→0 en conductor muy rugoso).
   - Desbordar (términos en denominador muy pequeños).
   - Producir 0/0 en pesos MIS (ambas pdf = 0).

---

## Autoevaluación y Recomendaciones para Futuro

### Qué funcionó bien
- **Paralelización y determinismo:** OpenMP sin estado compartido mutable permitió reproducibilidad bit-a-bit desde fase 1.
- **Progresión incremental:** cada fase extendía la anterior sin borrar lo anterior (no regresión).
- **Validación temprana:** probe.py y check.sh en fases tempranas detectaron desviaciones rápido.

### Qué falló y por qué
- **Fase 5 — Guardas incompletas:** Las singularidades de pdf en α muy pequeño (0.03) no se cubrieron exhaustivamente. Posibles puntos de fallo:
  - Fresnel de conductor: división por términos muy pequeños sin verificación.
  - BRDF de Beckmann: tan(θh) → ∞ cuando θh → π/2, cos(θh) → 0.
  - Pesos MIS: 0/0 cuando ambas pdf = 0, no manejado explícitamente en todos los términos.

### Con más tiempo

1. **Auditoría de singularidades de pdf:** Ejecutar `gdb` o insertar logs en cada evaluación de pdf/fresnel/D para capturar casos de 0/0, infinito, o NaN.

2. **Test de regresión de α:** Generar renders con α=0.3 (fase 4) vs α=0.03 (fase 5, escena MultiL) lado a lado; comparar píxel a píxel con `compare-ref.py` o similar para aislar la escena problemática.

3. **Desglose de contribuciones:** Separar visualización de componentes (Ld solo, Lind solo, peso MIS) para verificar que cada término es válido por separado antes de sumarse.

4. **Ruleta rusa:** Aunque fuera del alcance oficial, los caminos largos (B=10) con α pequeño pueden requerir Russian roulette para domar varianza y evitar overflow numérico.

5. **Validación contra referencia:** Antes de check.sh, renderizar una imagen conocida (p. ej., escena PointL a 32 spp) y compararla pixel a pixel contra la referencia usando correlación visual (SSIM, PSNR).

---

## Conclusión

El curso alcanzó fase 5 de 6 fases estructuradas (00–05). Las fases 0–4 completaron exitosamente un raytracer progresivo desde intersección básica hasta materiales microfacet. La fase 5 implementó MIS y path tracing recursivo, pero falló el gate de validación por presencia de píxeles NaN en renders de alta rugosidad (α=0.03). La raíz técnica es manejable (guardas exhaustivas de pdf), pero su ubicación exacta en el flujo de shading requeriría auditoría detallada con herramientas de debug.

**Tiempo utilizado:** 2052 s de 3600 s (~57% del presupuesto).

