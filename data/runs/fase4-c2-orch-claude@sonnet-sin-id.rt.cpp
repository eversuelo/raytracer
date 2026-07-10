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

// Fase 03: Estructura de escenas y soporte de múltiples emisores
const int MAX_SPHERES = 10;
const int MAX_SCENES = 2;

// Escena 1L: una esfera emisora (la de fase 1)
Sphere scene_1L[] = {
	Sphere(1e5,  Point(-1e5 - 49, 0, 0),   Color(.75, .25, .25), Color()), // pared izq
	Sphere(1e5,  Point(1e5 + 49, 0, 0),    Color(.25, .25, .75), Color()), // pared der
	Sphere(1e5,  Point(0, 0, -1e5 - 81.6), Color(.25, .75, .25), Color()), // pared detras
	Sphere(1e5,  Point(0, -1e5 - 40.8, 0), Color(.25, .75, .75), Color()), // suelo
	Sphere(1e5,  Point(0, 1e5 + 40.8, 0),  Color(.75, .75, .25), Color()), // techo
	Sphere(16.5, Point(-23, -24.3, -34.6), Color(.2, .3, .4), Color()),     // esfera abajo-izq
	Sphere(16.5, Point(23, -24.3, -3.6),   Color(.4, .3, .2), Color()),     // esfera abajo-der
	Sphere(10.5, Point(0, 24.3, 0),        Color(1, 1, 1), Color(10, 10, 10)) // emisor único
};
const int scene_1L_count = 8;

// Escena 2A1P: dos esferas emisoras + una fuente puntual (r=0)
Sphere scene_2A1P[] = {
	Sphere(1e5,  Point(-1e5 - 49, 0, 0),   Color(.75, .25, .25), Color()), // pared izq
	Sphere(1e5,  Point(1e5 + 49, 0, 0),    Color(.25, .25, .75), Color()), // pared der
	Sphere(1e5,  Point(0, 0, -1e5 - 81.6), Color(.25, .75, .25), Color()), // pared detras
	Sphere(1e5,  Point(0, -1e5 - 40.8, 0), Color(.25, .75, .75), Color()), // suelo
	Sphere(1e5,  Point(0, 1e5 + 40.8, 0),  Color(.75, .75, .25), Color()), // techo
	Sphere(16.5, Point(-23, -24.3, -34.6), Color(.2, .3, .4), Color()),     // esfera abajo-izq
	Sphere(16.5, Point(23, -24.3, -3.6),   Color(.4, .3, .2), Color()),     // esfera abajo-der
	Sphere(0.0,  Point(0, 24.3, 0),        Color(0, 0, 0),      Color(2000, 2000, 2000)), // puntual
	Sphere(10.5, Point(-23, 24.3, 0),      Color(1, 1, 1),      Color(12, 5, 5)),        // emisor 1
	Sphere(5.0,  Point(23, 24.3, -50),     Color(1, 1, 1),      Color(5, 5, 12))         // emisor 2
};
const int scene_2A1P_count = 10;

// Puntero global a escena actual
Sphere *current_scene = scene_1L;
int current_scene_count = scene_1L_count;

// Rayos índices de emisores por escena
int emitters_1L[] = {7};
int emitters_1L_count = 1;
int emitters_2A1P[] = {7, 8, 9};
int emitters_2A1P_count = 3;
int *current_emitters = emitters_1L;
int *current_emitters_count = &emitters_1L_count;

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

// Calcula intersección con la escena actual
inline bool intersect(const Ray &r, double &t, int &id) {
	t = 1e20;
	bool found = false;
	for (int i = 0; i < current_scene_count; i++) {
		double hit = current_scene[i].intersect(r);
		if (hit > 0.0 && hit < t) {
			t = hit;
			id = i;
			found = true;
		}
	}
	return found;
}

// Variables globales para el muestreador, spp y modo
const char *sampler_name = "uniformsphere";
int spp = 32;
const char *current_mode = "coshemi"; // "arealight", "solidangle" o "coshemi"

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

// Fase 03: Muestreo de importancia de fuentes esféricas

// sample_area_light: muestrea un punto uniforme en la superficie de una esfera
// Entrada: esfera, dos uniformes xi1, xi2 en [0,1)
// Salida: punto muestreado en la superficie, pdf en ángulo sólido
void sample_area_light(const Sphere &light, double xi1, double xi2,
                       Point &x_light, double &pdf_omega) {
	// Muestreo uniforme en esfera: θ = acos(1 - 2*ξ1), φ = 2π*ξ2
	double costheta = 1.0 - 2.0 * xi1;
	double sintheta = sqrt(1.0 - costheta * costheta);
	double phi = 2.0 * M_PI * xi2;

	Vector direction(sintheta * cos(phi), sintheta * sin(phi), costheta);
	x_light = light.p + direction * light.r;

	// pdf_omega = d^2 / (4π r^2 cos θ'), donde:
	// d = |x' - x|, cos θ' = n_luz · (dirección x' -> x)
	// Será calculado en shade()
	pdf_omega = 1.0; // placeholder, se recalcula con la geometría
}

