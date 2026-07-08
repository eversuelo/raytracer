PROYECTO FINAL

A su elección, una de las siguientes opciones

OPCION 1:
Implementar muestreo de importancia múltiple (luz y brdf) utilizando la heurística de potencia con beta=2
Recomiendo utilizar la estructura del código del proyecto 4 parte 1.
Pseudocodigo:
para cada fuente luminosa
      wl = muestreoFuente
      Ll = Le*fr*cos/pl(wl);
      wb = muestreoBRDF
      si wb intersecta una fuente luminosa:
              Lb = Le*fr*cos/pb(wb);
      sino
              Lb = 0;
      L = L + Ll*w(pl(wl), pb(wl)) + Lb*w(pb(wb), pl(wb));

En caso que necesiten una explicación alternativa a la que dimos en clase: 
https://www.pbr-book.org/3ed-2018/Light_Transport_I_Surface_Reflection/Direct_Lighting#EstimatingtheDirectLightingIntegral

Las imágenes obtenidas deben ser las mismas que en el proyecto 4, pero debe existir una reducción de variancia para la misma cantidad de muestras por pixel.


OPCION 2:
Implementar Path Tracing Explicito sesgado (terminando al llegar a una fuente luminosa o cuando el rayo escapa la escena).
De igual manera, utilizar el código del proyecto 4 parte 1 como base.
Pseudocódigo:
shade(ray, bounce)
       si ray escapa
              return 0
       si ray intersecta fuente luminosa
                 si bounce==1
                            return radiancia de la fuente
                 sino
                            return 0
        x = punto en que se intersecta
        Ld = calcular iluminación directa para x usando muestreo de luz con angulo solido
        wr = muestreoBRDF
        pr = pdfBRDF(wr)
        nuevorayo = Rayo(x,wr)
        Lind = fr(-ray.d, wr) * (cos/pr) * shade( nuevorayo, bounce+1);
        return Ld + Lind;


Se anexan renders de referencia para la opción 2, limitando la longitud de los caminos a un máximo de 10. Los renders con una sóla fuente (área o puntual) utilizan la misma escena que hemos venido trabajando. Para la escena con múltiples luces, modifiqué la aspereza de la esfera de aluminio a alpha=0.03 para obtener una imagen un poco más interesante.