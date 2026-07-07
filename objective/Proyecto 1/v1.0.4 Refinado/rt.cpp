// rt: un lanzador de rayos minimalista
// g++ -O3 -fopenmp rt.cpp -o rt
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <omp.h>
/*
double tmin = INFINITY;//Esto es para ubicar la tmin
double tmax = -INFINITY;//Esto es para ubicar la tmax
*/

//Las ultimas tmin tmax:
double tmax = 304.110891;
double tmin = 144.676098;
void acotar(double &t)
{
	double diferencial = (tmax - tmin);
	t -= tmin;
	t /= diferencial;
	printf("La distancia es: %f\n", t);
}
class Vector
{
public:
	double x, y, z; // coordenadas x,y,z

	// Constructor del vector, parametros por default en cero
	Vector(double x_ = 0, double y_ = 0, double z_ = 0)
	{
		x = x_;
		y = y_;
		z = z_;
	}

	// operador para suma y resta de vectores
	Vector operator+(const Vector &b) const { return Vector(x + b.x, y + b.y, z + b.z); }
	Vector operator-(const Vector &b) const { return Vector(x - b.x, y - b.y, z - b.z); }
	// operator multiplicacion vector y escalar
	Vector operator*(double b) const { return Vector(x * b, y * b, z * b); }

	// operator % para producto cruz
	Vector operator%(Vector &b) { return Vector(y * b.z - z * b.y, z * b.x - x * b.z, x * b.y - y * b.x); }
	Vector cruz(const Vector &b) const { return Vector((y * b.z) - (z * b.y), (z * b.x) - (x * b.z), (x * b.y) - (y * b.x)); }

	// producto punto con vector b
	double dot(const Vector &b) const { return x * b.x + y * b.y + z * b.z; }

	// producto elemento a elemento (Hadamard product)
	Vector mult(const Vector &b) const { return Vector(x * b.x, y * b.y, z * b.z); }

	// normalizar vector
	Vector &normalize() { return *this = *this * (1.0 / sqrt(x * x + y * y + z * z)); }
	double length_squared() const
	{
		return x * x + y * y + z * z;
	}
	double magnitude()
	{
		return sqrt(x * x + y * y + z * z);
	}
};
typedef Vector Point;
typedef Vector Color;

class Ray
{
public:
	Point o;
	Vector d;								   // origen y direcccion del rayo
	Ray(Point o_, Vector d_) : o(o_), d(d_) {} // constructor
};

class Sphere
{
public:
	double r; // radio de la esfera
	Point p;  // posicion
	Color c;  // color

	Sphere(double r_, Point p_, Color c_) : r(r_), p(p_), c(c_) {}

	// PROYECTO 1
	// determina si el rayo intersecta a esta esfera
	double intersect(const Ray &ray) const
	{
		Vector L = ray.o - p;
		double a = ray.d.dot(ray.d);
		double b = (ray.d.dot(L)) * 2;
		double c = L.dot(L) - r * r;
		double discriminant = b * b - 4 * a * c;
		double t0, t1;
		if (discriminant < 0)
			return 0.00;
		else if (discriminant == 0)
		{
			t0 = t1 = -0.5 * b / a;
		}
		else
		{
			//double q = (b > 0) ? -0.5 * (b + sqrt(discriminant)) : -0.5 * (b - sqrt(discriminant));
			//t0 = q / a;
			//t1 = c / q;
			t0 = ((-1 * b) - sqrt(discriminant)) / (2 * a);
			t1 = ((-1 * b) + sqrt(discriminant)) / (2 * a);
		}

		if (t0 > t1)
		{
			double temp = t0;
			t0 = t1;
			t1 = temp;
		}
		//Investigar que quiere decir distancia menor que 0
		//Ejemplo ver en octave o matlab donde cae el rayo o punto
		if (t0 < 0)
		{
			t0 = t1; // si t0 es negativa usa t1 en su lugar
			if (t0 < 0)
				return 0.00; //Tanto t1 como t0 son negativas
		}
		return t0;
	}
};
int NoSpheres = 8;

Sphere spheres[] = {
	//Escena: radio, posicion, color
	Sphere(16.5, Point(-23, -24.3, -34.6), Color(1.00, 1.00, 1.00)), // esfera abajo-izq
	Sphere(1e5, Point(-1e5 - 49, 0, 0), Color(1.00, 1.00, 1.00)),	 // pared izq
	Sphere(1e5, Point(1e5 + 49, 0, 0), Color(1.00, 1.00, 1.00)),	 // pared der
	Sphere(16.5, Point(23, -24.3, -3.6), Color(1.00, 1.00, 1.00)),	 // esfera abajo-der
	Sphere(1e5, Point(0, 0, -1e5 - 81.6), Color(1.00, 1.00, 1.00)),	 // pared detras
	Sphere(10.50, Point(0, 24.3, 0), Color(1.00, 1.00, 1.00)),		 // esfera arriba
	Sphere(1e5, Point(0, -1e5 - 40.8, 0), Color(1.00, 1.00, 1.00)),	 // suelo
	Sphere(1e5, Point(0, 1e5 + 40.8, 0), Color(1.00, 1.00, 1.00))	 // techo

};

// limita el valor de x a [0,1]
inline double clamp(const double x)
{
	if (x < 0.0)
		return 0.0;
	else if (x > 1.0)
		return 1.0;
	return x;
}

// convierte un valor de color en [0,1] a un entero en [0,255]
inline int toDisplayValue(const double x)
{
	return int(pow(clamp(x), 1.0 / 2.2) * 255 + .5);
}

