// rt: un lanzador de rayos minimalista
// g++ -O3 -fopenmp rt.cpp -o rt
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <omp.h>

const double PI = 3.14159265358979323846;

class Vector
{
public:
	double x, y, z; // coordenadas x,y,z

	// Constructor del vector, parametros por default en cero
	Vector(double x_= 0, double y_= 0, double z_= 0){ x=x_; y=y_; z=z_; }

	// operador para suma y resta de vectores
	Vector operator+(const Vector &b) const { return Vector(x + b.x, y + b.y, z + b.z); }
	Vector operator-(const Vector &b) const { return Vector(x - b.x, y - b.y, z - b.z); }
	// operator multiplicacion vector y escalar
	Vector operator*(double b) const { return Vector(x * b, y * b, z * b); }

	// operator % para producto cruz
	Vector operator%(Vector&b){return Vector(y * b.z - z * b.y, z * b.x - x * b.z, x * b.y - y * b.x);}

	// producto punto con vector b
	double dot(const Vector &b) const { return x * b.x + y * b.y + z * b.z; }

	// producto elemento a elemento (Hadamard product)
	Vector mult(const Vector &b) const { return Vector(x * b.x, y * b.y, z * b.z); }

	// normalizar vector
	Vector& normalize(){ return *this = *this * (1.0 / sqrt(x * x + y * y + z * z)); }
};
typedef Vector Point;
typedef Vector Color;

