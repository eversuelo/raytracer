PROYECTO 2

Fecha de entrega: Jueves 21 de octubre de 2021
Favor de hacerme llegar por correo electrónico a luis.gamboa@umich.mx
- Código fuente y Makefile: el código fuente debe cubrir las dos etapas de este proyecto.
- No incluir binarios.
- (opcional) Imagenes en formato jpg o png de los renders obtenidos.
- Asunto del correo: labgraficacion proyecto2


Descripción:
- Primera etapa: extender el código generado en el proyecto 1, de modo que ahora shade estime el color del pixel utilizando Monte Carlo para resolver la ecuación de renderizado de Iluminación Directa. En esta etapa, algunas esferas podrán emitir luz y todos los materiales serán difusos. Implementar los tres métodos de muestreo de direcciones: uniforme esférico y hemisférico, y coseno hemisférico.
- Segunda etapa: agregar luces puntuales.


Imágenes de referencia y parámetros de escena:

Las imágenes de referencia que les proporciono, fueron generadas utilizando los siguientes parámetros de la escena:
        Sphere(1e5,  Point(-1e5 - 49, 0, 0),     Color(.75, .25, .25),    Color()), // pared izq
        Sphere(1e5,  Point(1e5 + 49, 0, 0),     Color(.25, .25, .75),    Color()), // pared der
        Sphere(1e5,  Point(0, 0, -1e5 - 81.6),  Color(.25, .75, .25),    Color()), // pared detras
        Sphere(1e5,  Point(0, -1e5 - 40.8, 0),  Color(.25, .75, .75),    Color()), // suelo
        Sphere(1e5,  Point(0, 1e5 + 40.8, 0),  Color(.75, .75, .25),    Color()), // techo
        Sphere(16.5, Point(-23, -24.3, -34.6),  Color(.2, .3, .4),          Color()), // esfera abajo-izq
        Sphere(16.5, Point(23, -24.3, -3.6),     Color(.4, .3, .2),           Color()), // esfera abajo-der
        Sphere(10.5, Point(0, 24.3, 0),             Color(1, 1, 1),             Color(10,10,10)) // esfera arriba


Se adjuntan renders utilizando 3 métodos de muestreo (uniforme esférico, uniforme hemisférico y coseno hemisférico) . Con cada uno de los métodos de muestreo de direcciones, se renderizó utilizando 32, 512 y 2048 muestras por pixel (spp).


Aclaración sobre la apariencia de la fuente luminosa:
Estrictamente hablando, la ecuación de iluminación directa, sólo considera la iluminación directa recibida desde todas las direcciones. En los renders de referencia, si aplicáramos la ecuación tal cual, la esfera que funge como fuente luminosa aparecería completamente negra. Para solucionar este caso particular, podemos asumir que cuando el rayo intersecta un objeto que emite luz, simplemente se regresa la emisión de dicho objeto como la radiancia saliente.