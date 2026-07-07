// rt: un lanzador de rayos minimalista
// g++ -O3 -fopenmp rt.cpp -o rt
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <omp.h>
#include <time.h>
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
	double magnitude() const
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
	Color ke; // campo de emisión

	Sphere(double r_, Point p_, Color c_, Color ke_) : r(r_), p(p_), c(c_), ke(ke_) {}

	// PROYECTO 1
	// determina si el rayo intersecta a esta esfera
	double intersect(const Ray &ray) const
	{
		Vector temp = ray.o - p;
		double a = ray.d.dot(ray.d);
		double b = (ray.d.dot(temp)) * 2;
		double c = temp.dot(temp) - r * r;
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
	Sphere(1e5, Point(-1e5 - 49, 0, 0), Color(.75, .25, .25), Color()),	  // pared izq
	Sphere(1e5, Point(1e5 + 49, 0, 0), Color(.25, .25, .75), Color()),	  // pared der
	Sphere(1e5, Point(0, 0, -1e5 - 81.6), Color(.25, .75, .25), Color()), // pared detras
	Sphere(1e5, Point(0, -1e5 - 40.8, 0), Color(.25, .75, .75), Color()), // suelo
	Sphere(1e5, Point(0, 1e5 + 40.8, 0), Color(.75, .75, .25), Color()),  // techo
	Sphere(16.5, Point(-23, -24.3, -34.6), Color(.2, .3, .4), Color()),	  // esfera abajo-izq
	Sphere(16.5, Point(23, -24.3, -3.6), Color(.4, .3, .2), Color()),	  // esfera abajo-der
	Sphere(10.5, Point(0, 24.3, 0), Color(1, 1, 1), Color(10, 10, 10))	  // esfera arriba

};
Point FuenteLuminosa = spheres[7].p;
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
	bool centinela = false;	 //Centinela de Verificacióm
	double tarr[8];			 //Arreglo para almacenar los calculos de todas las esferas
	for (int i = 0; i < NoSpheres; i++)
	{
		tarr[i] = spheres[i].intersect(r); //Calculamos la interseccion i
		if (tarr[i] > 0)				   //Nos brindara una verificacion de distancia
		{
			centinela = true;	 // Si existe intersección
			if (tarr[i] < neart) // Encontramos la t mas cercana
			{
				neart = tarr[i]; // la t mas cercana
				id = i;			 //guardamos el indice
				t = tarr[i];	 //guardamos la distancia
			}
		}
	}
	return centinela; //confirmamos si hubo interseccion
}
void coordinateSystem(const Vector &n, Vector &s, Vector &t)
{
	if (std::abs(n.x) > std::abs(n.y))
	{
		float invLen = 1.0f / std::sqrt(n.x * n.x + n.z * n.z);
		t = Vector(n.z * invLen, 0.0f, -n.x * invLen);
	}
	else
	{
		float invLen = 1.0f / std::sqrt(n.y * n.y + n.z * n.z);
		t = Vector(0.0f, n.z * invLen, -n.y * invLen);
	}
	s = t.cruz(n);
	//printf("Vector n-> x:%f,y:%f,z:%f \n", n.x, n.y, n.z);
	//printf("Vector s-> x:%f,y:%f,z:%f \n", s.x, s.y, s.z);
	//printf("Vector t-> x:%f,y:%f,z:%f \n", t.x, t.y, t.z);
}
Vector LocalToGlobal(const Vector &n, Vector &s, Vector &t, Vector &v)
{
	Vector global = Vector();
	global.x = v.x * s.x + t.x * v.y + n.x * v.z;
	global.y = v.x * s.y + t.y * v.y + n.y * v.z;
	global.z = v.x * s.z + t.z * v.y + n.z * v.z;
	return global;
}

