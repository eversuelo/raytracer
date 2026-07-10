// rt: un lanzador de rayos minimalista
 // g++ -O3 -fopenmp rt.cpp -o rt
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <omp.h>
#include <string.h>
#include <algorithm>

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

// Forward declarations
void build_orthonormal_basis(const Vector &n, Vector &s, Vector &t);

// Material API
struct MaterialSample { Vector wi; double pdf; };
struct MaterialEval { Color fr; };

class Material {
public:
	virtual ~Material() {}
	virtual MaterialSample sample(const Vector &n, const Vector &wo, unsigned short *rand_state) = 0;
	virtual MaterialEval eval(const Vector &n, const Vector &wo, const Vector &wi) = 0;
	virtual double pdf(const Vector &n, const Vector &wo, const Vector &wi) = 0;
	virtual bool is_diffuse() const = 0;
};

class DiffuseMaterial : public Material {
public:
	Color albedo;
	DiffuseMaterial(Color c) : albedo(c) {}

	MaterialSample sample(const Vector &n, const Vector &wo, unsigned short *rand_state) override {
		double xi1 = erand48(rand_state);
		double xi2 = erand48(rand_state);

		double costheta = sqrt(1.0 - xi1);
		double sintheta = sqrt(xi1);
		double phi = 2.0 * M_PI * xi2;

		Vector s, t;
		build_orthonormal_basis(n, s, t);
		Vector wi = s * (sintheta * cos(phi)) + t * (sintheta * sin(phi)) + n * costheta;

		MaterialSample result;
		result.wi = wi;
		result.pdf = costheta / M_PI;
		return result;
	}

	MaterialEval eval(const Vector &n, const Vector &wo, const Vector &wi) override {
		double ndotl = n.dot(wi);
		MaterialEval result;
		if (ndotl > 0.0) {
			result.fr = albedo * (1.0 / M_PI);
		} else {
			result.fr = Color();
		}
		return result;
	}

	double pdf(const Vector &n, const Vector &wo, const Vector &wi) override {
		double costheta = n.dot(wi);
		if (costheta > 0.0) {
			return costheta / M_PI;
		}
		return 0.0;
	}

	bool is_diffuse() const override { return true; }
};

class ConductorMaterial : public Material {
public:
	double alpha;
	Color eta, k; // Fresnel parameters per channel

	ConductorMaterial(double a, Color e, Color k_) : alpha(a), eta(e), k(k_) {}

	double beckmann_D(double costheta_h) const {
		if (costheta_h <= 0.0) return 0.0;
		double alpha2 = alpha * alpha;
		double costheta2 = costheta_h * costheta_h;
		double tantheta2 = (1.0 - costheta2) / costheta2;
		double exp_term = exp(-tantheta2 / alpha2);
		return exp_term / (M_PI * alpha2 * costheta2 * costheta2);
	}

	double smith_G1(double v_dot_wh, double v_dot_n) const {
		if (v_dot_wh / v_dot_n <= 0.0) return 0.0;

		double costheta_v = v_dot_n;
		double sintheta_v = sqrt(std::max(0.0, 1.0 - costheta_v * costheta_v));
		if (sintheta_v < 1e-6) return 1.0;

		double tantheta_v = sintheta_v / costheta_v;
		double a = 1.0 / (alpha * tantheta_v);

		if (a < 1.6) {
			double a2 = a * a;
			return (3.535 * a + 2.181 * a2) / (1.0 + 2.276 * a + 2.577 * a2);
		}
		return 1.0;
	}

