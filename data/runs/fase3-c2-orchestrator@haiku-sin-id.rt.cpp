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

// Escena 1L: una esfera emisora (fase 1)
Sphere spheres_1L[] = {
	Sphere(1e5,  Point(-1e5 - 49, 0, 0),   Color(.75, .25, .25), Color(0, 0, 0)), // pared izq
	Sphere(1e5,  Point(1e5 + 49, 0, 0),    Color(.25, .25, .75), Color(0, 0, 0)), // pared der
	Sphere(1e5,  Point(0, 0, -1e5 - 81.6), Color(.25, .75, .25), Color(0, 0, 0)), // pared detras
	Sphere(1e5,  Point(0, -1e5 - 40.8, 0), Color(.25, .75, .75), Color(0, 0, 0)), // suelo
	Sphere(1e5,  Point(0, 1e5 + 40.8, 0),  Color(.75, .75, .25), Color(0, 0, 0)), // techo
	Sphere(16.5, Point(-23, -24.3, -34.6), Color(.2, .3, .4), Color(0, 0, 0)), // esfera abajo-izq
	Sphere(16.5, Point(23, -24.3, -3.6),   Color(.4, .3, .2), Color(0, 0, 0)), // esfera abajo-der
	Sphere(10.5, Point(0, 24.3, 0),        Color(1, 1, 1), Color(10, 10, 10)) // emisor arriba (fase 1)
};

// Escena 2A1P: dos esferas emisoras + una puntual
Sphere spheres_2A1P[] = {
	Sphere(1e5,  Point(-1e5 - 49, 0, 0),   Color(.75, .25, .25), Color(0, 0, 0)), // pared izq
	Sphere(1e5,  Point(1e5 + 49, 0, 0),    Color(.25, .25, .75), Color(0, 0, 0)), // pared der
	Sphere(1e5,  Point(0, 0, -1e5 - 81.6), Color(.25, .75, .25), Color(0, 0, 0)), // pared detras
	Sphere(1e5,  Point(0, -1e5 - 40.8, 0), Color(.25, .75, .75), Color(0, 0, 0)), // suelo
	Sphere(1e5,  Point(0, 1e5 + 40.8, 0),  Color(.75, .75, .25), Color(0, 0, 0)), // techo
	Sphere(16.5, Point(-23, -24.3, -34.6), Color(.2, .3, .4), Color(0, 0, 0)), // esfera abajo-izq
	Sphere(16.5, Point(23, -24.3, -3.6),   Color(.4, .3, .2), Color(0, 0, 0)), // esfera abajo-der
	Sphere(0.0,  Point(0, 24.3, 0),        Color(0, 0, 0), Color(2000, 2000, 2000)), // puntual
	Sphere(10.5, Point(-23, 24.3, 0),      Color(1, 1, 1), Color(12, 5, 5)),     // esfera emisora 1
	Sphere(5,    Point(23, 24.3, -50),     Color(1, 1, 1), Color(5, 5, 12))      // esfera emisora 2
};

// Punteros globales para manejar las escenas
Sphere* current_spheres = spheres_1L;
int current_sphere_count = 8;
int emitter_count = 0; // número de emisores en la escena

// Lista de emisores (esferas con ke > 0)
struct Emitter {
	int sphere_id;    // índice en current_spheres
	int emitter_idx;  // índice en la lista de emisores
};

Emitter emitters_1L[1] = {
	{7, 0}  // esfera en índice 7 es el emisor
};

Emitter emitters_2A1P[3] = {
	{7, 0},  // puntual
	{8, 1},  // esfera emisora 1
	{9, 2}   // esfera emisora 2
};

Emitter* current_emitters = emitters_1L;

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

