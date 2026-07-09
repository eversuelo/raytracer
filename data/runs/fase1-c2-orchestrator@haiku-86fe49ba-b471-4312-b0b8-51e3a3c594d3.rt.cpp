// rt: un lanzador de rayos minimalista
// g++ -O3 -fopenmp rt.cpp -o rt
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <omp.h>
#include <string.h>

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
	Vector operator%(const Vector&b) const {return Vector(y * b.z - z * b.y, z * b.x - x * b.z, x * b.y - y * b.x);}
	
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
	Color ke;	// emisión

	Sphere(double r_, Point p_, Color c_, Color ke_ = Color()): r(r_), p(p_), c(c_), ke(ke_) {}
  
	// PROYECTO 1
	// determina si el rayo intersecta a esta esfera
	double intersect(const Ray &ray) const {
		Vector oc = ray.o - p;
		double b = oc.dot(ray.d);
		double det = b * b - oc.dot(oc) + r * r;
		if (det < 0.0) return 0.0;
		double sqrt_det = sqrt(det);
		double t1 = -b - sqrt_det;
		double t2 = -b + sqrt_det;
		if (t1 > 1e-4) return t1;
		if (t2 > 1e-4) return t2;
		return 0.0;
	}
};

Sphere spheres[] = {
	Sphere(1e5,  Point(-1e5 - 49, 0, 0),   Color(.75, .25, .25), Color(0, 0, 0)), // pared izq
	Sphere(1e5,  Point(1e5 + 49, 0, 0),    Color(.25, .25, .75), Color(0, 0, 0)), // pared der
	Sphere(1e5,  Point(0, 0, -1e5 - 81.6), Color(.25, .75, .25), Color(0, 0, 0)), // pared detras
	Sphere(1e5,  Point(0, -1e5 - 40.8, 0), Color(.25, .75, .75), Color(0, 0, 0)), // suelo
	Sphere(1e5,  Point(0, 1e5 + 40.8, 0),  Color(.75, .75, .25), Color(0, 0, 0)), // techo
	Sphere(16.5, Point(-23, -24.3, -34.6), Color(.2, .3, .4), Color(0, 0, 0)), // esfera abajo-izq
	Sphere(16.5, Point(23, -24.3, -3.6),   Color(.4, .3, .2), Color(0, 0, 0)), // esfera abajo-der
	Sphere(10.5, Point(0, 24.3, 0),        Color(1, 1, 1), Color(10, 10, 10)) // emisor arriba
};

const char* renderMode = "normal";

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
	t = 1e20;
	bool found = false;
	for (int i = 0; i < 8; i++) {
		double hit = spheres[i].intersect(r);
		if (hit > 0.0 && hit < t) {
			t = hit;
			id = i;
			found = true;
		}
	}
	return found;
}

// Muestreadores de direcciones
Vector uniformsphere_sample(double xi1, double xi2, const Vector &n) {
	double costheta = 1.0 - 2.0 * xi1;
	double sintheta = sqrt(fmax(0.0, 1.0 - costheta * costheta));
	double phi = 2.0 * M_PI * xi2;

	Vector w_local(sintheta * cos(phi), sintheta * sin(phi), costheta);

	Vector t, s;
	if (fabs(n.x) > fabs(n.y)) {
		t = Vector(n.z, 0, -n.x);
		t.normalize();
	} else {
		t = Vector(0, n.z, -n.y);
		t.normalize();
	}
	s = t % n;

	return s * w_local.x + t * w_local.y + n * w_local.z;
}

double uniformsphere_pdf(double costheta) {
	return 1.0 / (4.0 * M_PI);
}

Vector uniformhemi_sample(double xi1, double xi2, const Vector &n) {
	double costheta = xi1;
	double sintheta = sqrt(fmax(0.0, 1.0 - costheta * costheta));
	double phi = 2.0 * M_PI * xi2;

	Vector w_local(sintheta * cos(phi), sintheta * sin(phi), costheta);

	Vector t, s;
	if (fabs(n.x) > fabs(n.y)) {
		t = Vector(n.z, 0, -n.x);
		t.normalize();
	} else {
		t = Vector(0, n.z, -n.y);
		t.normalize();
	}
	s = t % n;

	return s * w_local.x + t * w_local.y + n * w_local.z;
}

double uniformhemi_pdf(double costheta) {
	return 1.0 / (2.0 * M_PI);
}

Vector cosinehemi_sample(double xi1, double xi2, const Vector &n) {
	double costheta = sqrt(fmax(0.0, 1.0 - xi1));
	double sintheta = sqrt(fmax(0.0, xi1));
	double phi = 2.0 * M_PI * xi2;

	Vector w_local(sintheta * cos(phi), sintheta * sin(phi), costheta);

	Vector t, s;
	if (fabs(n.x) > fabs(n.y)) {
		t = Vector(n.z, 0, -n.x);
		t.normalize();
	} else {
		t = Vector(0, n.z, -n.y);
		t.normalize();
	}
	s = t % n;

	return s * w_local.x + t * w_local.y + n * w_local.z;
}

