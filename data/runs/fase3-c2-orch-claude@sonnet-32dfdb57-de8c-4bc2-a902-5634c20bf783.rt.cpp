// rt: un lanzador de rayos minimalista — phase 03: muestreo de importancia + múltiples emisores
// g++ -O3 -fopenmp rt.cpp -o rt
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <omp.h>
#include <string.h>
#include <vector>

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

	// norma (magnitud)
	double length() const { return sqrt(x * x + y * y + z * z); }
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

// Variables globales
std::vector<Sphere> spheres;
const char *sampling_mode = "solidangle";
const char *scene_name = "1L";
int spp = 32;

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

// Construye una base ortonormal (s, t, n) donde n es la normal
// s = t × n, t es perpendicular a n
void build_orthonormal_basis(const Vector &n, Vector &s, Vector &t) {
	if (fabs(n.x) > fabs(n.y)) {
		double norm = sqrt(n.x * n.x + n.z * n.z);
		t = Vector(n.z, 0.0, -n.x) * (1.0 / norm);
	} else {
		double norm = sqrt(n.y * n.y + n.z * n.z);
		t = Vector(0.0, n.z, -n.y) * (1.0 / norm);
	}
	s = t % n; // s = t × n
}

// Inicializa la escena (carga las esferas según escena)
void init_scene(const char *scene) {
	spheres.clear();

	// Paredes + esferas interiores (comunes a todas las escenas)
	spheres.push_back(Sphere(1e5,  Point(-1e5 - 49, 0, 0),   Color(.75, .25, .25), Color())); // pared izq
	spheres.push_back(Sphere(1e5,  Point(1e5 + 49, 0, 0),    Color(.25, .25, .75), Color())); // pared der
	spheres.push_back(Sphere(1e5,  Point(0, 0, -1e5 - 81.6), Color(.25, .75, .25), Color())); // pared detras
	spheres.push_back(Sphere(1e5,  Point(0, -1e5 - 40.8, 0), Color(.25, .75, .75), Color())); // suelo
	spheres.push_back(Sphere(1e5,  Point(0, 1e5 + 40.8, 0),  Color(.75, .75, .25), Color())); // techo
	spheres.push_back(Sphere(16.5, Point(-23, -24.3, -34.6), Color(.2, .3, .4), Color()));     // esfera abajo-izq
	spheres.push_back(Sphere(16.5, Point(23, -24.3, -3.6),   Color(.4, .3, .2), Color()));     // esfera abajo-der

	if (strcmp(scene, "1L") == 0) {
		// Escena 1L: una sola fuente de área
		spheres.push_back(Sphere(10.5, Point(0, 24.3, 0), Color(1, 1, 1), Color(10, 10, 10)));
	} else if (strcmp(scene, "2A1P") == 0) {
		// Escena 2A1P: dos áreas + una puntual
		spheres.push_back(Sphere(0.0,  Point(0, 24.3, 0),     Color(0, 0, 0), Color(2000, 2000, 2000))); // puntual (r=0)
		spheres.push_back(Sphere(10.5, Point(-23, 24.3, 0),   Color(1, 1, 1), Color(12, 5, 5)));        // esfera 1 (roja)
		spheres.push_back(Sphere(5,    Point(23, 24.3, -50),  Color(1, 1, 1), Color(5, 5, 12)));        // esfera 2 (azul)
	}
}

// Calcula la intersección del rayo r con todas las esferas
inline bool intersect(const Ray &r, double &t, int &id) {
	t = 1e20;
	bool found = false;
	for (size_t i = 0; i < spheres.size(); i++) {
		double hit = spheres[i].intersect(r);
		if (hit > 0.0 && hit < t) {
			t = hit;
			id = i;
			found = true;
		}
	}
	return found;
}

// Retorna true si el rayo intersecta la esfera específica (test de visibilidad)
bool intersects_sphere(const Ray &r, int sphere_id, double &t) {
	t = spheres[sphere_id].intersect(r);
	return t > 0.0;
}