// Calcular la intersección del rayo r con todas las esferas en current_spheres
inline bool intersect(const Ray &r, double &t, int &id) {
	t = 1e20;
	bool found = false;
	for (int i = 0; i < current_sphere_count; i++) {
		double hit = current_spheres[i].intersect(r);
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

// Muestreo de fuentes esféricas por muestreo de área
struct AreaLightSample {
	Point x_light;    // punto muestreado en la esfera
	Vector normal;    // normal en el punto (apunta hacia afuera)
};

AreaLightSample arealight_sample(const Sphere &light, double xi1, double xi2) {
	AreaLightSample result;

	// Muestrear punto uniforme en la esfera: θ = acos(1 - 2ξ₁), φ = 2πξ₂
	double costheta = 1.0 - 2.0 * xi1;
	double sintheta = sqrt(fmax(0.0, 1.0 - costheta * costheta));
	double phi = 2.0 * M_PI * xi2;

	// Normal en el punto muestreado (es la dirección radial normalizada)
	result.normal = Vector(sintheta * cos(phi), sintheta * sin(phi), costheta);

	// Punto en la esfera
	result.x_light = light.p + result.normal * light.r;

	return result;
}

// Muestreo de fuentes esféricas por ángulo sólido (cono)
struct SolidAngleSample {
	Point x_light;    // punto en la superficie de la esfera
	double pdf_omega; // pdf en ángulo sólido
	Vector direction;  // dirección desde x hacia x_light
};

SolidAngleSample solidangle_sample(const Sphere &light, const Point &x, double xi1, double xi2) {
	SolidAngleSample result;

	// Eje W = normalize(c - x)
	Vector W = light.p - x;
	double dist = sqrt(W.dot(W));
	W = W * (1.0 / dist);

	// cosθ_max = √(1 − (r/d)²)
	double r_over_d = light.r / dist;
	double costheta_max = sqrt(fmax(0.0, 1.0 - r_over_d * r_over_d));

	// θ = acos(1 − ξ₁ + ξ₁·cosθ_max), φ = 2πξ₂
	double costheta = 1.0 - xi1 + xi1 * costheta_max;
	double sintheta = sqrt(fmax(0.0, 1.0 - costheta * costheta));
	double phi = 2.0 * M_PI * xi2;

	// Dirección en base local de W
	Vector u, v;
	if (fabs(W.x) > fabs(W.y)) {
		u = Vector(W.z, 0, -W.x);
		u.normalize();
	} else {
		u = Vector(0, W.z, -W.y);
		u.normalize();
	}
	v = W % u;

	Vector dir = u * (sintheta * cos(phi)) + v * (sintheta * sin(phi)) + W * costheta;
	dir.normalize();

	result.direction = dir;
	result.pdf_omega = 1.0 / (2.0 * M_PI * (1.0 - costheta_max));

	// Encontrar la intersección del rayo (x, dir) con la esfera
	// Para calcular x_light en la intersección
	Vector oc = x - light.p;
	double b = oc.dot(dir);
	double det = b * b - oc.dot(oc) + light.r * light.r;
	if (det >= 0.0) {
		double sqrt_det = sqrt(det);
		double t = -b - sqrt_det;
		if (t > 1e-4) {
			result.x_light = x + dir * t;
		} else {
			t = -b + sqrt_det;
			result.x_light = x + dir * t;
		}
	}

	return result;
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

// Modo de muestreo para fuentes esféricas (0 = arealight, 1 = solidangle, 2 = coshemi)
int light_sampling_mode = 0;

// Shade para fase 03: múltiples emisores con muestreo de importancia
Color shade(const Ray &r, unsigned short xi[3]) {
	double t;
	int id = 0;
	if (!intersect(r, t, id))
		return Color();

	const Sphere &obj = current_spheres[id];
	Point x = r.o + r.d * t;
	Vector n = (x - obj.p);
	n.normalize();

	// Si el objeto emite, devolver su emisión (clamped a blanco)
	if (obj.ke.x > 0 || obj.ke.y > 0 || obj.ke.z > 0) {
		return Color(clamp(obj.ke.x), clamp(obj.ke.y), clamp(obj.ke.z));
	}

	Color contrib(0, 0, 0);

	// Iterar sobre todos los emisores
	for (int e = 0; e < emitter_count; e++) {
		const Sphere &light = current_spheres[current_emitters[e].sphere_id];

		if (light.r == 0.0) {
			// Fuente puntual: término determinista
			Vector w = light.p - x;
			double r_dist = sqrt(w.dot(w));
			w = w * (1.0 / r_dist);

			double costheta = n.dot(w);
			if (costheta > 0.0) {
				// Lanzar rayo de sombra
				Ray shadowRay(x + n * 1e-4, w);
				double t_shadow;
				int id_shadow = 0;
				bool hit = intersect(shadowRay, t_shadow, id_shadow);

				// Verificar si la luz es visible
				bool visible = !hit || t_shadow >= r_dist - 1e-4;

				if (visible) {
					Vector fr = obj.c * (1.0 / M_PI);
					double r_squared = r_dist * r_dist;
					Color light_contrib = light.ke.mult(fr) * (costheta / r_squared);
					contrib = contrib + light_contrib;
				}
			}
		} else {
			// Fuente esférica: muestreo de importancia
			double xi1 = erand48(xi);
			double xi2 = erand48(xi);

			if (light_sampling_mode == 0) {
				// Arealight: muestreo uniforme de la esfera
				double costheta_sample = 1.0 - 2.0 * xi1;
				double sintheta_sample = sqrt(fmax(0.0, 1.0 - costheta_sample * costheta_sample));
				double phi_sample = 2.0 * M_PI * xi2;

				Vector normal_sample(sintheta_sample * cos(phi_sample), sintheta_sample * sin(phi_sample), costheta_sample);
				Point x_light = light.p + normal_sample * light.r;

				Vector to_light = x_light - x;
				double d = sqrt(to_light.dot(to_light));
				to_light = to_light * (1.0 / d);

				double costheta = n.dot(to_light);
				if (costheta > 0.0) {
					Vector from_light_to_x = (x - x_light) * (1.0 / d);
					double cos_theta_prime = normal_sample.dot(from_light_to_x);

					if (cos_theta_prime > 0.0) {
						// Test de visibilidad
						Ray shadowRay(x + n * 1e-4, to_light);
						double t_shadow;
						int id_shadow = 0;
						bool hit = intersect(shadowRay, t_shadow, id_shadow);

						if (hit && id_shadow == current_emitters[e].sphere_id) {
							// Contribución: Le · (albedo/π) · cosθ · (4πr² · cos²θ') / d²
							Vector fr = obj.c * (1.0 / M_PI);
							double r_sq = light.r * light.r;
							double d_sq = d * d;
							double cos_sq_prime = cos_theta_prime * cos_theta_prime;
							Color light_contrib = light.ke.mult(fr) * (costheta * 4.0 * M_PI * r_sq * cos_sq_prime / d_sq);
							contrib = contrib + light_contrib;
						}
					}
				}
			} else if (light_sampling_mode == 1) {
				// Solidangle: muestreo del cono
				SolidAngleSample sample = solidangle_sample(light, x, xi1, xi2);

				Vector to_light = sample.x_light - x;
				double d = sqrt(to_light.dot(to_light));
				to_light = to_light * (1.0 / d);

				double costheta = n.dot(to_light);
				if (costheta > 0.0) {
					Ray shadowRay(x + n * 1e-4, sample.direction);
					double t_shadow;
					int id_shadow = 0;
					bool hit = intersect(shadowRay, t_shadow, id_shadow);

					if (hit && id_shadow == current_emitters[e].sphere_id) {
						Vector fr = obj.c * (1.0 / M_PI);
						Color light_contrib = light.ke.mult(fr) * (costheta / sample.pdf_omega);
						contrib = contrib + light_contrib;
					}
				}
			} else if (false) {
				// Solidangle: muestreo del cono
				SolidAngleSample sample = solidangle_sample(light, x, xi1, xi2);

				Vector to_light = sample.x_light - x;
				double d = sqrt(to_light.dot(to_light));
				to_light = to_light * (1.0 / d);

				double costheta = n.dot(to_light);
				if (costheta > 0.0) {
					Ray shadowRay(x + n * 1e-4, sample.direction);
					double t_shadow;
					int id_shadow = 0;
					bool hit = intersect(shadowRay, t_shadow, id_shadow);

					if (hit && id_shadow == current_emitters[e].sphere_id) {
						Vector fr = obj.c * (1.0 / M_PI);
						Color light_contrib = light.ke.mult(fr) * (costheta / sample.pdf_omega);
						contrib = contrib + light_contrib;
					}
				}
			} else if (light_sampling_mode == 2) {
				// Coshemi: muestreo coseno-hemisférico (sin luz esférica)
				// No procesamos esferas emisoras en modo coshemi
			}
		}
	}

	return Color(clamp(contrib.x), clamp(contrib.y), clamp(contrib.z));
}

// Shade para modo coshemi: coseno-hemisférico + términos deterministas de puntales
Color shade_coshemi(const Ray &r, unsigned short xi[3]) {
	double t;
	int id = 0;
	if (!intersect(r, t, id))
		return Color();

	const Sphere &obj = current_spheres[id];
	Point x = r.o + r.d * t;
	Vector n = (x - obj.p);
	n.normalize();

	// Si el objeto emite, devolver su emisión
	if (obj.ke.x > 0 || obj.ke.y > 0 || obj.ke.z > 0) {
		return Color(clamp(obj.ke.x), clamp(obj.ke.y), clamp(obj.ke.z));
	}

	// Generar UNA muestra del estimador coseno-hemisférico
	double xi1 = erand48(xi);
	double xi2 = erand48(xi);
	Vector wi = cosinehemi_sample(xi1, xi2, n);

	// Lanzar rayo con origen x + n*epsilon
	Ray scatter(x + n * 1e-4, wi);
	double t2;
	int id2 = 0;
	if (intersect(scatter, t2, id2)) {
		const Sphere &emitter = current_spheres[id2];
		// Si el rayo de muestreo toca un emisor, acumular su contribución
		if (emitter.ke.x > 0 || emitter.ke.y > 0 || emitter.ke.z > 0) {
			double costheta = n.dot(wi);
			if (costheta > 0.0) {
				double pdf = cosinehemi_pdf(costheta);
				Vector fr = obj.c * (1.0 / M_PI);
				Color contrib = emitter.ke.mult(fr) * (costheta / pdf);

				// Agregar términos deterministas de fuentes puntuales
				for (int e = 0; e < emitter_count; e++) {
					const Sphere &light = current_spheres[current_emitters[e].sphere_id];

					if (light.r == 0.0) {
						Vector w = light.p - x;
						double r_dist = sqrt(w.dot(w));
						w = w * (1.0 / r_dist);

						double costheta_pt = n.dot(w);
						if (costheta_pt > 0.0) {
							Ray shadowRay(x + n * 1e-4, w);
							double t_shadow;
							int id_shadow = 0;
							bool hit = intersect(shadowRay, t_shadow, id_shadow);

							bool visible = !hit || t_shadow >= r_dist - 1e-4;
							if (visible) {
								Vector fr_pt = obj.c * (1.0 / M_PI);
								double r_squared = r_dist * r_dist;
								Color light_contrib = light.ke.mult(fr_pt) * (costheta_pt / r_squared);
								contrib = contrib + light_contrib;
							}
						}
					}
				}

				return Color(clamp(contrib.x), clamp(contrib.y), clamp(contrib.z));
			}
		}
	}

	// Agregar términos deterministas de fuentes puntuales (incluso si coshemi no toca emisor)
	Color contrib(0, 0, 0);
	for (int e = 0; e < emitter_count; e++) {
		const Sphere &light = current_spheres[current_emitters[e].sphere_id];

		if (light.r == 0.0) {
			Vector w = light.p - x;
			double r_dist = sqrt(w.dot(w));
			w = w * (1.0 / r_dist);

			double costheta = n.dot(w);
			if (costheta > 0.0) {
				Ray shadowRay(x + n * 1e-4, w);
				double t_shadow;
				int id_shadow = 0;
				bool hit = intersect(shadowRay, t_shadow, id_shadow);

				bool visible = !hit || t_shadow >= r_dist - 1e-4;
				if (visible) {
					Vector fr = obj.c * (1.0 / M_PI);
					double r_squared = r_dist * r_dist;
					Color light_contrib = light.ke.mult(fr) * (costheta / r_squared);
					contrib = contrib + light_contrib;
				}
			}
		}
	}

	return Color(clamp(contrib.x), clamp(contrib.y), clamp(contrib.z));
}


int main(int argc, char *argv[]) {
	int w = 1024, h = 768;
	Ray camera( Point(0, 11.2, 214), Vector(0, -0.042612, -1).normalize() );
	Vector cx = Vector( w * 0.5095 / h, 0., 0.);
	Vector cy = (cx % camera.d).normalize() * 0.5095;

	// Parsear argumentos: ./rt <modo> <spp> <escena>
	// modo ∈ {arealight, solidangle, coshemi}
	// escena ∈ {1L, 2A1P}
	const char *mode = "coshemi";
	int spp = 32;
	const char *scene = "1L";
	const char *output_file = "image.ppm";

	if (argc > 1) {
		mode = argv[1];
	}

	if (argc > 2) {
		spp = atoi(argv[2]);
	}

	if (argc > 3) {
		scene = argv[3];
	}

	// Configurar escena
	if (strcmp(scene, "1L") == 0) {
		current_spheres = spheres_1L;
		current_sphere_count = 8;
		current_emitters = emitters_1L;
		emitter_count = 1;
	} else if (strcmp(scene, "2A1P") == 0) {
		current_spheres = spheres_2A1P;
		current_sphere_count = 10;
		current_emitters = emitters_2A1P;
		emitter_count = 3;
	} else {
		fprintf(stderr, "Error: escena debe ser '1L' o '2A1P'\n");
		return 1;
	}

	// Configurar modo de iluminación
	if (strcmp(mode, "arealight") == 0) {
		light_sampling_mode = 0;
	} else if (strcmp(mode, "solidangle") == 0) {
		light_sampling_mode = 1;
	} else if (strcmp(mode, "coshemi") == 0) {
		light_sampling_mode = 2;
	} else {
		fprintf(stderr, "Error: modo debe ser 'arealight', 'solidangle' o 'coshemi'\n");
		return 1;
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
				Color sample;
				if (light_sampling_mode == 2) {
					// Modo coshemi
					sample = shade_coshemi(camRay, xi);
				} else {
					// Modos arealight y solidangle
					sample = shade(camRay, xi);
				}
				pixelValue = pixelValue + sample;
			}
			pixelValue = pixelValue * (1.0 / spp);

			pixelColors[idx] = Color(clamp(pixelValue.x), clamp(pixelValue.y), clamp(pixelValue.z));
		}
	}

	fprintf(stderr,"\n");

	FILE *f = fopen(output_file, "w");
	fprintf(f, "P3\n%d %d\n%d\n", w, h, 255);
	for (int p = 0; p < w * h; p++) {
		fprintf(f,"%d %d %d ", toDisplayValue(pixelColors[p].x), toDisplayValue(pixelColors[p].y),
				toDisplayValue(pixelColors[p].z));
	}
	fclose(f);

	delete[] pixelColors;

	return 0;
}
