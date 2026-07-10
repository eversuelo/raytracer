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
	Color c;	// color (albedo)
	Color ke;	// emisión

	Sphere(double r_, Point p_, Color c_, Color ke_ = Color()): r(r_), p(p_), c(c_), ke(ke_) {}
  
	// PROYECTO 1
	// determina si el rayo intersecta a esta esfera
	double intersect(const Ray &ray) const {
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
	//Escena: radio, posicion, albedo, emisión
	Sphere(1e5,  Point(-1e5 - 49, 0, 0),   Color(.75, .25, .25), Color()), // pared izq
	Sphere(1e5,  Point(1e5 + 49, 0, 0),    Color(.25, .25, .75), Color()), // pared der
	Sphere(1e5,  Point(0, 0, -1e5 - 81.6), Color(.25, .75, .25), Color()), // pared detras
	Sphere(1e5,  Point(0, -1e5 - 40.8, 0), Color(.25, .75, .75), Color()), // suelo
	Sphere(1e5,  Point(0, 1e5 + 40.8, 0),  Color(.75, .75, .25), Color()), // techo
	Sphere(16.5, Point(-23, -24.3, -34.6), Color(.2, .3, .4), Color()),     // esfera abajo-izq
	Sphere(16.5, Point(23, -24.3, -3.6),   Color(.4, .3, .2), Color()),     // esfera abajo-der
	Sphere(10.5, Point(0, 24.3, 0),        Color(1, 1, 1), Color(10, 10, 10)) // esfera arriba (emisor)
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

// Variables globales para el muestreador y spp
const char *sampler_name = "uniformsphere";
int spp = 32;

// Función auxiliar: retorna coseno del ángulo polar usando el muestreador especificado
// y el pdf correspondiente, dados dos uniformes (xi1, xi2) en [0, 1)
void sample_direction(const char *sampler, double xi1, double xi2,
                      Vector &local_dir, double &pdf) {
	double costheta, sintheta, phi;
	phi = 2.0 * M_PI * xi2;

	if (strcmp(sampler, "uniformsphere") == 0) {
		costheta = 1.0 - 2.0 * xi1;
		pdf = 1.0 / (4.0 * M_PI);
	} else if (strcmp(sampler, "uniformhemi") == 0) {
		costheta = xi1;
		pdf = 1.0 / (2.0 * M_PI);
	} else { // cosinehemi
		costheta = sqrt(1.0 - xi1);
		pdf = costheta / M_PI;
	}

	sintheta = sqrt(1.0 - costheta * costheta);
	local_dir = Vector(sintheta * cos(phi), sintheta * sin(phi), costheta);
}

// Construye una base ortonormal (s, t, n) donde n es la normal
// s = t × n, t es perpendicular a n
void build_orthonormal_basis(const Vector &n, Vector &s, Vector &t) {
	if (fabs(n.x) > fabs(n.y)) {
		// |n.x| > |n.y|: t = (n.z, 0, -n.x) / sqrt(n.x^2 + n.z^2)
		double norm = sqrt(n.x * n.x + n.z * n.z);
		t = Vector(n.z, 0.0, -n.x) * (1.0 / norm);
	} else {
		// else: t = (0, n.z, -n.y) / sqrt(n.y^2 + n.z^2)
		double norm = sqrt(n.y * n.y + n.z * n.z);
		t = Vector(0.0, n.z, -n.y) * (1.0 / norm);
	}
	s = t % n; // s = t × n
}

// Calcula el valor de color para el rayo dado (una muestra)
Color shade(const Ray &r, unsigned short *rand_state) {
	double t;
	int id = 0;
	// determinar que esfera (id) y a que distancia (t) el rayo intersecta
	if (!intersect(r, t, id))
		return Color(); // el rayo no intersecto objeto, return Color() == negro

	const Sphere &obj = spheres[id];

	// determinar coordenadas del punto de interseccion
	Point x = r.o + r.d * t;

	// determinar la dirección normal en el punto de interseccion
	Vector n = (x - obj.p);
	n.normalize();

	// Si el objeto es un emisor (ke ≠ 0), devolver su emisión
	if (obj.ke.x > 0 || obj.ke.y > 0 || obj.ke.z > 0) {
		return obj.ke;
	}

	// Iluminación directa por Monte Carlo
	// Generar dirección uniforme
	double xi1 = erand48(rand_state);
	double xi2 = erand48(rand_state);

	Vector local_dir;
	double pdf;
	sample_direction(sampler_name, xi1, xi2, local_dir, pdf);

	// Convertir de local a global usando base ortonormal
	Vector s, t_basis;
	build_orthonormal_basis(n, s, t_basis);

	Vector wi = s * local_dir.x + t_basis * local_dir.y + n * local_dir.z;

	// Lanzar rayo de sombra desde x
	Ray shadow_ray(x + n * 1e-4, wi); // epsilon = 1e-4
	double t_shadow;
	int id_shadow;
	Color L = Color(); // acumulador

	if (intersect(shadow_ray, t_shadow, id_shadow)) {
		const Sphere &shadow_obj = spheres[id_shadow];
		// Si toca un emisor, usar su emisión
		if (shadow_obj.ke.x > 0 || shadow_obj.ke.y > 0 || shadow_obj.ke.z > 0) {
			// Le = shadow_obj.ke
			// fr = obj.c / π
			// cos(θ) = n · wi
			double costheta = n.dot(wi);
			if (costheta > 0) {
				Color fr = obj.c * (1.0 / M_PI);
				L = shadow_obj.ke.mult(fr) * (costheta / pdf);
			}
		}
	}

	return L;
}


int main(int argc, char *argv[]) {
	int w = 1024, h = 768; // image resolution

	// parsear argumentos de línea de comandos
	if (argc > 1) {
		sampler_name = argv[1];
	}
	if (argc > 2) {
		spp = atoi(argv[2]);
	}

	// fija la posicion de la camara y la dirección en que mira
	Ray camera( Point(0, 11.2, 214), Vector(0, -0.042612, -1).normalize() );

	// parametros de la camara
	Vector cx = Vector( w * 0.5095 / h, 0., 0.);
	Vector cy = (cx % camera.d).normalize() * 0.5095;

	// auxiliar para valor de pixel y matriz para almacenar la imagen
	Color *pixelColors = new Color[w * h];

	// usar openmp para paralelizar el ciclo: cada hilo computara un renglon
	#pragma omp parallel for schedule(dynamic)
	for(int y = 0; y < h; y++)
	{
		// recorre todos los pixeles de la imagen
		#pragma omp critical
		fprintf(stderr,"\r%5.2f%%",100.*y/(h-1));

		for(int x = 0; x < w; x++ ) {
			int idx = (h - y - 1) * w + x; // index en 1D para una imagen 2D
			Color accumulator = Color(); // acumulador para las N muestras

			// para el pixel actual, computar la dirección que un rayo debe tener
			Vector cameraRayDir = cx * ( double(x)/w - .5) + cy * ( double(y)/h - .5) + camera.d;
			cameraRayDir.normalize();

			// Inicializar estado RNG per-pixel de forma determinista
			unsigned short rand_state[3];
			unsigned int pixel_index = y * w + x;
			rand_state[0] = (pixel_index >> 16) & 0xFFFF;
			rand_state[1] = pixel_index & 0xFFFF;
			rand_state[2] = 5489 ^ pixel_index; // semilla fija + índice

			// Promediar spp muestras
			for (int s = 0; s < spp; s++) {
				Color sample = shade( Ray(camera.o, cameraRayDir), rand_state );
				accumulator = accumulator + sample;
			}

			// Promediar
			Color pixelValue = accumulator * (1.0 / spp);

			// limitar los tres valores de color del pixel a [0,1]
			pixelColors[idx] = Color(clamp(pixelValue.x), clamp(pixelValue.y), clamp(pixelValue.z));
		}
	}

	fprintf(stderr,"\n");

	// PROYECTO 1
	// Investigar formato ppm
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