// PROYECTO 1
// calcular la intersección del rayo r con todas las esferas
// regresar true si hubo una intersección, falso de otro modo
// almacenar en t la distancia sobre el rayo en que sucede la interseccion
// almacenar en id el indice de spheres[] de la esfera cuya interseccion es mas cercana
inline bool intersect(const Ray &r, double &t, int &id)
{
	double neart = INFINITY; // t más cercana (near t in english)

	bool centinela = false; //Centinela de Verificacióm
	double tarr[8];			//Arreglo para almacenar los calculos de todas las esferas
	for (int i = 0; i < NoSpheres; i++)
	{
		tarr[i] = spheres[i].intersect(r); //Calculamos la interseccion i
		if (tarr[i] > 0)				   //Nos brindara una verificacion de distancia
		{
			centinela = true;	 // Si existe intersección
			if (tarr[i] < neart) // Encontramos la t mas cercana
			{
				neart = tarr[i]; // la t mas cercana
			}
		}
	}
	for (int i = 0; i < NoSpheres; i++)
	{
		if (tarr[i] == neart) //buscaremos el indice de la t mas cercana
		{
			id = i;		 //guardamos el indice
			t = tarr[i]; //guardamos la distancia
		}
	}

	return centinela; //confirmamos si hubo interseccion
}
void distance() {}
// Calcula el valor de color para el rayo dado
Color shade(const Ray &r)
{
	double t = 0;

	int id = 0;
	// determinar que esfera (id) y a que distancia (t) el rayo intersecta
	if (!intersect(r, t, id))
	{
		return Color(); // el rayo no intersecto objeto, return Vector() == negro
	}
	const Sphere &obj = spheres[id];
	//0.) Metodo es: Factor de Reduccion directa que es la division
	//1.)Restas la decena o la centena mas pequeña o la unidad o el millar //Factor Minimo
	//2.)Divides entre dos si el numero es mas grande que el factor Minimo
	//3.) Vuelves al paso 1
	/*if (t > tmax)//Busca la tmax
	{
		tmax = t;
	}
	if (t < tmin)//Busca la tmin
	{
		tmin = t;
	}*/
	acotar(t);

	Color colorValue = obj.c * t;
	return colorValue;
}

int main(int argc, char *argv[])
{
	int w = 1024, h = 768; // image resolution

	// fija la posicion de la camara y la dirección en que mira
	Ray camera(Point(0, 11.2, 214), Vector(0, -0.042612, -1).normalize());

	// parametros de la camara
	Vector cx = Vector(w * 0.5095 / h, 0., 0.);
	Vector cy = (cx % camera.d).normalize() * 0.5095;

	// auxiliar para valor de pixel y matriz para almacenar la imagen
	Color *pixelColors = new Color[w * h];

	// PROYECTO 1
	// usar openmp para paralelizar el ciclo: cada hilo computara un renglon (ciclo interior),
	int np;
#if defined(_OPENMP)
	np = omp_get_num_threads(); //Defome el número de procesadores
#endif
	/*
Una cláusula schedule indica la forma como se dividen las iteraciones del for entre los
threads:
• schedule(static,tamaño) las iteraciones se dividen según el tamaño, y la asignación
se hace estáticamente a los threads. Si no se indica el tamaño se divide por igual
entre los threads.
• schedule(dynamic,tamaño) las iteraciones se dividen según el tamaño y se asignan
a los threads dinámicamente cuando van acabando su trabajo.
• schedule(guided,tamaño) las iteraciones se asignan dinámicamente a los threads
pero con tamaños decrecientes, empezando en tamaño numiter/np y acabando en
“tamaño”.
• schedule(runtime) deja la decisión para el tiempo de ejecución, y se obtienen de la
variable de entorno OMP_SCHEDULE.
**/

#pragma omp parallel for schedule(dynamic, np)
	for (int y = 0; y < h; y++)
	{

		// recorre todos los pixeles de la imagen
		fprintf(stderr, "\r%5.2f%%", 100. * y / (h - 1));
		for (int x = 0; x < w; x++)
		{

			int idx = (h - y - 1) * w + x; // index en 1D para una imagen 2D x,y son invertidos
			Color pixelValue = Color();	   // pixelValue en negro por ahora
			// para el pixel actual, computar la dirección que un rayo debe tener
			Vector cameraRayDir = cx * (double(x) / w - .5) + cy * (double(y) / h - .5) + camera.d;

			// computar el color del pixel para el punto que intersectó el rayo desde la camara
			pixelValue = shade(Ray(camera.o, cameraRayDir.normalize()));

			// limitar los tres valores de color del pixel a [0,1]
			pixelColors[idx] = Color(clamp(pixelValue.x), clamp(pixelValue.y), clamp(pixelValue.z));
		}
	}

	fprintf(stderr, "\n");

	// PROYECTO 1
	// Investigar formato ppm
	FILE *f = fopen("image.ppm", "w");
	// escribe cabecera del archivo ppm, ancho, alto y valor maximo de color
	fprintf(f, "P3\n%d %d\n%d\n", w, h, 255);
	for (int p = 0; p < w * h; p++)
	{ // escribe todos los valores de los pixeles
		fprintf(f, "%d %d %d ", toDisplayValue(pixelColors[p].x), toDisplayValue(pixelColors[p].y),
				toDisplayValue(pixelColors[p].z));
	}
	fclose(f);

	delete[] pixelColors;
	printf("tmin: %f\ttmax: %f\n", tmin, tmax);
	return 0;
}
