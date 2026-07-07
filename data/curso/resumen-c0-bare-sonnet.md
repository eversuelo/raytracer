# Resumen — curso c0-bare@sonnet (raytracer IA7200-L)

Condición: **c0-bare** (sin memoria persistente) · Modelo: **Sonnet** · Rama:
`curso/c0-bare-sonnet` · Tope de sesión: 60 min (3600 s) · Tiempo total usado: **1322 s**
(~22 min, muy por debajo del tope).

## 1. Tabla por fase

| Fase | Proyecto del curso | Run ID | Gate | Duración |
|---|---|---|---|---|
| 0 | Proyecto 1 — intersección + debug | `a6012b9d-dd36-4eb9-b348-29cec5581595` | ✅ ok | 105 s |
| 1 (intento 1) | Proyecto 2 parte 1 — MC directa, emisor esférico | *(sin id)* | ❌ fail | 32 s |
| 1 (intento 2) | Proyecto 2 parte 1 — MC directa, emisor esférico | *(sin id)* | ❌ fail | 31 s |
| 1 (intento 3) | Proyecto 2 parte 1 — MC directa, emisor esférico | `1d1901ac-1c29-4ae5-bf94-c556d9b73a32` | ✅ ok | 43 s |
| 2 | Proyecto 2 parte 2 — fuente puntual | `5d70588e-a8b0-40a6-9e16-9886e690eb1f` | ✅ ok | 234 s |
| 3 | Proyecto 3 — IS de fuentes + múltiples emisores | `e3455874-94bd-4ca4-8afc-206480b02278` | ✅ ok | 450 s |
| 4 | Proyecto 4 — microfacets (conductores ásperos) | `59d61e68-273b-46df-b452-98a842bd8b77` | ✅ ok | 338 s |
| 5 | Proyecto final — MIS (β=2) / Path Tracing explícito | `7ffa99a6-ece0-4982-8aa6-31f42ee0417b` | ❌ fail | 141 s |

## 2. Hasta dónde llegó el curso y por qué se detuvo

El curso avanzó de forma limpia hasta el **Proyecto 4 (fase 4)**, con gate en verde en
cada fase aceptada. En la **fase 5** (Proyecto final: combinar muestreo de luz y BRDF
con MIS β=2, y/o path tracing explícito sesgado con longitud máxima de camino) el gate
de `sdd/phase-05/check.sh` terminó en rojo. Como el curso es **secuencial y no avanza
sobre una base rota** (regla del harness), no hubo commit de fase 5 ni se intentó una
fase 6: el `rt.cpp` que queda en el repo corresponde íntegramente al estado aceptado de
la fase 4 (no expone el modo `ptexp` que pide `check.sh` de fase 5), y ese es el punto
final de la corrida.

El corte fue por **fallo de gate, no por tiempo**: se usaron 1322 de los 3600 s
disponibles (~37% del tope).

## 3. Qué se implementó en cada fase completada y cómo se verificó

**Fase 0 — Proyecto 1 (intersección + debug).** Se completó `Sphere::intersect`
(solución de la cuadrática rayo-esfera con la raíz positiva más cercana), la función
global `intersect()` para hallar el hit más cercano de la escena, y `shade()` en dos
modos de depuración (color = normal, y gris = distancia normalizada), con el bucle de
píxeles paralelizado en OpenMP de forma determinista. Verificado con
`make && bash sdd/phase-00/check.sh` (usa `probe.py` para comparar contra
`image-normalcolor.jpg` / `image-distance.jpg`).

**Fase 1 — Proyecto 2 parte 1 (iluminación directa Monte Carlo).** Se implementó el
estimador Monte Carlo de iluminación directa con emisor esférico, con tres
muestreadores de dirección seleccionables por CLI (uniforme esférico, uniforme
hemisférico, coseno hemisférico) y un RNG por hilo/píxel para determinismo bajo OpenMP.
Verificado con `check.sh`, que compara las 9 combinaciones (3 samplers × {32,512,2048}
spp) contra las referencias `.jpg` con `compare-ref.py`. Costó tres intentos: los dos
primeros (32 s y 31 s) fallaron el gate antes de que el tercero (43 s) lo pasara.

