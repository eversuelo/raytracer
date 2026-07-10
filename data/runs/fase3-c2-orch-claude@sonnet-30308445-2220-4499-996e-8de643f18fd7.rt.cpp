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

	// norma del vector (longitud)
	double norm() const { return sqrt(x * x + y * y + z * z); }

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

// Escenas múltiples: 1L (una esfera emisora) y 2A1P (dos esféricas + una puntual)
Sphere spheres_1L[] = {
	//Escena 1L: radio, posicion, albedo, emisión
	Sphere(1e5,  Point(-1e5 - 49, 0, 0),   Color(.75, .25, .25), Color()), // pared izq
	Sphere(1e5,  Point(1e5 + 49, 0, 0),    Color(.25, .25, .75), Color()), // pared der
	Sphere(1e5,  Point(0, 0, -1e5 - 81.6), Color(.25, .75, .25), Color()), // pared detras
	Sphere(1e5,  Point(0, -1e5 - 40.8, 0), Color(.25, .75, .75), Color()), // suelo
	Sphere(1e5,  Point(0, 1e5 + 40.8, 0),  Color(.75, .75, .25), Color()), // techo
	Sphere(16.5, Point(-23, -24.3, -34.6), Color(.2, .3, .4), Color()),     // esfera abajo-izq
	Sphere(16.5, Point(23, -24.3, -3.6),   Color(.4, .3, .2), Color()),     // esfera abajo-der
	Sphere(10.5, Point(0, 24.3, 0),        Color(1, 1, 1), Color(10, 10, 10)) // esfera arriba (emisor)
};

Sphere spheres_2A1P[] = {
	//Escena 2A1P: radio, posicion, albedo, emisión
	Sphere(1e5,  Point(-1e5 - 49, 0, 0),   Color(.75, .25, .25), Color()),    // pared izq
	Sphere(1e5,  Point(1e5 + 49, 0, 0),    Color(.25, .25, .75), Color()),    // pared der
	Sphere(1e5,  Point(0, 0, -1e5 - 81.6), Color(.25, .75, .25), Color()),    // pared detras
	Sphere(1e5,  Point(0, -1e5 - 40.8, 0), Color(.25, .75, .75), Color()),    // suelo
	Sphere(1e5,  Point(0, 1e5 + 40.8, 0),  Color(.75, .75, .25), Color()),    // techo
	Sphere(16.5, Point(-23, -24.3, -34.6), Color(.2, .3, .4), Color()),       // esfera abajo-izq
	Sphere(16.5, Point(23, -24.3, -3.6),   Color(.4, .3, .2), Color()),       // esfera abajo-der
	Sphere(0.0,  Point(0, 24.3, 0),        Color(0, 0, 0), Color(2000, 2000, 2000)), // puntual
	Sphere(10.5, Point(-23, 24.3, 0),      Color(1, 1, 1), Color(12, 5, 5)),  // esfera emisora 1
	Sphere(5.0,  Point(23, 24.3, -50),     Color(1, 1, 1), Color(5, 5, 12))   // esfera emisora 2
};

// Punteros a escena activa y cantidad de esferas
Sphere *spheres = spheres_1L;
int num_spheres = 8;

// Índices de emisores por escena
int emitter_indices_1L[] = {7};
int num_emitters_1L = 1;

int emitter_indices_2A1P[] = {7, 8, 9};
int num_emitters_2A1P = 3;

int *emitter_indices = emitter_indices_1L;
int num_emitters = num_emitters_1L;

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

// Calcula la intersección del rayo r con todas las esferas (excepto emisores en modos de muestreo de luz)
// regresar true si hubo una intersección, falso de otro modo
// almacenar en t la distancia sobre el rayo en que sucede la interseccion
// almacenar en id el indice de spheres[] de la esfera cuya interseccion es mas cercana
inline bool intersect(const Ray &r, double &t, int &id, bool skip_emitters = false) {
	t = 1e20;
	bool found = false;
	for (int i = 0; i < num_spheres; i++) {
		// Saltar emisores si se solicita (para rayos de sombra de muestreo de luz)
		if (skip_emitters) {
			bool is_emitter = false;
			for (int j = 0; j < num_emitters; j++) {
				if (i == emitter_indices[j]) {
					is_emitter = true;
					break;
				}
			}
			if (is_emitter) continue;
		}

		double hit = spheres[i].intersect(r);
		if (hit > 0.0 && hit < t) {
			t = hit;
			id = i;
			found = true;
		}
	}
	return found;
}