Color ComputeLe(Ray r, Point x, int id2, double &distancia)
{
	distancia = (r.o - x).magnitude();
	double t = 0;
	int id = 0;
	//printf("Magnitud:%f\n", magnitud);
	if (intersect(r, t, id))
	{
		//printf("d1:%f, d2:%f, t:%f id:%d \n", distancia, distancia2, t, id);
		if (id == id2)
		{
			return spheres[7].ke;
		}
		return Color();
	}
	return Color();
}
unsigned short xsubi[3] = {1, 2, 3};
Vector MEsfera(void)
{
	double xiphi = erand48(xsubi);	//((double)rand() / (RAND_MAX));
	double xiteta = erand48(xsubi); //((double)rand() / (RAND_MAX));
	double theta = acos(1 - 2 * xiteta);
	double phi = 2 * M_PI * xiphi;
	double x_ = sin(theta) * cos(phi);
	double y_ = sin(theta) * sin(phi);
	double z_ = cos(theta);
	//printf("Vector %f,%f,%f xiteta:%f xiphi:%f,theta:%f,phi:%f\n", x_, y_, z_, xiteta, xiphi, theta, phi);
	return Vector(x_, y_, z_);
}
double PFuenteDeArea(double costheta, double distancia)
{
	double area = (4 * M_PI * spheres[7].r * spheres[7].r);
	double probabilidad = (distancia * distancia) / (area * costheta);
	return probabilidad;
}
Color EstimadorMonteCarlo(Color color, Vector n, Point x, int id)
{

	Color temp;
	Sphere FuenteLuminosa = spheres[7];
	Vector PuntoAleatorioEnLaSuperficie = MEsfera() * spheres[7].r + spheres[7].p;
	Vector direction = x - PuntoAleatorioEnLaSuperficie; //de la FUente Luminosa hacia el Punto Intersectado
	Vector normal = PuntoAleatorioEnLaSuperficie - FuenteLuminosa.p;
	normal.normalize();
	double distanciaP = direction.magnitude();
	direction.normalize();
	double costheta = n.dot(direction * -1);
	double costhetaO = normal.dot(direction);
	if (costhetaO < 0) //Costheta no debe ser mayor que 1, Coseno siempre va de 1 a -1;
	{
		return Color();
	}
	else
	{
		double distancia;
		Color Le = ComputeLe(Ray(PuntoAleatorioEnLaSuperficie, direction), x, id, distancia);
		if (Le.x > 0)
		{
			if (fabs(distanciaP - distancia) > 1e-8)
			{
				printf("Hay un Problema");
			}
		}
		//Verificar los angulos, que el punto muestreado este en el hemisferio visible desde x,
		//printf("Vector %f,%f,%f \n", Le.x, Le.y, Le.z);
		Color fr = color * (1 / M_PI);
		temp = (fr.mult(Le) * costheta) * (1 / PFuenteDeArea(costhetaO, distancia)); //) * (1 / PFuenteDeArea(costheta, distancia));(4 * M_PI)
		return temp;
	}
}
// Calcula el valor de color para el rayo dado
Color shade(const Ray &r)
{
	//printf("Vamos Bien Shade\n");
	double t = 0;
	int id = 0;
	// determinar que esfera (id) y a que distancia (t) el rayo intersecta
	if (!intersect(r, t, id))
	{
		return Color(); // el rayo no intersecto objeto, return Vector() == negro
	}
	const Sphere &obj = spheres[id];
	// PROYECTO 1
	// determinar coordenadas del punto de interseccion
	Point x = r.o + r.d * t;
	Vector normal = (x - obj.p);
	//printf("%f\n", sqrt(normal.length_squared()));
	normal.normalize();

	if (obj.ke.x > 1e-8 && obj.ke.y > 1e-8 && obj.ke.z > 1e-8)
	{
		return obj.ke;
	}
	else
	{
		Color radiancia = EstimadorMonteCarlo(obj.c, normal, x, id);
		return radiancia;
	}
}

int main(int argc, char *argv[])
{
	int muestras = atof(argv[1]);
	int w = 1024, h = 768; // image resolution
	srand(time(NULL));
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
		//fprintf(stderr, "\r%5.2f%%", 100. * y / (h - 1));
		for (int x = 0; x < w; x++)
		{

			int idx = (h - y - 1) * w + x; // index en 1D para una imagen 2D x,y son invertidos
			Color pixelValue = Color();	   // pixelValue en negro por ahora
			for (int i = 0; i < muestras; i++)
			{
				// para el pixel actual, computar la dirección que un rayo debe tener
				Vector cameraRayDir = cx * (double(x) / w - .5) + cy * (double(y) / h - .5) + camera.d;

				// computar el color del pixel para el punto que intersectó el rayo desde la camara
				pixelValue = pixelValue + shade(Ray(camera.o, cameraRayDir.normalize()));
			}

			pixelValue = pixelValue * (1.0 / muestras);
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

	return 0;
}