// producto cruz que acepta argumentos const (operator% exige referencias no-const)
inline Vector cross(const Vector &a, const Vector &b) {
	return Vector(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

class Ray
{
public:
	Point o;
	Vector d; // origen y direcccion del rayo
	Ray(Point o_, Vector d_) : o(o_), d(d_) {} // constructor
};

class Sphere
{
public:
	double r;	// radio de la esfera
	Point p;	// posicion
	Color c;	// albedo (color difuso)
	Color ke;	// emision (radiancia propia; (0,0,0) si no emite)

	Sphere(double r_, Point p_, Color c_, Color ke_): r(r_), p(p_), c(c_), ke(ke_) {}

	// determina si el rayo intersecta a esta esfera
	double intersect(const Ray &ray) const {
		// regresar distancia si hay intersección
		// regresar 0.0 si no hay interseccion
		Vector oc = ray.o - p;
		double b = oc.dot(ray.d);
		double det = b * b - (oc.dot(oc) - r * r);
		if (det < 0)
			return 0.0;
		double sq = sqrt(det);
		double t1 = -b - sq;
		if (t1 > 1e-4)
			return t1;
		double t2 = -b + sq;
		if (t2 > 1e-4)
			return t2;
		return 0.0;
	}
};

Sphere spheres[] = {
	//Escena: radio, posicion, albedo, emision
	Sphere(1e5,  Point(-1e5 - 49, 0, 0),   Color(.75, .25, .25), Color()),          // pared izq
	Sphere(1e5,  Point(1e5 + 49, 0, 0),    Color(.25, .25, .75), Color()),          // pared der
	Sphere(1e5,  Point(0, 0, -1e5 - 81.6), Color(.25, .75, .25), Color()),          // pared detras
	Sphere(1e5,  Point(0, -1e5 - 40.8, 0), Color(.25, .75, .75), Color()),          // suelo
	Sphere(1e5,  Point(0, 1e5 + 40.8, 0),  Color(.75, .75, .25), Color()),          // techo
	Sphere(16.5, Point(-23, -24.3, -34.6), Color(.2, .3, .4),    Color()),          // esfera abajo-izq
	Sphere(16.5, Point(23, -24.3, -3.6),   Color(.4, .3, .2),    Color()),          // esfera abajo-der
	Sphere(10.5, Point(0, 24.3, 0),        Color(1, 1, 1),       Color(10, 10, 10)) // esfera arriba (emisor)
};

// limita el valor de x a [0,1]
inline double clamp(const double x) {
	if(x < 0.0)
		return 0.0;
	else if(x > 1.0)
		return 1.0;
	return x;
}

// convierte un valor de color en [0,1] a un entero en [0,255]
inline int toDisplayValue(const double x) {
	return int( pow( clamp(x), 1.0/2.2 ) * 255 + .5);
}

// calcular la intersección del rayo r con todas las esferas
// regresar true si hubo una intersección, falso de otro modo
// almacenar en t la distancia sobre el rayo en que sucede la interseccion
// almacenar en id el indice de spheres[] de la esfera cuya interseccion es mas cercana
inline bool intersect(const Ray &r, double &t, int &id) {
	int n = sizeof(spheres) / sizeof(Sphere);
	t = 1e20;
	bool hitAny = false;
	for (int i = 0; i < n; i++) {
		double d = spheres[i].intersect(r);
		if (d > 0.0 && d < t) {
			t = d;
			id = i;
			hitAny = true;
		}
	}
	return hitAny;
}

inline bool emits(const Color &ke) { return ke.x != 0.0 || ke.y != 0.0 || ke.z != 0.0; }

// muestreador de direcciones seleccionado en main() vía argv
enum Sampler { UNIFORM_SPHERE, UNIFORM_HEMI, COSINE_HEMI };
Sampler g_sampler = COSINE_HEMI;

// Muestrea una dirección wi alrededor de la normal n según g_sampler (xi1,xi2 ~ U[0,1)).
// Regresa wi (unitario) y almacena en pdf la densidad de esa dirección.
Vector sampleDir(const Vector &n, double xi1, double xi2, double &pdf) {
	double cosTheta;
	switch (g_sampler) {
	case UNIFORM_SPHERE:
		cosTheta = 1.0 - 2.0 * xi1;
		pdf = 1.0 / (4.0 * PI);
		break;
	case UNIFORM_HEMI:
		cosTheta = xi1;
		pdf = 1.0 / (2.0 * PI);
		break;
	default: // COSINE_HEMI
		cosTheta = sqrt(1.0 - xi1);
		pdf = cosTheta / PI;
		break;
	}
	double sinTheta = sqrt(1.0 - cosTheta * cosTheta);
	double phi = 2.0 * PI * xi2;
	double wx = sinTheta * cos(phi), wy = sinTheta * sin(phi), wz = cosTheta;

	// base ortonormal local sobre n
	Vector t;
	if (fabs(n.x) > fabs(n.y))
		t = Vector(n.z, 0.0, -n.x) * (1.0 / sqrt(n.x * n.x + n.z * n.z));
	else
		t = Vector(0.0, n.z, -n.y) * (1.0 / sqrt(n.y * n.y + n.z * n.z));
	Vector s = cross(t, n);

	return s * wx + t * wy + n * wz;
}

// Calcula UNA muestra del color para el rayo dado: estimador Monte Carlo de
// iluminación directa con emisores esféricos y materiales difusos.
// Xi es el estado erand48 del pixel; avanza con cada llamada.
Color shade(const Ray &r, unsigned short Xi[3]) {
	double t;
	int id = 0;
	if (!intersect(r, t, id))
		return Color(); // el rayo no intersecto objeto: negro

	const Sphere &obj = spheres[id];
	if (emits(obj.ke))
		return obj.ke; // el rayo toca un emisor: su radiancia (no se pinta negro)

	Point x = r.o + r.d * t;
	Vector n = (x - obj.p).normalize();

	double xi1 = erand48(Xi), xi2 = erand48(Xi);
	double pdf;
	Vector wi = sampleDir(n, xi1, xi2, pdf);
	double cosTheta = n.dot(wi);

	// visibilidad implícita: el epsilon 1e-4 de Sphere::intersect evita
	// autointersección en x; Le es la emisión del objeto más cercano en (x, wi)
	double t2;
	int id2 = 0;
	Color Le = intersect(Ray(x, wi), t2, id2) ? spheres[id2].ke : Color();

	Color fr = obj.c * (1.0 / PI);
	return fr.mult(Le) * (cosTheta / pdf);
}

int main(int argc, char *argv[]) {
	int w = 1024, h = 768; // image resolution

	// selecciona el muestreador y las muestras por pixel por argumentos de línea de comandos
	if (argc > 1) {
		if (strcmp(argv[1], "uniformsphere") == 0) g_sampler = UNIFORM_SPHERE;
		else if (strcmp(argv[1], "uniformhemi") == 0) g_sampler = UNIFORM_HEMI;
		else g_sampler = COSINE_HEMI;
	}
	int spp = (argc > 2) ? atoi(argv[2]) : 32;
	if (spp < 1) spp = 1;

	// fija la posicion de la camara y la dirección en que mira
	Ray camera( Point(0, 11.2, 214), Vector(0, -0.042612, -1).normalize() );

	// parametros de la camara
	Vector cx = Vector( w * 0.5095 / h, 0., 0.);
	Vector cy = (cx % camera.d).normalize() * 0.5095;

	// auxiliar para valor de pixel y matriz para almacenar la imagen
	Color *pixelColors = new Color[w * h];

	// cada hilo computa un renglon (ciclo interior); el RNG por pixel hace la imagen determinista
	#pragma omp parallel for schedule(dynamic, 1)
	for(int y = 0; y < h; y++)
	{
		fprintf(stderr,"\r%5.2f%%",100.*y/(h-1));
		for(int x = 0; x < w; x++ ) {
			int idx = (h - y - 1) * w + x; // index en 1D para una imagen 2D x,y son invertidos

			// estado erand48 sembrado de forma determinista por indice de pixel (x,y):
			// misma imagen con 1 o N hilos, sin rand()/srand() global ni dependencia del hilo
			unsigned int seedIdx = (unsigned int)y * w + x;
			unsigned int mixed = seedIdx * 2654435761u; // hash multiplicativo: decorrelaciona pixeles vecinos
			unsigned short Xi[3] = {
				(unsigned short)(mixed & 0xFFFF),
				(unsigned short)((mixed >> 16) & 0xFFFF),
				(unsigned short)((seedIdx >> 8) ^ 0x330E)
			};

			// para el pixel actual, el rayo de camara es identico en las N muestras (sin jitter)
			Vector cameraRayDir = cx * ( double(x)/w - .5) + cy * ( double(y)/h - .5) + camera.d;
			Ray cameraRay( camera.o, cameraRayDir.normalize() );

			Color sum;
			for (int s = 0; s < spp; s++)
				sum = sum + shade(cameraRay, Xi);
			Color pixelValue = sum * (1.0 / spp);

			// limitar los tres valores de color del pixel a [0,1] despues de promediar
			pixelColors[idx] = Color(clamp(pixelValue.x), clamp(pixelValue.y), clamp(pixelValue.z));
		}
	}

	fprintf(stderr,"\n");

	FILE *f = fopen("image.ppm", "w");
	// escribe cabecera del archivo ppm, ancho, alto y valor maximo de color
	fprintf(f, "P3\n%d %d\n%d\n", w, h, 255);
	for (int p = 0; p < w * h; p++)
	{ // escribe todos los valores de los pixeles
    		fprintf(f,"%d %d %d ", toDisplayValue(pixelColors[p].x), toDisplayValue(pixelColors[p].y),
				toDisplayValue(pixelColors[p].z));
  	}
  	fclose(f);

  	delete[] pixelColors;

	return 0;
}