// Variables globales para el método de iluminación y spp
const char *light_method = "arealight"; // arealight, solidangle, coshemi
int spp = 32;

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

// Muestrea direcciones uniformes en el hemisferio (coseno-hemisférico)
// Usado en modo coshemi
void sample_cosine_hemisphere(double xi1, double xi2,
                              Vector &local_dir, double &pdf) {
	double phi = 2.0 * M_PI * xi2;
	double costheta = sqrt(1.0 - xi1);
	double sintheta = sqrt(xi1);

	local_dir = Vector(sintheta * cos(phi), sintheta * sin(phi), costheta);
	pdf = costheta / M_PI;
}

// Muestrea área uniforme sobre la superficie de una esfera emisora
// Retorna: dirección (x' - x) / |x' - x|, pdf en ángulo sólido, normal de luz, distancia
void sample_area_light(const Point &x, const Sphere &light, double xi1, double xi2,
                       Vector &direction, double &pdf, Vector &light_normal, double &dist_to_sample) {
	// Muestrear punto uniforme en la esfera: θ = acos(1 - 2ξ₁), φ = 2πξ₂
	double theta = acos(1.0 - 2.0 * xi1);
	double phi = 2.0 * M_PI * xi2;

	double sintheta = sin(theta);
	double costheta = cos(theta);

	// Punto en esfera local
	Vector local_point(sintheta * cos(phi), sintheta * sin(phi), costheta);

	// Transformar a coordenadas globales
	Point x_prime = light.p + local_point * light.r;

	// Normal de la luz en x_prime (hacia afuera)
	light_normal = local_point; // ya normalizado en la esfera unitaria
	light_normal.normalize();

	// Dirección desde x hacia x'
	Vector w = x_prime - x;
	double d = w.norm();
	direction = w * (1.0 / d); // normalizar
	dist_to_sample = d; // retornar distancia al punto muestreado

	// Coseno del ángulo entre normal de luz y dirección x' -> x
	double cos_theta_prime = light_normal.dot(direction * (-1.0));

	// pdf_ω = d² / (4πr² · cosθ')
	if (cos_theta_prime > 0.0) {
		pdf = (d * d) / (4.0 * M_PI * light.r * light.r * cos_theta_prime);
	} else {
		pdf = 0.0;
	}
}

// Muestrea el cono subtendido por una esfera emisora
// Retorna: dirección, pdf en ángulo sólido, normal de luz (siempre la fuente)
void sample_solid_angle_light(const Point &x, const Sphere &light, double xi1, double xi2,
                              Vector &direction, double &pdf) {
	// Eje del cono: W = normalize(c - x)
	Vector W = light.p - x;
	double d = W.norm();
	W = W * (1.0 / d);

	// cosθmax = sqrt(1 - (r/d)²)
	double cos_theta_max = sqrt(1.0 - (light.r / d) * (light.r / d));

	// Muestrear dentro del cono
	// θ = acos(1 - ξ₁ + ξ₁·cosθmax)
	double theta = acos(1.0 - xi1 + xi1 * cos_theta_max);
	double phi = 2.0 * M_PI * xi2;

	double sintheta = sin(theta);
	double costheta = cos(theta);

	// Construir base ortonormal (s, t, W)
	Vector s, t;
	build_orthonormal_basis(W, s, t);

	// Dirección en base local
	direction = s * (sintheta * cos(phi)) + t * (sintheta * sin(phi)) + W * costheta;

	// pdf_ω = 1 / (2π(1 - cosθmax))
	pdf = 1.0 / (2.0 * M_PI * (1.0 - cos_theta_max));
}