	double fresnel_conductor(double costheta_i, int channel) const {
		double eta_ch = (channel == 0) ? eta.x : (channel == 1) ? eta.y : eta.z;
		double k_ch = (channel == 0) ? k.x : (channel == 1) ? k.y : k.z;

		double costheta_i_sq = costheta_i * costheta_i;
		double sintheta_i_sq = std::max(0.0, 1.0 - costheta_i_sq);

		double eta2 = eta_ch * eta_ch;
		double k2 = k_ch * k_ch;
		double eta2_minus_k2_minus_sin2 = eta2 - k2 - sintheta_i_sq;
		double a2b2_sq = eta2_minus_k2_minus_sin2 * eta2_minus_k2_minus_sin2 + 4.0 * eta2 * k2;
		double a2b2 = sqrt(std::max(0.0, a2b2_sq));

		double A_inner = a2b2 + eta2 - k2 - sintheta_i_sq;
		double A = sqrt(std::max(0.0, A_inner / 2.0));

		double num_perp = a2b2 + costheta_i_sq - 2.0 * A * costheta_i;
		double den_perp = a2b2 + costheta_i_sq + 2.0 * A * costheta_i;
		double R_perp = std::max(0.0, std::min(1.0, num_perp / std::max(1e-6, den_perp)));

		double num_par = R_perp * (a2b2 * costheta_i_sq + sintheta_i_sq * sintheta_i_sq - 2.0 * A * costheta_i * sintheta_i_sq);
		double den_par = a2b2 * costheta_i_sq + sintheta_i_sq * sintheta_i_sq + 2.0 * A * costheta_i * sintheta_i_sq;
		double R_par = (den_par > 1e-6) ? std::max(0.0, std::min(1.0, num_par / den_par)) : R_perp;

		double F = (R_perp + R_par) / 2.0;
		return std::max(0.0, std::min(1.0, F));
	}

	MaterialSample sample(const Vector &n, const Vector &wo, unsigned short *rand_state) override {
		double xi1 = erand48(rand_state);
		double xi2 = erand48(rand_state);

		double alpha2 = alpha * alpha;
		double tantheta_h_sq = -alpha2 * log(std::max(1e-6, 1.0 - xi1));
		double tantheta_h = sqrt(std::max(0.0, tantheta_h_sq));

		// cos(θh) = 1 / √(1 + tan²θh)
		double costheta_h = 1.0 / sqrt(1.0 + tantheta_h_sq);
		double sintheta_h = tantheta_h * costheta_h;

		double phi_h = 2.0 * M_PI * xi2;

		Vector s, t;
		build_orthonormal_basis(n, s, t);
		Vector wh = s * (sintheta_h * cos(phi_h)) + t * (sintheta_h * sin(phi_h)) + n * costheta_h;
		wh.normalize();

		double wo_dot_wh = wo.dot(wh);
		Vector wi = wh * (2.0 * wo_dot_wh) - wo;
		wi.normalize();

		MaterialSample result;
		result.wi = wi;

		double pdf_val = beckmann_D(costheta_h) * costheta_h / (4.0 * std::max(1e-6, fabs(wo_dot_wh)));
		result.pdf = pdf_val;

		return result;
	}

	MaterialEval eval(const Vector &n, const Vector &wo, const Vector &wi) override {
		double ndotl = n.dot(wi);
		double ndotv = n.dot(wo);

		MaterialEval result;
		result.fr = Color();

		if (ndotl <= 0.0 || ndotv <= 0.0) return result;

		Vector wh = (wo + wi);
		wh.normalize();

		double costheta_h = n.dot(wh);
		if (costheta_h <= 0.0) return result;

		double wo_dot_wh = wo.dot(wh);
		double wi_dot_wh = wi.dot(wh);

		double D = beckmann_D(costheta_h);
		double G = smith_G1(wo_dot_wh, ndotv) * smith_G1(wi_dot_wh, ndotl);
		double denom = 4.0 * ndotl * ndotv;
		if (denom < 1e-6) return result;

		// Fresnel uses the angle between wi (incoming ray) and wh (half-vector)
		double costheta_i = std::max(0.0, fabs(wi_dot_wh));
		double F_x = fresnel_conductor(costheta_i, 0);
		double F_y = fresnel_conductor(costheta_i, 1);
		double F_z = fresnel_conductor(costheta_i, 2);
		result.fr.x = std::max(0.0, D * G * F_x / denom);
		result.fr.y = std::max(0.0, D * G * F_y / denom);
		result.fr.z = std::max(0.0, D * G * F_z / denom);

		return result;
	}