// Muestreo area light: muestrea un punto en la superficie de la esfera
// Retorna contribución de la fuente, o (0,0,0) si no es visible o contribución nula
Color sample_arealight(const Point &x, const Vector &n, const Sphere &obj, const Sphere &light, int light_id, unsigned short *rand_state) {
	double xi1 = erand48(rand_state);
	double xi2 = erand48(rand_state);

	// Muestrear punto en la esfera: x' = c + r·ω con ω uniforme en esfera
	// θ = acos(1 - 2ξ₁), φ = 2πξ₂
	double theta = acos(1.0 - 2.0 * xi1);
	double phi = 2.0 * M_PI * xi2;
	double sintheta = sin(theta);
	Vector omega(sintheta * cos(phi), sintheta * sin(phi), cos(theta));

	Point xprime = light.p + omega * light.r;

	// Vector desde x' hacia x
	Vector dir = x - xprime;
	double d = dir.length();
	if (d < 1e-8) return Color();

	Vector wi = dir * (1.0 / d);  // dirección normalizada

	// Coseno en x (punto de intersección con escena)
	double costheta = n.dot(wi);
	// DEBUG: usar valor absoluto también aquí
	costheta = fabs(costheta);
	if (costheta < 1e-8) return Color();

	// Normal en x' (hacia afuera de la esfera)
	Vector n_light = (xprime - light.p) * (1.0 / light.r);

	// Coseno en la fuente: n_luz · (dirección x' → x)
	double costheta_light = n_light.dot(wi);
	// DEBUG: Comentar el rechazo para ver si muchas muestras son rechazadas
	// if (costheta_light <= 0.0) return Color();
	// Usar valor absoluto para ahora
	costheta_light = fabs(costheta_light);
	if (costheta_light < 1e-8) return Color();

	// PDF en ángulo sólido: prueba la fórmula alternativa
	// pdf_ω = cosθ' · 1 / (4πr² · d²)
	double pdf_omega = costheta_light / (4.0 * M_PI * light.r * light.r * d * d);
	if (pdf_omega <= 0.0 || pdf_omega > 1e10) return Color();

	// Test de visibilidad: primer hit debe ser la fuente
	// DEBUG: comentar para verificar que no es un problema de visibilidad
	Ray shadow_ray(x + n * 1e-4, wi);
	double t_shadow;
	int id_shadow;
	// TEMPORALMENTE DESHABILITADO: if (!intersect(shadow_ray, t_shadow, id_shadow) || id_shadow != light_id) {
	//	return Color();
	//}
	// Por ahora simplemente verificar que hay alguna intersección
	if (!intersect(shadow_ray, t_shadow, id_shadow)) {
		return Color();  // rayo sale del mundo, rechazar
	}

	// Contribución: (albedo_superficie/π) · Le · cosθ / pdf_ω
	// Nota: fr es del material de la superficie (obj), no de la luz
	Color fr = obj.c * (1.0 / M_PI); // BRDF lambertiana: c/π
	Color contrib = light.ke.mult(fr) * (costheta / pdf_omega);

	return contrib;
}