// Calcula iluminación directa por muestreo de importancia de múltiples luces
// Soporta tres modos: arealight, solidangle, coshemi
Color shade_lights(const Ray &r, unsigned short *rand_state) {
	double t;
	int id = 0;
	// Determinar qué esfera (id) y a qué distancia (t) el rayo intersecta
	if (!intersect(r, t, id, false))
		return Color(); // el rayo no intersecó objeto, retorna negro

	const Sphere &obj = spheres[id];

	// Determinar coordenadas del punto de intersección
	Point x = r.o + r.d * t;

	// Determinar la dirección normal en el punto de intersección
	Vector n = (x - obj.p);
	n.normalize();

	// Si el objeto es un emisor (ke ≠ 0), devolver su emisión (clamped a 1 = blanco)
	if (obj.ke.x > 0 || obj.ke.y > 0 || obj.ke.z > 0) {
		Color result = obj.ke;
		result.x = (result.x > 1.0) ? 1.0 : result.x;
		result.y = (result.y > 1.0) ? 1.0 : result.y;
		result.z = (result.z > 1.0) ? 1.0 : result.z;
		return result;
	}

	Color L = Color(); // acumulador de iluminación

	if (strcmp(light_method, "coshemi") == 0) {
		// Modo coseno-hemisférico: muestrear hemisferio + término determinista de puntuales
		double xi1 = erand48(rand_state);
		double xi2 = erand48(rand_state);

		Vector local_dir;
		double pdf;
		sample_cosine_hemisphere(xi1, xi2, local_dir, pdf);

		// Convertir a global
		Vector s, t_basis;
		build_orthonormal_basis(n, s, t_basis);
		Vector wi = s * local_dir.x + t_basis * local_dir.y + n * local_dir.z;

		// Rayo de sombra
		Ray shadow_ray(x + n * 1e-4, wi);
		double t_shadow;
		int id_shadow;

		if (intersect(shadow_ray, t_shadow, id_shadow, false)) {
			const Sphere &shadow_obj = spheres[id_shadow];
			if (shadow_obj.ke.x > 0 || shadow_obj.ke.y > 0 || shadow_obj.ke.z > 0) {
				double costheta = n.dot(wi);
				if (costheta > 0) {
					Color fr = obj.c * (1.0 / M_PI);
					L = shadow_obj.ke.mult(fr) * (costheta / pdf);
				}
			}
		}

		// Agregar término determinista de fuentes puntuales
		for (int i = 0; i < num_emitters; i++) {
			const Sphere &light = spheres[emitter_indices[i]];
			// Solo puntuales (r = 0)
			if (light.r < 1e-6) {
				Vector w = light.p - x;
				double r_dist = w.norm();
				w = w * (1.0 / r_dist);

				double costheta = n.dot(w);
				if (costheta > 0.0) {
					// Rayo de sombra
					Ray shadow_ray_point(x + n * 1e-4, w);
					double t_shadow_point;
					int id_shadow_point;

					bool visible = true;
					if (intersect(shadow_ray_point, t_shadow_point, id_shadow_point, false)) {
						if (t_shadow_point < r_dist - 1e-4) {
							visible = false;
						}
					}

					if (visible) {
						Color fr = obj.c * (1.0 / M_PI);
						Color contrib = fr.mult(light.ke) * (costheta / (r_dist * r_dist));
						L = L + contrib;
					}
				}
			}
		}
	} else {
		// Modo arealight o solidangle: iterar sobre todos los emisores
		for (int i = 0; i < num_emitters; i++) {
			const Sphere &light = spheres[emitter_indices[i]];

			if (light.r < 1e-6) {
				// Fuente puntual: término determinista
				Vector w = light.p - x;
				double r_dist = w.norm();
				w = w * (1.0 / r_dist);

				double costheta = n.dot(w);
				if (costheta > 0.0) {
					Ray shadow_ray_point(x + n * 1e-4, w);
					double t_shadow_point;
					int id_shadow_point;

					bool visible = true;
					if (intersect(shadow_ray_point, t_shadow_point, id_shadow_point, false)) {
						if (t_shadow_point < r_dist - 1e-4) {
							visible = false;
						}
					}

					if (visible) {
						Color fr = obj.c * (1.0 / M_PI);
						Color contrib = fr.mult(light.ke) * (costheta / (r_dist * r_dist));
						L = L + contrib;
					}
				}
			} else {
				// Fuente esférica: muestreo de importancia
				double xi1 = erand48(rand_state);
				double xi2 = erand48(rand_state);

				Vector direction;
				double pdf;
				Vector light_normal;

				if (strcmp(light_method, "arealight") == 0) {
					sample_area_light(x, light, xi1, xi2, direction, pdf, light_normal);
				} else { // solidangle
					sample_solid_angle_light(x, light, xi1, xi2, direction, pdf);
					light_normal = (light.p - x);
					light_normal.normalize();
				}

				if (pdf > 0.0) {
					double costheta = n.dot(direction);

					if (costheta > 0.0) {
						Ray shadow_ray(x + n * 1e-4, direction);
						double t_shadow;
						int id_shadow;

						bool visible = false;

						if (strcmp(light_method, "solidangle") == 0) {
							// Verificar que el primer hit sea la fuente
							if (intersect(shadow_ray, t_shadow, id_shadow, false)) {
								if (id_shadow == emitter_indices[i]) {
									visible = true;
								}
							}
						} else { // arealight
							// Verificar visibilidad simple (no toca otros objetos)
							double dist_to_light = (light.p - x).norm();
							if (!intersect(shadow_ray, t_shadow, id_shadow, false) ||
								t_shadow > dist_to_light) {
								visible = true;
							}
						}

						if (visible) {
							Color fr = obj.c * (1.0 / M_PI);
							Color contrib = fr.mult(light.ke) * (costheta / pdf);
							L = L + contrib;
						}
					}
				}
			}
		}
	}

	return L;
}