	double pdf(const Vector &n, const Vector &wo, const Vector &wi) override {
		Vector wh = (wo + wi);
		wh.normalize();

		double costheta_h = n.dot(wh);
		if (costheta_h <= 0.0) return 0.0;

		double wo_dot_wh = wo.dot(wh);
		if (fabs(wo_dot_wh) < 1e-6) return 0.0;

		double D = beckmann_D(costheta_h);
		return D * costheta_h / (4.0 * fabs(wo_dot_wh));
	}

	bool is_diffuse() const override { return false; }
};

class Sphere
{
public:
	double r;	// radio de la esfera
	Point p;	// posicion
	Material *material;
	Color ke;	// emisión

	Sphere(double r_, Point p_, Material *mat, Color ke_ = Color()): r(r_), p(p_), material(mat), ke(ke_) {}

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

// Fase 04: Escenas con materiales microfacet para conductores
const int MAX_SPHERES = 10;
const int MAX_SCENES = 4;

// Materiales globales
DiffuseMaterial wall_left(Color(.75, .25, .25));
DiffuseMaterial wall_right(Color(.25, .25, .75));
DiffuseMaterial wall_back(Color(.25, .75, .25));
DiffuseMaterial wall_floor(Color(.25, .75, .75));
DiffuseMaterial wall_ceil(Color(.75, .75, .25));

DiffuseMaterial white_diffuse(Color(1, 1, 1));

// Conductores con α=0.3
ConductorMaterial aluminum(0.3,
	Color(1.66058, 0.88143, 0.521467),
	Color(9.2282, 6.27077, 4.83803));

ConductorMaterial gold(0.3,
	Color(0.143245, 0.377423, 1.43919),
	Color(3.98479, 2.3847, 1.60434));

// Escena "point": solo puntual
Sphere scene_point[] = {
	Sphere(1e5,  Point(-1e5 - 49, 0, 0),   &wall_left,     Color()), // pared izq
	Sphere(1e5,  Point(1e5 + 49, 0, 0),    &wall_right,    Color()), // pared der
	Sphere(1e5,  Point(0, 0, -1e5 - 81.6), &wall_back,     Color()), // pared detras
	Sphere(1e5,  Point(0, -1e5 - 40.8, 0), &wall_floor,    Color()), // suelo
	Sphere(1e5,  Point(0, 1e5 + 40.8, 0),  &wall_ceil,     Color()), // techo
	Sphere(16.5, Point(-23, -24.3, -34.6), &aluminum,      Color()), // conductor izq
	Sphere(16.5, Point(23, -24.3, -3.6),   &gold,          Color()), // conductor der
	Sphere(0.0,  Point(0, 24.3, 0),        &white_diffuse, Color(2000, 2000, 2000)) // puntual
};
const int scene_point_count = 8;

// Escena "areal": una esfera emisora
Sphere scene_areal[] = {
	Sphere(1e5,  Point(-1e5 - 49, 0, 0),   &wall_left,     Color()), // pared izq
	Sphere(1e5,  Point(1e5 + 49, 0, 0),    &wall_right,    Color()), // pared der
	Sphere(1e5,  Point(0, 0, -1e5 - 81.6), &wall_back,     Color()), // pared detras
	Sphere(1e5,  Point(0, -1e5 - 40.8, 0), &wall_floor,    Color()), // suelo
	Sphere(1e5,  Point(0, 1e5 + 40.8, 0),  &wall_ceil,     Color()), // techo
	Sphere(16.5, Point(-23, -24.3, -34.6), &aluminum,      Color()), // conductor izq
	Sphere(16.5, Point(23, -24.3, -3.6),   &gold,          Color()), // conductor der
	Sphere(10.5, Point(0, 24.3, 0),        &white_diffuse, Color(10, 10, 10)) // emisor único
};
const int scene_areal_count = 8;

// Escena "2a1p": dos emisores + puntual (fases anteriores)
Sphere scene_2a1p[] = {
	Sphere(1e5,  Point(-1e5 - 49, 0, 0),   &wall_left,     Color()), // pared izq
	Sphere(1e5,  Point(1e5 + 49, 0, 0),    &wall_right,    Color()), // pared der
	Sphere(1e5,  Point(0, 0, -1e5 - 81.6), &wall_back,     Color()), // pared detras
	Sphere(1e5,  Point(0, -1e5 - 40.8, 0), &wall_floor,    Color()), // suelo
	Sphere(1e5,  Point(0, 1e5 + 40.8, 0),  &wall_ceil,     Color()), // techo
	Sphere(16.5, Point(-23, -24.3, -34.6), &aluminum,      Color()), // conductor izq
	Sphere(16.5, Point(23, -24.3, -3.6),   &gold,          Color()), // conductor der
	Sphere(0.0,  Point(0, 24.3, 0),        &white_diffuse, Color(2000, 2000, 2000)), // puntual
	Sphere(10.5, Point(-23, 24.3, 0),      &white_diffuse, Color(12, 5, 5)),        // emisor 1
	Sphere(5.0,  Point(23, 24.3, -50),     &white_diffuse, Color(5, 5, 12))         // emisor 2
};
const int scene_2a1p_count = 10;

// Puntero global a escena actual
Sphere *current_scene = scene_point;
int current_scene_count = scene_point_count;

// Índices de emisores por escena
int emitters_point[] = {7};
int emitters_point_count = 1;
int emitters_areal[] = {7};
int emitters_areal_count = 1;
int emitters_2a1p[] = {7, 8, 9};
int emitters_2a1p_count = 3;
int *current_emitters = emitters_point;
int *current_emitters_count = &emitters_point_count;

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
int spp = 32;
const char *current_mode = "issa"; // "issa", "isarea", "isbrdf"

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

// Calcula el valor de color para el rayo dado (una muestra) — fase 04 (microfacet conductor)
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