// sample_solid_angle: muestrea direcciones dentro del cono subtendido por una esfera
// Entrada: esfera, punto de incidencia, dos uniformes xi1, xi2
// Salida: dirección muestreada, pdf en ángulo sólido
void sample_solid_angle(const Sphere &light, const Point &x, double xi1, double xi2,
                        Vector &direction, double &pdf_omega) {
	Vector x_to_c = light.p - x;
	double dist = sqrt(x_to_c.dot(x_to_c));
	Vector W = x_to_c * (1.0 / dist); // normalizar (eje del cono hacia la fuente)

	double cos_theta_max = sqrt(1.0 - (light.r / dist) * (light.r / dist));
	double cos_theta = 1.0 - xi1 * (1.0 - cos_theta_max);
	double sin_theta = sqrt(1.0 - cos_theta * cos_theta);
	double phi = 2.0 * M_PI * xi2;

	// Construir base local de W
	Vector W_basis_u, W_basis_v;
	build_orthonormal_basis(W, W_basis_u, W_basis_v);

	// Dirección en base local
	Vector local_dir(sin_theta * cos(phi), sin_theta * sin(phi), cos_theta);
	direction = W_basis_u * local_dir.x + W_basis_v * local_dir.y + W * local_dir.z;

	pdf_omega = 1.0 / (2.0 * M_PI * (1.0 - cos_theta_max));
}

// Calcula el valor de color para el rayo dado (una muestra) — fase 03 (múltiples emisores + IS)
Color shade(const Ray &r, unsigned short *rand_state) {
	double t;
	int id = 0;
	// determinar que esfera (id) y a que distancia (t) el rayo intersecta
	if (!intersect(r, t, id))
		return Color(); // el rayo no intersecto objeto, return Color() == negro

	const Sphere &obj = current_scene[id];

	// determinar coordenadas del punto de interseccion
	Point x = r.o + r.d * t;

	// determinar la dirección normal en el punto de interseccion
	Vector n = (x - obj.p);
	n.normalize();

	// Si el objeto es un emisor (ke ≠ 0), devolver su emisión
	if (obj.ke.x > 0 || obj.ke.y > 0 || obj.ke.z > 0) {
		return obj.ke;
	}

	// Iluminación directa: iterar TODOS los emisores
	Color L_total = Color();

	for (int emitter_idx = 0; emitter_idx < *current_emitters_count; emitter_idx++) {
		int light_id = current_emitters[emitter_idx];
		const Sphere &light = current_scene[light_id];

		// Caso 1: Fuente puntual (r=0)
		if (light.r < 1e-6) {
			// Término determinista de fase 2
			Vector w = light.p - x;
			double r_dist = sqrt(w.dot(w)); // distancia
			w = w * (1.0 / r_dist); // normalizar

			double costheta = n.dot(w);
			if (costheta > 0.0) {
				// Lanzar rayo de sombra
				Ray shadow_ray(x + n * 1e-4, w);
				double t_shadow;
				int id_shadow;
				if (intersect(shadow_ray, t_shadow, id_shadow)) {
					// Verificar visibilidad
					if (t_shadow >= r_dist - 1e-4) {
						// Visible
						Color fr = obj.c * (1.0 / M_PI);
						Color contrib = fr.mult(light.ke) * (costheta / (r_dist * r_dist));
						L_total = L_total + contrib;
					}
				}
			}
		}
		// Caso 2: Fuentes esféricas (r > 0)
		else if (strcmp(current_mode, "arealight") == 0) {
			// Muestreo de área
			double xi1 = erand48(rand_state);
			double xi2 = erand48(rand_state);

			Point x_light;
			double pdf_dummy;
			sample_area_light(light, xi1, xi2, x_light, pdf_dummy);

			Vector to_light = x_light - x;
			double d = sqrt(to_light.dot(to_light));
			Vector w = to_light * (1.0 / d); // normalizar

			double costheta = n.dot(w);
			if (costheta > 0.0) {
				// Calcular cosθ' = n_luz · (dirección x' → x)
				Vector n_light = (x_light - light.p);
				n_light.normalize();
				Vector to_x = x - x_light;
				double dist_to_x = sqrt(to_x.dot(to_x));
				Vector dir_to_x = to_x * (1.0 / dist_to_x);
				double costheta_light = n_light.dot(dir_to_x);

				if (costheta_light > 0.0) {
					// Calcular pdf_omega
					double pdf_omega = (d * d) / (4.0 * M_PI * light.r * light.r * costheta_light);

					// Lanzar rayo de sombra
					Ray shadow_ray(x + n * 1e-4, w);
					double t_shadow;
					int id_shadow;
					if (intersect(shadow_ray, t_shadow, id_shadow)) {
						// Verificar si el primer hit es la fuente
						if (id_shadow == light_id) {
							// Visible y es la fuente correcta
							Color fr = obj.c * (1.0 / M_PI);
							Color contrib = fr.mult(light.ke) * (costheta / pdf_omega);
							L_total = L_total + contrib;
						}
					}
				}
			}
		}
		else if (strcmp(current_mode, "solidangle") == 0) {
			// Muestreo del ángulo sólido
			double xi1 = erand48(rand_state);
			double xi2 = erand48(rand_state);

			Vector w;
			double pdf_omega;
			sample_solid_angle(light, x, xi1, xi2, w, pdf_omega);

			double costheta = n.dot(w);
			if (costheta > 0.0) {
				// Lanzar rayo de sombra
				Ray shadow_ray(x + n * 1e-4, w);
				double t_shadow;
				int id_shadow;
				if (intersect(shadow_ray, t_shadow, id_shadow)) {
					// Verificar si el primer hit es la fuente
					if (id_shadow == light_id) {
						// Visible
						Color fr = obj.c * (1.0 / M_PI);
						Color contrib = fr.mult(light.ke) * (costheta / pdf_omega);
						L_total = L_total + contrib;
					}
				}
			}
		}
		else if (strcmp(current_mode, "coshemi") == 0) {
			// Modo coshemi: ignorar fuentes esféricas, solo puntuales
			// (Para puntuales ya manejado arriba)
		}
	}

	// Para coshemi: agregar término hemisférico
	if (strcmp(current_mode, "coshemi") == 0) {
		double xi1 = erand48(rand_state);
		double xi2 = erand48(rand_state);

		Vector local_dir;
		double pdf;
		sample_direction("cosinehemi", xi1, xi2, local_dir, pdf);

		// Convertir de local a global usando base ortonormal
		Vector s, t_basis;
		build_orthonormal_basis(n, s, t_basis);

		Vector wi = s * local_dir.x + t_basis * local_dir.y + n * local_dir.z;

		// Lanzar rayo de sombra desde x
		Ray shadow_ray(x + n * 1e-4, wi);
		double t_shadow;
		int id_shadow;

		if (intersect(shadow_ray, t_shadow, id_shadow)) {
			const Sphere &shadow_obj = current_scene[id_shadow];
			// Si toca un emisor (esférico), usar su emisión
			if (shadow_obj.ke.x > 0 || shadow_obj.ke.y > 0 || shadow_obj.ke.z > 0) {
				double costheta = n.dot(wi);
				if (costheta > 0) {
					Color fr = obj.c * (1.0 / M_PI);
					L_total = L_total + shadow_obj.ke.mult(fr) * (costheta / pdf);
				}
			}
		}
	}

	return L_total;
}

