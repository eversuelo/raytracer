// rt: un lanzador de rayos minimalista
 // g++ -O3 -fopenmp rt.cpp -o rt
#include <math.h>
#include <stdlib.h>
#include <stdio.h>  
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
	Color c;	// color (albedo)
	Color ke;	// emisión

	Sphere(double r_, Point p_, Color c_, Color ke_ = Color()): r(r_), p(p_), c(c_), ke(ke_) {}

	// PROYECTO 1
	// determina si el rayo intersecta a esta esfera
	double intersect(const Ray &ray) const {
		// regresar distancia si hay intersección
		// regresar 0.0 si no hay interseccion
		Vector oc = ray.o - p;
		double b = oc.dot(ray.d);
		double det = b * b - (oc.dot(oc) - r * r);
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
	// Escena: radio, posicion, albedo, emisión
	Sphere(1e5,  Point(-1e5 - 49, 0, 0),   Color(.75, .25, .25), Color(0, 0, 0)), // pared izq
	Sphere(1e5,  Point(1e5 + 49, 0, 0),    Color(.25, .25, .75), Color(0, 0, 0)), // pared der
	Sphere(1e5,  Point(0, 0, -1e5 - 81.6), Color(.25, .75, .25), Color(0, 0, 0)), // pared detras
	Sphere(1e5,  Point(0, -1e5 - 40.8, 0), Color(.25, .75, .75), Color(0, 0, 0)), // suelo
	Sphere(1e5,  Point(0, 1e5 + 40.8, 0),  Color(.75, .75, .25), Color(0, 0, 0)), // techo
	Sphere(16.5, Point(-23, -24.3, -34.6), Color(.2, .3, .4), Color(0, 0, 0)), // esfera abajo-izq
	Sphere(16.5, Point(23, -24.3, -3.6),   Color(.4, .3, .2), Color(0, 0, 0)), // esfera abajo-der
	Sphere(10.5, Point(0, 24.3, 0),        Color(1, 1, 1), Color(10, 10, 10)) // emisor
};

// sampler seleccionado: "uniformsphere", "uniformhemi", "cosinehemi"
const char *sampler = "cosinehemi";

// limita el valor de x a [0,1]
inline double clamp(const double x) {
	if(x < 0.0)
		return 0.0;
	else if(x > 1.0)
		return 1.0;
	return x;
}

// RNG: erand48 con semilla por píxel
inline double rng(unsigned short *seed) {
	return erand48(seed);
}

// Construye base ortonormal sobre el normal n
// Retorna t (tangente) en el resultado; s y n se construyen para completar la base
void localBasis(const Vector &n, Vector &s, Vector &t) {
	if (fabs(n.x) > fabs(n.y)) {
		t = Vector(n.z, 0.0, -n.x);
		double len = sqrt(t.x * t.x + t.z * t.z);
		t = t * (1.0 / len);
	} else {
		t = Vector(0.0, n.z, -n.y);
		double len = sqrt(t.y * t.y + t.z * t.z);
		t = t * (1.0 / len);
	}
	s = t % n;
}

// Muestrea una dirección según el sampler seleccionado
// Retorna (dirección, pdf)
struct Sample {
	Vector d;
	double pdf;
};

Sample sampleDirection(const Vector &n, unsigned short *seed) {
	double xi1 = rng(seed);
	double xi2 = rng(seed);

	Vector s, t;
	localBasis(n, s, t);

	// Identificar sampler por nombre
	bool is_uniform_sphere = (sampler[7] == 's');  // uniformsphere[7]='s'
	bool is_uniform_hemi = (sampler[0] == 'u' && sampler[7] == 'h');  // uniformhemi[7]='h'
	bool is_cosine_hemi = (sampler[0] == 'c');  // cosinehemi[0]='c'

	if (is_uniform_sphere) {  // uniformsphere
		double costheta = 1.0 - 2.0 * xi1;
		double sintheta = sqrt(fmax(0.0, 1.0 - costheta * costheta));
		double phi = 2.0 * M_PI * xi2;
		Vector local(sintheta * cos(phi), sintheta * sin(phi), costheta);
		Vector d = s * local.x + t * local.y + n * local.z;
		return {d.normalize(), 1.0 / (4.0 * M_PI)};
	} else if (is_uniform_hemi) {  // uniformhemi
		double costheta = xi1;
		double sintheta = sqrt(fmax(0.0, 1.0 - costheta * costheta));
		double phi = 2.0 * M_PI * xi2;
		Vector local(sintheta * cos(phi), sintheta * sin(phi), costheta);
		Vector d = s * local.x + t * local.y + n * local.z;
		return {d.normalize(), 1.0 / (2.0 * M_PI)};
	} else {  // cosinehemi
		double costheta = sqrt(fmax(0.0, 1.0 - xi1));
		double sintheta = sqrt(fmax(0.0, 1.0 - costheta * costheta));
		double phi = 2.0 * M_PI * xi2;
		Vector local(sintheta * cos(phi), sintheta * sin(phi), costheta);
		Vector d = s * local.x + t * local.y + n * local.z;
		return {d.normalize(), costheta / M_PI};
	}
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
		double dist = spheres[i].intersect(r);
		if (dist > 0.0 && dist < t) {
			t = dist;
			id = i;
			found = true;
		}
	}
	return found;
}

