// rt: un lanzador de rayos minimalista
 // g++ -O3 -fopenmp rt.cpp -o rt
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <omp.h>

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
	Color ke;	// emision

	Sphere(double r_, Point p_, Color c_, Color ke_): r(r_), p(p_), c(c_), ke(ke_) {}

	// PROYECTO 1
	// determina si el rayo intersecta a esta esfera
	double intersect(const Ray &ray) const {
		// regresar distancia si hay intersección
		// regresar 0.0 si no hay interseccion
		Vector oc = ray.o - p;
		double b = oc.dot(ray.d);
		double det = b * b - (oc.dot(oc) - r * r);
		if (det < 0.0)
			return 0.0;
		double sqrtDet = sqrt(det);
		double t1 = -b - sqrtDet;
		if (t1 > 1e-4)
			return t1;
		double t2 = -b + sqrtDet;
		if (t2 > 1e-4)
			return t2;
		return 0.0;
	}
};

// PROYECTO 2 (parte 1): escena con emisores esfericos (ke != 0 => emite luz)
Sphere spheres[] = {
	//Escena: radio, posicion, albedo, emision
	Sphere(1e5,  Point(-1e5 - 49, 0, 0),   Color(.75, .25, .25), Color()),        // pared izq
	Sphere(1e5,  Point(1e5 + 49, 0, 0),    Color(.25, .25, .75), Color()),        // pared der
	Sphere(1e5,  Point(0, 0, -1e5 - 81.6), Color(.25, .75, .25), Color()),        // pared detras
	Sphere(1e5,  Point(0, -1e5 - 40.8, 0), Color(.25, .75, .75), Color()),        // suelo
	Sphere(1e5,  Point(0, 1e5 + 40.8, 0),  Color(.75, .75, .25), Color()),        // techo
	Sphere(16.5, Point(-23, -24.3, -34.6), Color(.2, .3, .4),    Color()),        // esfera abajo-izq
	Sphere(16.5, Point(23, -24.3, -3.6),   Color(.4, .3, .2),    Color()),        // esfera abajo-der
	Sphere(10.5, Point(0, 24.3, 0),        Color(1, 1, 1),       Color(10,10,10)) // esfera arriba (emisor, solo modos MC)
};

// numero de esferas activas para intersect(): todas (modos MC) o todas menos el
// emisor esferico (modo "point", ver g_numSpheres en main)
int g_numSpheres = sizeof(spheres) / sizeof(Sphere);

// PROYECTO 2 (parte 2): fuente puntual, ADR-0005 — se representa como struct
// aparte (no como esfera de radio 0) para que ningun rayo pueda intersectarla;
// la esfera emisora de la phase-01 se excluye del arreglo activo en modo "point"
// (ver g_numSpheres) para que no se dibuje ni se autoilumine.
struct PointLight {
	Point p;   // posicion
	Color I;   // intensidad radiante
};
PointLight g_pointLight = { Point(0, 24.3, 0), Color(4000, 4000, 4000) };

const double PI = 3.14159265358979323846;

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

// PROYECTO 1
// calcular la intersección del rayo r con todas las esferas
// regresar true si hubo una intersección, falso de otro modo
// almacenar en t la distancia sobre el rayo en que sucede la interseccion
// almacenar en id el indice de spheres[] de la esfera cuya interseccion es mas cercana
inline bool intersect(const Ray &r, double &t, int &id) {
	int n = g_numSpheres;
	double inf = 1e20;
	t = inf;
	for (int i = 0; i < n; i++) {
		double d = spheres[i].intersect(r);
		if (d > 0.0 && d < t) {
			t = d;
			id = i;
		}
	}
	return t < inf;
}

inline bool isEmitter(const Color &ke) {
	return ke.x > 0.0 || ke.y > 0.0 || ke.z > 0.0;
}

// Muestreadores de direccion, seleccionados por argumento de linea de comandos
enum Sampler { UNIFORM_SPHERE, UNIFORM_HEMI, COSINE_HEMI };
Sampler g_sampler = COSINE_HEMI;