**Fase 2 — Proyecto 2 parte 2 (fuente puntual).** Se reemplazó el emisor esférico por
una fuente puntual determinista (I=4000, modelada como esfera de radio 0), con el
término `Lo = (albedo/π)·I·cosθ/r²` y rayo de sombra para sombras duras, preservando sin
regresión el modo Monte Carlo de la fase 1. Verificado con `check.sh`: comparación
contra `image-plight.jpg` y prueba de reproducibilidad (dos corridas idénticas por
`cmp`).

**Fase 3 — Proyecto 3 (muestreo de importancia de fuentes + múltiples emisores).** Se
añadió muestreo de importancia de fuentes esféricas por dos métodos (área y ángulo
sólido subtendido) y soporte para múltiples emisores simultáneos (puntuales + de área),
acumulando la contribución de cada fuente en cada muestra del estimador. Verificado con
`check.sh` sobre las escenas `1L` y `2A1P` (modos `arealight`/`solidangle`) contra sus
referencias, incluyendo el criterio de menor varianza frente al coseno hemisférico de la
fase 1.

**Fase 4 — Proyecto 4 (microfacets, conductores ásperos).** Se introdujo el primer
material no difuso: conductores ásperos con modelo microfacet Torrance–Sparrow (D de
Beckmann, G de Smith, Fresnel de conductor exacto por canal RGB) para esferas de
aluminio y oro, separando la interfaz de materiales en `sample`/`eval`/`pdf` y
soportando dos estrategias de muestreo (por fuente de luz y por BRDF vía half-vector).
Verificado con `check.sh`: comparación de las escenas `RC-*` (point/areal/2a1p) e
`ISBRDF-{32,512}` contra referencias, consistencia entre ambas estrategias de muestreo,
y ausencia de NaN/energía negativa.

**Fase 5 — no completada.** Se intentó el proyecto final (MIS β=2 y/o path tracing
explícito, modo `ptexp`) mencionado en `sdd/phase-05/spec.md`, pero el gate falló en
141 s. Esta sesión no tuvo acceso a los logs de esa corrida (fuera del directorio de
trabajo permitido) para precisar en qué criterio de `check.sh` falló exactamente; el
único hecho verificable desde el repo es que `rt.cpp` no quedó con el modo `ptexp`
implementado.

## 4. Incidencias y decisiones técnicas relevantes

- **Modelado de fuentes puntuales como esferas de radio 0** con `Le = I`: decisión
  tomada en la fase 2 y reutilizada en las fases 3 y 4 para unificar el pipeline de
  emisores (puntuales y de área comparten la misma lista de luces).
- **Interfaz `sample`/`eval`/`pdf` de materiales**, introducida en la fase 4 para poder
  combinar muestreo de luz y de BRDF — es exactamente el prerequisito que pedía la fase
  5 (MIS), pero la combinación final no se logró antes de que el gate fallara.
- **Tres intentos en la fase 1**: los dos primeros fallaron el gate en ~31-32 s cada
  uno (rápido, consistente con un error temprano — p. ej. sampler equivocado o RNG no
  determinista entre hilos, criterios explícitamente calibrados en `check.sh`) antes
  de lograrlo en el tercer intento.
- El harness respetó la regla de secuencialidad: **un gate en rojo en fase 5 no generó
  commit ni intento de fase 6**, dejando el repo en el último estado aceptado (fase 4).

## 5. Autoevaluación breve

Con más tiempo, valdría la pena invertirlo en **iterar sobre la fase 5** en lugar de
detenerse tras un solo intento de 141 s: dado que las corridas de referencia de esa fase
pueden tomar bastantes minutos (renders de 512 spp), un fallo tan rápido probablemente
ocurrió temprano (compilación, o la primera comparación a 32 spp) y con un segundo
intento — revisando el stderr de `check.sh` antes de terminar — probablemente se
hubiera identificado y corregido la causa dentro del margen de tiempo restante (quedaban
~38 de los 60 minutos). De forma similar, los dos intentos fallidos de la fase 1
sugieren que conviene correr una verificación rápida de determinismo (mismo resultado a
1 y N hilos) antes de invocar el gate completo, para no gastar intentos en errores
evitables de RNG/sampler.