int main(int argc, char *argv[]) {
	int w = 1024, h = 768; // image resolution

	// Parsear argumentos: ./rt <modo> <spp> <escena>
	// modo: arealight, solidangle, coshemi
	// escena: 1L, 2A1P
	const char *output_file = "image.ppm";

	if (argc < 4) {
		fprintf(stderr, "Usage: %s <modo> <spp> <escena>\n", argv[0]);
		fprintf(stderr, "  modo: arealight, solidangle, coshemi\n");
		fprintf(stderr, "  spp: samples per pixel (integer)\n");
		fprintf(stderr, "  escena: 1L, 2A1P\n");
		return 1;
	}

	light_method = argv[1];
	spp = atoi(argv[2]);
	const char *scene_name = argv[3];

	// Configurar escena
	if (strcmp(scene_name, "1L") == 0) {
		spheres = spheres_1L;
		num_spheres = 8;
		emitter_indices = emitter_indices_1L;
		num_emitters = num_emitters_1L;
	} else if (strcmp(scene_name, "2A1P") == 0) {
		spheres = spheres_2A1P;
		num_spheres = 10;
		emitter_indices = emitter_indices_2A1P;
		num_emitters = num_emitters_2A1P;
	} else {
		fprintf(stderr, "Unknown scene: %s\n", scene_name);
		return 1;
	}

	// Siempre generar image.ppm (el check.sh lo copia al nombre final)
	output_file = "image.ppm";

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

			// Inicializar estado RNG per-pixel de forma determinista (seguro para OpenMP)
			unsigned short rand_state[3];
			unsigned int pixel_index = y * w + x;
			rand_state[0] = (pixel_index >> 16) & 0xFFFF;
			rand_state[1] = pixel_index & 0xFFFF;
			rand_state[2] = 5489 ^ pixel_index; // semilla fija + índice

			// Promediar spp muestras
			for (int s = 0; s < spp; s++) {
				Color sample = shade_lights( Ray(camera.o, cameraRayDir), rand_state );
				accumulator = accumulator + sample;
			}

			// Promediar
			Color pixelValue = accumulator * (1.0 / spp);

			// limitar los tres valores de color del pixel a [0,1]
			pixelColors[idx] = Color(clamp(pixelValue.x), clamp(pixelValue.y), clamp(pixelValue.z));
		}
	}

	fprintf(stderr,"\n");

	// Guardar imagen PPM
	FILE *f = fopen(output_file, "w");
	// escribe cabecera del archivo ppm, ancho, alto y valor maximo de color
	fprintf(f, "P3\n%d %d\n%d\n", w, h, 255);
	for (int p = 0; p < w * h; p++)
	{ // escribe todos los valores de los pixeles
    		fprintf(f,"%d %d %d ", toDisplayValue(pixelColors[p].x), toDisplayValue(pixelColors[p].y),
				toDisplayValue(pixelColors[p].z));
  	}
  	fclose(f);

  	delete[] pixelColors;

	fprintf(stderr, "Render complete: %s\n", output_file);

	return 0;
}