int main(int argc, char *argv[]) {
	int w = 1024, h = 768; // image resolution
	const char *output_file = "image.ppm"; // Siempre genera image.ppm; check.sh lo copia al destino

	// Parsear argumentos: <modo> <spp> <escena>
	// modo: arealight, solidangle, coshemi
	// spp: entero
	// escena: 1L o 2A1P
	if (argc < 4) {
		fprintf(stderr, "Usage: %s <modo> <spp> <escena>\n", argv[0]);
		fprintf(stderr, "  modo: arealight, solidangle, coshemi\n");
		fprintf(stderr, "  spp: número de muestras por píxel\n");
		fprintf(stderr, "  escena: 1L o 2A1P\n");
		return 1;
	}

	current_mode = argv[1]; // arealight, solidangle, coshemi
	spp = atoi(argv[2]);
	const char *scene_name = argv[3]; // 1L o 2A1P

	// Configurar escena
	if (strcmp(scene_name, "1L") == 0) {
		current_scene = scene_1L;
		current_scene_count = scene_1L_count;
		current_emitters = emitters_1L;
		current_emitters_count = &emitters_1L_count;
	} else if (strcmp(scene_name, "2A1P") == 0) {
		current_scene = scene_2A1P;
		current_scene_count = scene_2A1P_count;
		current_emitters = emitters_2A1P;
		current_emitters_count = &emitters_2A1P_count;
	} else {
		fprintf(stderr, "Escena desconocida: %s (use 1L o 2A1P)\n", scene_name);
		return 1;
	}

	// Validar modo
	if (strcmp(current_mode, "arealight") != 0 &&
	    strcmp(current_mode, "solidangle") != 0 &&
	    strcmp(current_mode, "coshemi") != 0) {
		fprintf(stderr, "Modo desconocido: %s\n", current_mode);
		return 1;
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

			// Modo Monte Carlo: con spp muestras
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

	return 0;
}