// Calcula una muestra del estimador MC de iluminación directa
Color shade(const Ray &r, unsigned short *seed) {
	double t;
	int id = 0;
	// determinar que esfera (id) y a que distancia (t) el rayo intersecta
	if (!intersect(r, t, id))
		return Color();	// el rayo no intersecto objeto, return Vector() == negro

	const Sphere &obj = spheres[id];

	// Si el objeto intersectado emite, devolver su emisión
	if (obj.ke.x > 0.0 || obj.ke.y > 0.0 || obj.ke.z > 0.0) {
		return obj.ke;
	}

	// determinar coordenadas del punto de interseccion
	Point x = r.o + r.d * t;

	// determinar la dirección normal en el punto de interseccion
	Vector n = (x - obj.p);
	n.normalize();

	// Muestrear dirección de salida
	Sample sample = sampleDirection(n, seed);
	Vector wi = sample.d;
	double pdf = sample.pdf;

	// Lanzar rayo desde x en dirección wi con pequeño offset
	Ray shadowRay(x + n * 1e-4, wi);

	// Encontrar la esfera más cercana alcanzada por el rayo
	double t_shadow;
	int id_shadow = 0;
	if (!intersect(shadowRay, t_shadow, id_shadow)) {
		return Color();  // no hay objeto visible en esta dirección
	}

	const Sphere &target = spheres[id_shadow];

	// Si no emite, retorna negro
	if (target.ke.x <= 0.0 && target.ke.y <= 0.0 && target.ke.z <= 0.0) {
		return Color();
	}

	// Calcular contribución: L = fr * Le * cos(theta) / pdf
	// fr = albedo / pi (material difuso)
	double costheta = fmax(0.0, n.dot(wi));
	double fr_factor = 1.0 / M_PI;

	Color L = obj.c.mult(target.ke) * (costheta * fr_factor / pdf);

	return L;
}


int main(int argc, char *argv[]) {
	int w = 1024, h = 768; // image resolution

	// fija la posicion de la camara y la dirección en que mira
	Ray camera( Point(0, 11.2, 214), Vector(0, -0.042612, -1).normalize() );

	// parametros de la camara
	Vector cx = Vector( w * 0.5095 / h, 0., 0.);
	Vector cy = (cx % camera.d).normalize() * 0.5095;

	// Procesar argumentos CLI: sampler y spp
	const char *selectedSampler = "cosinehemi";
	int spp = 32;

	if (argc > 1) {
		selectedSampler = argv[1];
	}
	if (argc > 2) {
		spp = atoi(argv[2]);
	}

	sampler = selectedSampler;

	// auxiliar para valor de pixel y matriz para almacenar la imagen
	Color *pixelColors = new Color[w * h];

	// usar openmp para paralelizar el ciclo: cada hilo computara un renglon
	#pragma omp parallel for schedule(static)
	for(int y = 0; y < h; y++)
	{
		// recorre todos los pixeles de la imagen
		fprintf(stderr,"\r%5.2f%%",100.*y/(h-1));
		for(int x = 0; x < w; x++ ) {
			int idx = (h - y - 1) * w + x; // index en 1D para una imagen 2D x,y son invertidos

			// Inicializar RNG con semilla determinista basada en índice del píxel
			unsigned short seed[3];
			seed[0] = (unsigned short)(idx & 0xFFFF);
			seed[1] = (unsigned short)((idx >> 16) & 0xFFFF);
			seed[2] = 0;

			// para el pixel actual, computar la dirección que un rayo debe tener
			Vector cameraRayDir = cx * ( double(x)/w - .5) + cy * ( double(y)/h - .5) + camera.d;
			Ray cameraRay(camera.o, cameraRayDir.normalize());

			// Promediar spp muestras
			Color pixelValue = Color();
			for (int s = 0; s < spp; s++) {
				pixelValue = pixelValue + shade(cameraRay, seed);
			}
			pixelValue = pixelValue * (1.0 / spp);

			// limitar los tres valores de color del pixel a [0,1]
			pixelColors[idx] = Color(clamp(pixelValue.x), clamp(pixelValue.y), clamp(pixelValue.z));
		}
	}

	fprintf(stderr,"\n");

	// Escribir formato ppm
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