	// Dirección de salida (hacia la cámara)
	Vector wo = r.d * (-1.0);
	wo.normalize();

	Color L_total = Color();

	// Modos ISSA e ISArea: muestrear TODAS las fuentes
	if (strcmp(current_mode, "issa") == 0 || strcmp(current_mode, "isarea") == 0) {
		for (int emitter_idx = 0; emitter_idx < *current_emitters_count; emitter_idx++) {
			int light_id = current_emitters[emitter_idx];
			const Sphere &light = current_scene[light_id];

			// Caso 1: Fuente puntual (r=0)
			if (light.r < 1e-6) {
				Vector wi = light.p - x;
				double d = sqrt(wi.dot(wi));
				wi = wi * (1.0 / d);

				double ndotl = n.dot(wi);
				if (ndotl > 0.0) {
					// Rayo de sombra
					Ray shadow_ray(x + n * 1e-4, wi);
					double t_shadow;
					int id_shadow;
					bool occluded = intersect(shadow_ray, t_shadow, id_shadow) && t_shadow < d - 1e-4;
					if (!occluded) {
						// Visible
						MaterialEval mat_eval = obj.material->eval(n, wo, wi);
						Color contrib = mat_eval.fr.mult(light.ke) * (ndotl / (d * d));
						L_total = L_total + contrib;
					}
				}
			}
			// Fuentes esféricas
			else {
				double xi1 = erand48(rand_state);
				double xi2 = erand48(rand_state);

				Vector wi;
				double pdf_omega;

				if (strcmp(current_mode, "issa") == 0) {
					// Muestreo de ángulo sólido
					sample_solid_angle(light, x, xi1, xi2, wi, pdf_omega);
				} else { // isarea
					// Muestreo de área
					Point x_light;
					sample_area_light(light, xi1, xi2, x_light, pdf_omega);

					Vector to_light = x_light - x;
					double d = sqrt(to_light.dot(to_light));
					wi = to_light * (1.0 / d);

					// Calcular pdf en ángulo sólido
					Vector n_light = (x_light - light.p);
					n_light.normalize();
					Vector to_x = x - x_light;
					double dist_to_x = sqrt(to_x.dot(to_x));
					Vector dir_to_x = to_x * (1.0 / dist_to_x);
					double costheta_light = n_light.dot(dir_to_x);

					if (costheta_light > 0.0) {
						pdf_omega = (d * d) / (4.0 * M_PI * light.r * light.r * costheta_light);
					} else {
						pdf_omega = 0.0;
					}
				}

				double ndotl = n.dot(wi);
				if (ndotl > 0.0 && pdf_omega > 1e-6) {
					// Rayo de sombra
					Ray shadow_ray(x + n * 1e-4, wi);
					double t_shadow;
					int id_shadow;
					bool occluded = intersect(shadow_ray, t_shadow, id_shadow) && id_shadow != light_id;
					if (!occluded) {
						// Visible
						MaterialEval mat_eval = obj.material->eval(n, wo, wi);
						Color contrib = mat_eval.fr.mult(light.ke) * (ndotl / pdf_omega);
						L_total = L_total + contrib;
					}
				}
			}
		}
	}
	// Modo ISBRDF: muestrear la BRDF (distribución D para conductores, coseno para difusos)
	else if (strcmp(current_mode, "isbrdf") == 0) {
		MaterialSample sample = obj.material->sample(n, wo, rand_state);
		Vector wi = sample.wi;
		double pdf = sample.pdf;

		if (pdf > 1e-6) {
			double ndotl = n.dot(wi);
			if (ndotl > 0.0) {
				// Lanzar rayo en la dirección muestreada
				Ray wi_ray(x + n * 1e-4, wi);
				double t_wi;
				int id_wi;
				if (intersect(wi_ray, t_wi, id_wi)) {
					const Sphere &hit_obj = current_scene[id_wi];
					// Solo suma si toca un emisor
					if (hit_obj.ke.x > 0 || hit_obj.ke.y > 0 || hit_obj.ke.z > 0) {
						MaterialEval mat_eval = obj.material->eval(n, wo, wi);
						Color contrib = mat_eval.fr.mult(hit_obj.ke) * (ndotl / pdf);
						L_total = L_total + contrib;
					}
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
	// modo: issa, isarea, isbrdf
	// spp: entero
	// escena: point, areal, 2a1p
	if (argc < 4) {
		fprintf(stderr, "Usage: %s <modo> <spp> <escena>\n", argv[0]);
		fprintf(stderr, "  modo: issa, isarea, isbrdf\n");
		fprintf(stderr, "  spp: número de muestras por píxel\n");
		fprintf(stderr, "  escena: point, areal, 2a1p\n");
		return 1;
	}

	current_mode = argv[1]; // issa, isarea, isbrdf
	spp = atoi(argv[2]);
	const char *scene_name = argv[3]; // point, areal, 2a1p

	// Validar modo
	if (strcmp(current_mode, "issa") != 0 &&
	    strcmp(current_mode, "isarea") != 0 &&
	    strcmp(current_mode, "isbrdf") != 0) {
		fprintf(stderr, "Modo desconocido: %s\n", current_mode);
		return 1;
	}

	// Configurar escena
	if (strcmp(scene_name, "point") == 0) {
		current_scene = scene_point;
		current_scene_count = scene_point_count;
		current_emitters = emitters_point;
		current_emitters_count = &emitters_point_count;
	} else if (strcmp(scene_name, "areal") == 0) {
		current_scene = scene_areal;
		current_scene_count = scene_areal_count;
		current_emitters = emitters_areal;
		current_emitters_count = &emitters_areal_count;
	} else if (strcmp(scene_name, "2a1p") == 0) {
		current_scene = scene_2a1p;
		current_scene_count = scene_2a1p_count;
		current_emitters = emitters_2a1p;
		current_emitters_count = &emitters_2a1p_count;
	} else {
		fprintf(stderr, "Escena desconocida: %s (use point, areal o 2a1p)\n", scene_name);
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