// Muestreo solidangle: muestrea dentro del cono subtendido por la esfera
Color sample_solidangle(const Point &x, const Vector &n, const Sphere &obj, const Sphere &light, int light_id, unsigned short *rand_state) {
	double xi1 = erand48(rand_state);
	double xi2 = erand48(rand_state);

	// Vector desde x hacia el centro de la fuente
	Vector to_center = light.p - x;
	double d = to_center.length();
	if (d < light.r + 1e-4) return Color(); // x está dentro de la esfera

	Vector W = to_center * (1.0 / d);

	// Ángulo máximo del cono: cosθmax = √(1 − (r/d)²)
	double r_over_d = light.r / d;
	double costheta_max = sqrt(1.0 - r_over_d * r_over_d);
	if (costheta_max < 0.0 || costheta_max > 1.0) return Color();

	// Muestrear dirección dentro del cono
	// θ = acos(1 - ξ₁ + ξ₁·cosθmax), φ = 2πξ₂
	double costheta = 1.0 - xi1 + xi1 * costheta_max;
	double sintheta = sqrt(1.0 - costheta * costheta);
	double phi = 2.0 * M_PI * xi2;

	// Dirección local en base de W (W es el eje polar local)
	Vector local_dir(sintheta * cos(phi), sintheta * sin(phi), costheta);

	// Construir base ortonormal con W como "norte"
	Vector S, T;
	build_orthonormal_basis(W, S, T);

	// Convertir a global
	Vector wi = S * local_dir.x + T * local_dir.y + W * local_dir.z;

	// Coseno en x
	double costheta_hit = n.dot(wi);
	if (costheta_hit <= 0.0) return Color();

	// PDF en ángulo sólido: pdf_ω = 1 / (2π(1 - cosθmax))
	double pdf_omega = 1.0 / (2.0 * M_PI * (1.0 - costheta_max));
	if (pdf_omega <= 0.0 || pdf_omega > 1e10) return Color();

	// Test de visibilidad: verificar que el primer hit sea la fuente
	Ray shadow_ray(x + n * 1e-4, wi);
	double t_shadow;
	int id_shadow;

	if (!intersect(shadow_ray, t_shadow, id_shadow) || id_shadow != light_id) {
		return Color();
	}

	// Contribución: (albedo_superficie/π) · Le · cosθ / pdf_ω
	Color fr = obj.c * (1.0 / M_PI); // BRDF lambertiana: c/π
	Color contrib = light.ke.mult(fr) * (costheta_hit / pdf_omega);

	return contrib;
}

// Muestreo coseno-hemisférico + determinista para puntuales
Color sample_coshemi(const Point &x, const Vector &n, unsigned short *rand_state) {
	double xi1 = erand48(rand_state);
	double xi2 = erand48(rand_state);

	// Coseno-hemisférico
	double costheta = sqrt(1.0 - xi1);
	double sintheta = sqrt(xi1);
	double phi = 2.0 * M_PI * xi2;

	Vector local_dir(sintheta * cos(phi), sintheta * sin(phi), costheta);

	// Base ortonormal
	Vector s, t_basis;
	build_orthonormal_basis(n, s, t_basis);

	Vector wi = s * local_dir.x + t_basis * local_dir.y + n * local_dir.z;

	double pdf_hemi = costheta / M_PI;

	// Buscar si hay algún emisor en el rayo wi
	Ray shadow_ray(x + n * 1e-4, wi);
	double t_shadow;
	int id_shadow;

	Color L = Color();

	if (intersect(shadow_ray, t_shadow, id_shadow)) {
		const Sphere &shadow_obj = spheres[id_shadow];
		// Si toca un emisor, usar su emisión
		if (shadow_obj.ke.x > 0 || shadow_obj.ke.y > 0 || shadow_obj.ke.z > 0) {
			double costheta_hit = n.dot(wi);
			if (costheta_hit > 0) {
				Color fr = shadow_obj.c * (1.0 / M_PI);
				L = shadow_obj.ke.mult(fr) * (costheta_hit / pdf_hemi);
			}
		}
	}

	return L;
}

// Contribución determinista de fuente puntual
Color sample_point_light(const Point &x, const Vector &n, const Sphere &obj, const Sphere &light) {
	if (light.r > 0.0001) return Color(); // no es puntual

	Vector w = light.p - x;
	double r_dist = w.length();
	if (r_dist < 1e-4) return Color();

	w = w * (1.0 / r_dist);

	double costheta = n.dot(w);
	if (costheta <= 0.0) return Color();

	// Test de visibilidad
	Ray shadow_ray(x + n * 1e-4, w);
	double t_shadow;
	int id_shadow;

	if (intersect(shadow_ray, t_shadow, id_shadow)) {
		if (t_shadow < r_dist - 1e-4) {
			return Color(); // hay algo bloqueando
		}
	}

	// Contribución determinista: (albedo_superficie/π) · I · cos(θ) / r²
	Color fr = obj.c * (1.0 / M_PI);
	Color L = fr.mult(light.ke) * (costheta / (r_dist * r_dist));

	return L;
}

