PROYECTO 3

PARTE 1
- Implementar muestreo de importancia de fuentes luminosas esféricas usando dos técnicas:
       - Muestreo de área
       - Muestreo del ángulo sólido

PARTE 2
- Extender el mecanismo de muestreo de importancia para soportar múltiples emisores en la escena, incluyendo fuentes de área y puntuales.
Los renders de referencia se generaron utilizando las siguientes fuentes luminosas
        Sphere(0.0, Point(0, 24.3, 0),        Color(0, 0, 0),          Color(2000,2000,2000)),
        Sphere(10.5, Point(-23, 24.3, 0),        Color(1, 1, 1),          Color(12,5,5)),
        Sphere(5, Point(+23, 24.3, -50),        Color(1, 1, 1),          Color(5,5,12))
La modificación necesaria a nuestro renderer, consiste en que por cada muestra del estimador Monte Carlo, se muestree CADA fuente luminosa y se acumule el resultado. Es decir una sóla muestra del estimador Monte Carlo debe hacer algo como esto:
      Color value;
      for each fuente in fuentesluminosas
      do
             Le = sample(fuente)
             p = pdf(fuente)
             value = value + Le*fr*cos/p 
      done
Nota: el render 2A1P-cosinehemi fue generado con el proyecto 2, SIN considerar esta modificación, por lo que su código del proyecto anterior debería poder generar sin problemas esta imagen.


RENDERS DE REFERENCIA
Para la misma escena que el proyecto 2, utilizando 32spp