// muestrea una direccion wi alrededor de la normal n segun g_sampler,
// regresa la direccion en coordenadas globales y su pdf en *pdf
Vector sampleDirection(Vector n, unsigned short *Xi, double *pdf) {
	double xi1 = erand48(Xi);
	double xi2 = erand48(Xi);
	double phi = 2.0 * PI * xi2;

	double cosTheta;
	switch (g_sampler) {
	case UNIFORM_SPHERE:
		cosTheta = 1.0 - 2.0 * xi1;
		*pdf = 1.0 / (4.0 * PI);
		break;
	case UNIFORM_HEMI:
		cosTheta = xi1;
		*pdf = 1.0 / (2.0 * PI);
		break;
	default: // COSINE_HEMI
		cosTheta = sqrt(1.0 - xi1);
		*pdf = cosTheta / PI;
		break;
	}
	double sinTheta = sqrt(fmax(0.0, 1.0 - cosTheta * cosTheta));
	double wx = sinTheta * cos(phi);
	double wy = sinTheta * sin(phi);
	double wz = cosTheta;

	// base ortonormal (s,t,n) sobre n
	Vector t;
	if (fabs(n.x) > fabs(n.y))
		t = Vector(n.z, 0, -n.x) * (1.0 / sqrt(n.x * n.x + n.z * n.z));
	else
		t = Vector(0, n.z, -n.y) * (1.0 / sqrt(n.y * n.y + n.z * n.z));
	Vector s = t % n;

	return s * wx + t * wy + n * wz;
}

// PROYECTO 2 (parte 1)
// estima la ecuacion de renderizado de iluminacion directa por Monte Carlo
Color shade(const Ray &r, unsigned short *Xi) {
	double t;
	int id = 0;
	// determinar que esfera (id) y a que distancia (t) el rayo intersecta
	if (!intersect(r, t, id))
		return Color(); // el rayo no intersecto objeto, negro

	const Sphere &obj = spheres[id];

	// si el objeto alcanzado por el rayo de camara emite, se regresa su emision
	if (isEmitter(obj.ke))
		return obj.ke;

	// punto y normal de interseccion
	Point x = r.o + r.d * t;
	Vector n = (x - obj.p).normalize();

	// UNA muestra del estimador L = fr * Le * (n.wi) / pdf(wi)
	double pdf;
	Vector wi = sampleDirection(n, Xi, &pdf);

	double cosTheta = n.dot(wi);
	if (cosTheta <= 0.0 || pdf <= 0.0)
		return Color();

	double tHit;
	int idHit = 0;
	// la visibilidad va implicita en este rayo (epsilon 1e-4 de Sphere::intersect)
	if (!intersect(Ray(x, wi), tHit, idHit))
		return Color();

	const Sphere &hit = spheres[idHit];
	if (!isEmitter(hit.ke))
		return Color();

	Color fr = obj.c * (1.0 / PI);
	return hit.ke.mult(fr) * (cosTheta / pdf);
}

// PROYECTO 2 (parte 2)
// shading determinista con fuente puntual: 1 rayo por pixel, sin Monte Carlo,
// sin jitter; L = (albedo/PI) . I * cosTheta / r^2, con sombras duras
Color shadePoint(const Ray &r) {
	double t;
	int id = 0;
	if (!intersect(r, t, id))
		return Color();

	const Sphere &obj = spheres[id];

	// punto y normal de interseccion
	Point x = r.o + r.d * t;
	Vector n = (x - obj.p).normalize();

	Vector toLight = g_pointLight.p - x;
	double r2 = toLight.dot(toLight);
	double dist = sqrt(r2);
	Vector w = toLight * (1.0 / dist);

	double cosTheta = n.dot(w);
	if (cosTheta <= 0.0)
		return Color();

	// rayo de sombra: si algo bloquea antes de llegar a la fuente (con el
	// epsilon de la fase 0), la contribucion es cero (sombra dura)
	double tHit;
	int idHit = 0;
	if (intersect(Ray(x, w), tHit, idHit) && tHit < dist - 1e-4)
		return Color();

	Color fr = obj.c * (1.0 / PI);
	return fr.mult(g_pointLight.I) * (cosTheta / r2);
}