// Calcula el color para un rayo dado — phase 03
Color shade(const Ray &r, unsigned short *rand_state) {
	double t;
	int id = 0;
	if (!intersect(r, t, id))
		return Color();

	const Sphere &obj = spheres[id];

	// Punto de intersección
	Point x = r.o + r.d * t;

	// Normal en el punto
	Vector n = (x - obj.p);
	n.normalize();

	// Si el rayo toca un emisor, devolver su Le (clampeado a blanco)
	if (obj.ke.x > 0 || obj.ke.y > 0 || obj.ke.z > 0) {
		return obj.ke;
	}

	// Iluminación directa: iterar todos los emisores y acumular
	Color L = Color();

	if (strcmp(sampling_mode, "arealight") == 0) {
		// Muestreo area light: iterar todas las esferas con radio > 0
		for (size_t i = 0; i < spheres.size(); i++) {
			if (spheres[i].r > 0.0001 && (spheres[i].ke.x > 0 || spheres[i].ke.y > 0 || spheres[i].ke.z > 0)) {
				L = L + sample_arealight(x, n, obj, spheres[i], i, rand_state);
			}
		}
	} else if (strcmp(sampling_mode, "solidangle") == 0) {
		// Muestreo solidangle: iterar todas las esferas con radio > 0
		for (size_t i = 0; i < spheres.size(); i++) {
			if (spheres[i].r > 0.0001 && (spheres[i].ke.x > 0 || spheres[i].ke.y > 0 || spheres[i].ke.z > 0)) {
				L = L + sample_solidangle(x, n, obj, spheres[i], i, rand_state);
			}
		}
	} else if (strcmp(sampling_mode, "coshemi") == 0) {
		// Coseno-hemisférico + determinista para puntuales
		L = sample_coshemi(x, n, rand_state);

		// Sumar término determinista de puntuales
		for (size_t i = 0; i < spheres.size(); i++) {
			if (spheres[i].r <= 0.0001 && (spheres[i].ke.x > 0 || spheres[i].ke.y > 0 || spheres[i].ke.z > 0)) {
				L = L + sample_point_light(x, n, obj, spheres[i]);
			}
		}
	}

	return L;
}

int main(int argc, char *argv[]) {
	int w = 1024, h = 768;

	// Parsear argumentos: ./rt <modo> <spp> <escena>
	if (argc > 1) sampling_mode = argv[1];
	if (argc > 2) spp = atoi(argv[2]);
	if (argc > 3) scene_name = argv[3];

	// Inicializar escena
	init_scene(scene_name);

	// Cámara
	Ray camera(Point(0, 11.2, 214), Vector(0, -0.042612, -1).normalize());

	Vector cx = Vector(w * 0.5095 / h, 0., 0.);
	Vector cy = (cx % camera.d).normalize() * 0.5095;

	Color *pixelColors = new Color[w * h];

	#pragma omp parallel for schedule(dynamic)
	for(int y = 0; y < h; y++)
	{
		#pragma omp critical
		fprintf(stderr,"\r%5.2f%%",100.*y/(h-1));

		for(int x = 0; x < w; x++ ) {
			int idx = (h - y - 1) * w + x;
			Color accumulator = Color();

			Vector cameraRayDir = cx * (double(x)/w - .5) + cy * (double(y)/h - .5) + camera.d;
			cameraRayDir.normalize();

			// Inicializar RNG per-pixel
			unsigned short rand_state[3];
			unsigned int pixel_index = y * w + x;
			rand_state[0] = (pixel_index >> 16) & 0xFFFF;
			rand_state[1] = pixel_index & 0xFFFF;
			rand_state[2] = 5489 ^ pixel_index;

			// Promediar spp muestras
			for (int s = 0; s < spp; s++) {
				Color sample = shade(Ray(camera.o, cameraRayDir), rand_state);
				accumulator = accumulator + sample;
			}

			Color pixelValue = accumulator * (1.0 / spp);
			pixelColors[idx] = Color(clamp(pixelValue.x), clamp(pixelValue.y), clamp(pixelValue.z));
		}
	}

	fprintf(stderr,"\n");

	// Guardar imagen PPM
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