double cosinehemi_pdf(double costheta) {
	return costheta / M_PI;
}

struct Sampler {
	const char *name;
	Vector (*sample)(double xi1, double xi2, const Vector &n);
	double (*pdf)(double costheta);
};

Sampler samplers[] = {
	{"uniformsphere", uniformsphere_sample, uniformsphere_pdf},
	{"uniformhemi", uniformhemi_sample, uniformhemi_pdf},
	{"cosinehemi", cosinehemi_sample, cosinehemi_pdf}
};

int sampler_id = 2; // cosinehemi por defecto

Color shade(const Ray &r, unsigned short xi[3], int samp_id) {
	double t;
	int id = 0;
	if (!intersect(r, t, id))
		return Color();

	const Sphere &obj = spheres[id];
	Point x = r.o + r.d * t;
	Vector n = (x - obj.p);
	n.normalize();

	// Si el objeto emite, devolver su emisión
	if (obj.ke.x > 0 || obj.ke.y > 0 || obj.ke.z > 0) {
		return obj.ke;
	}

	// Generar UNA muestra del estimador Monte Carlo de iluminación directa
	double xi1 = erand48(xi);
	double xi2 = erand48(xi);
	Vector wi = samplers[samp_id].sample(xi1, xi2, n);

	// Lanzar rayo con origen x + n*epsilon
	Ray scatter(x + n * 1e-4, wi);
	double t2;
	int id2 = 0;
	if (!intersect(scatter, t2, id2))
		return Color();

	const Sphere &emitter = spheres[id2];
	// Si el rayo de muestreo no toca un emisor, retorna negro
	if (!(emitter.ke.x > 0 || emitter.ke.y > 0 || emitter.ke.z > 0))
		return Color();

	// Coseno entre normal y dirección muestreada
	double costheta = n.dot(wi);
	if (costheta <= 0.0)
		return Color();

	// Estimador: L = Le * fr * costheta / pdf
	// fr = albedo / pi (material difuso)
	double pdf = samplers[samp_id].pdf(costheta);
	Vector fr = obj.c * (1.0 / M_PI);
	Color Le = emitter.ke;
	Color contrib = Le.mult(fr) * (costheta / pdf);

	return contrib;
}


int main(int argc, char *argv[]) {
	int w = 1024, h = 768;
	Ray camera( Point(0, 11.2, 214), Vector(0, -0.042612, -1).normalize() );
	Vector cx = Vector( w * 0.5095 / h, 0., 0.);
	Vector cy = (cx % camera.d).normalize() * 0.5095;

	// Parsear argumentos: sampler y spp
	int spp = 32;
	sampler_id = 2; // cosinehemi por defecto
	const char *sampler_name = "cosinehemi";

	if (argc > 1) {
		for (int i = 0; i < 3; i++) {
			if (strcmp(argv[1], samplers[i].name) == 0) {
				sampler_id = i;
				sampler_name = samplers[i].name;
				break;
			}
		}
	}

	if (argc > 2) {
		spp = atoi(argv[2]);
	}

	Color *pixelColors = new Color[w * h];

	#pragma omp parallel for schedule(dynamic, 1)
	for(int y = 0; y < h; y++)
	{
		fprintf(stderr,"\r%5.2f%%",100.*y/(h-1));
		for(int x = 0; x < w; x++) {
			int idx = (h - y - 1) * w + x;
			Color pixelValue = Color();

			// Semilla determinista por píxel
			unsigned int seed = y * w + x;
			unsigned short xi[3];
			xi[0] = (seed) & 0xffff;
			xi[1] = (seed >> 16) & 0xffff;
			xi[2] = 1;

			Vector cameraRayDir = cx * ( double(x)/w - .5) + cy * ( double(y)/h - .5) + camera.d;
			Ray camRay(camera.o, cameraRayDir.normalize());

			// Promediar spp muestras
			for (int s = 0; s < spp; s++) {
				Color sample = shade(camRay, xi, sampler_id);
				pixelValue = pixelValue + sample;
			}

			pixelValue = pixelValue * (1.0 / spp);
			pixelColors[idx] = Color(clamp(pixelValue.x), clamp(pixelValue.y), clamp(pixelValue.z));
		}
	}

	fprintf(stderr,"\n");

	FILE *f = fopen("image.ppm", "w");
	fprintf(f, "P3\n%d %d\n%d\n", w, h, 255);
	for (int p = 0; p < w * h; p++) {
		fprintf(f,"%d %d %d ", toDisplayValue(pixelColors[p].x), toDisplayValue(pixelColors[p].y),
				toDisplayValue(pixelColors[p].z));
	}
	fclose(f);

	delete[] pixelColors;

	return 0;
}