int main(int argc, char *argv[]) {
	int w = 1024, h = 768; // image resolution

	if (argc < 2) {
		fprintf(stderr, "uso: %s <uniformsphere|uniformhemi|cosinehemi> <spp>\n", argv[0]);
		fprintf(stderr, "     %s point\n", argv[0]);
		return 1;
	}

	// PROYECTO 2 (parte 2): modo "point" — fuente puntual, determinista, 1 rayo
	// por pixel (el spp de los modos MC no aplica); excluye del arreglo activo
	// la esfera emisora de la phase-01 (ultimo elemento) para que no se dibuje
	bool pointMode = strcmp(argv[1], "point") == 0;

	int spp = 1;
	if (pointMode) {
		g_numSpheres = sizeof(spheres) / sizeof(Sphere) - 1;
	} else {
		if (argc < 3) {
			fprintf(stderr, "uso: %s <uniformsphere|uniformhemi|cosinehemi> <spp>\n", argv[0]);
			return 1;
		}
		if (strcmp(argv[1], "uniformsphere") == 0)
			g_sampler = UNIFORM_SPHERE;
		else if (strcmp(argv[1], "uniformhemi") == 0)
			g_sampler = UNIFORM_HEMI;
		else if (strcmp(argv[1], "cosinehemi") == 0)
			g_sampler = COSINE_HEMI;
		else {
			fprintf(stderr, "sampler desconocido: %s\n", argv[1]);
			return 1;
		}

		spp = atoi(argv[2]);
		if (spp < 1)
			spp = 1;
	}

	// fija la posicion de la camara y la dirección en que mira
	Ray camera( Point(0, 11.2, 214), Vector(0, -0.042612, -1).normalize() );

	// parametros de la camara
	Vector cx = Vector( w * 0.5095 / h, 0., 0.);
	Vector cy = (cx % camera.d).normalize() * 0.5095;

	// auxiliar para valor de pixel y matriz para almacenar la imagen
	Color *pixelColors = new Color[w * h];

	// PROYECTO 1
	// usar openmp para paralelizar el ciclo: cada hilo computara un renglon (ciclo interior),
	#pragma omp parallel for schedule(dynamic, 1)
	for(int y = 0; y < h; y++)
	{
		// recorre todos los pixeles de la imagen
		fprintf(stderr,"\r%5.2f%%",100.*y/(h-1));
		for(int x = 0; x < w; x++ ) {
			int idx = (h - y - 1) * w + x; // index en 1D para una imagen 2D x,y son invertidos

			// RNG por pixel, sembrado deterministamente con (x,y): independiente
			// del numero de hilos y del orden de ejecucion de OpenMP
			unsigned short Xi[3] = { (unsigned short)y, (unsigned short)x, (unsigned short)idx };

			// para el pixel actual, computar la dirección que un rayo debe tener
			// (identica en las N muestras: no se agrega jitter de pixel)
			Vector cameraRayDir = cx * ( double(x)/w - .5) + cy * ( double(y)/h - .5) + camera.d;
			Ray cameraRay( camera.o, cameraRayDir.normalize() );

			// promediar N muestras por pixel y clampear despues de promediar
			// (en modo "point" no hay Monte Carlo: una unica muestra determinista)
			Color pixelValue;
			if (pointMode) {
				pixelValue = shadePoint(cameraRay);
			} else {
				Color accum;
				for (int s = 0; s < spp; s++)
					accum = accum + shade(cameraRay, Xi);
				pixelValue = accum * (1.0 / spp);
			}

			// limitar los tres valores de color del pixel a [0,1]
			pixelColors[idx] = Color(clamp(pixelValue.x), clamp(pixelValue.y), clamp(pixelValue.z));
		}
	}

	fprintf(stderr,"\n");

	// PROYECTO 1
	// Investigar formato ppm
	FILE *f = fopen(pointMode ? "image-plight.ppm" : "image.ppm", "w");
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
