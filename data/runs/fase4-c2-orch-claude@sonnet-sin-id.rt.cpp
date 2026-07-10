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

	// norma (longitud)
	double norm() const { return sqrt(x * x + y * y + z * z); }
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

// Tipo de material
enum MaterialType {
	DIFFUSE = 0,
	CONDUCTOR = 1
};

// Parámetros de material conductor (índice de refracción η y coeficiente de extinción k)
struct ConductorParams {
	Color eta; // índice de refracción
	Color k;   // coeficiente de extinción
	double alpha; // rugosidad Beckmann
};

class Sphere
{
public:
	double r;	// radio de la esfera
	Point p;	// posicion
	Color c;	// color (albedo para difusos)
	Color ke;	// emisión
	MaterialType material_type;
	ConductorParams* conductor; // si material_type == CONDUCTOR

	Sphere(double r_, Point p_, Color c_, Color ke_ = Color(), MaterialType type_ = DIFFUSE, ConductorParams* cond_ = NULL)
		: r(r_), p(p_), c(c_), ke(ke_), material_type(type_), conductor(cond_) {}

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

// Fase 04: Conductores con parámetros η, k
const int MAX_SPHERES = 15;
const int MAX_SCENES = 5;

// Parámetros de conductores
ConductorParams aluminium = {
	Color(1.66058, 0.88143, 0.521467),  // eta
	Color(9.2282, 6.27077, 4.83803),    // k
	0.3  // alpha (Beckmann)
};

ConductorParams gold = {
	Color(0.143245, 0.377423, 1.43919),  // eta
	Color(3.98479, 2.3847, 1.60434),     // k
	0.3  // alpha (Beckmann)
};

// Escena "point": solo puntual en (0, 24.3, 0)
Sphere scene_point[] = {
	Sphere(1e5,  Point(-1e5 - 49, 0, 0),   Color(.75, .25, .25), Color()), // pared izq
	Sphere(1e5,  Point(1e5 + 49, 0, 0),    Color(.25, .25, .75), Color()), // pared der
	Sphere(1e5,  Point(0, 0, -1e5 - 81.6), Color(.25, .75, .25), Color()), // pared detras
	Sphere(1e5,  Point(0, -1e5 - 40.8, 0), Color(.25, .75, .75), Color()), // suelo
	Sphere(1e5,  Point(0, 1e5 + 40.8, 0),  Color(.75, .75, .25), Color()), // techo
	Sphere(16.5, Point(-23, -24.3, -34.6), Color(0, 0, 0), Color(), CONDUCTOR, &aluminium),  // conductor
	Sphere(16.5, Point(23, -24.3, -3.6),   Color(0, 0, 0), Color(), CONDUCTOR, &gold),       // conductor
	Sphere(0.0,  Point(0, 24.3, 0),        Color(0, 0, 0), Color(2000, 2000, 2000), DIFFUSE) // puntual
};
const int scene_point_count = 8;

// Escena "areal": una emisora esférica r=10.5 en (0, 24.3, 0)
Sphere scene_areal[] = {
	Sphere(1e5,  Point(-1e5 - 49, 0, 0),   Color(.75, .25, .25), Color()), // pared izq
	Sphere(1e5,  Point(1e5 + 49, 0, 0),    Color(.25, .25, .75), Color()), // pared der
	Sphere(1e5,  Point(0, 0, -1e5 - 81.6), Color(.25, .75, .25), Color()), // pared detras
	Sphere(1e5,  Point(0, -1e5 - 40.8, 0), Color(.25, .75, .75), Color()), // suelo
	Sphere(1e5,  Point(0, 1e5 + 40.8, 0),  Color(.75, .75, .25), Color()), // techo
	Sphere(16.5, Point(-23, -24.3, -34.6), Color(0, 0, 0), Color(), CONDUCTOR, &aluminium),  // conductor
	Sphere(16.5, Point(23, -24.3, -3.6),   Color(0, 0, 0), Color(), CONDUCTOR, &gold),       // conductor
	Sphere(10.5, Point(0, 24.3, 0),        Color(1, 1, 1), Color(10, 10, 10), DIFFUSE)        // emisor esférico
};
const int scene_areal_count = 8;

// Escena "2a1p": puntual + dos emisoras esféricas
Sphere scene_2a1p[] = {
	Sphere(1e5,  Point(-1e5 - 49, 0, 0),   Color(.75, .25, .25), Color()), // pared izq
	Sphere(1e5,  Point(1e5 + 49, 0, 0),    Color(.25, .25, .75), Color()), // pared der
	Sphere(1e5,  Point(0, 0, -1e5 - 81.6), Color(.25, .75, .25), Color()), // pared detras
	Sphere(1e5,  Point(0, -1e5 - 40.8, 0), Color(.25, .75, .75), Color()), // suelo
	Sphere(1e5,  Point(0, 1e5 + 40.8, 0),  Color(.75, .75, .25), Color()), // techo
	Sphere(16.5, Point(-23, -24.3, -34.6), Color(0, 0, 0), Color(), CONDUCTOR, &aluminium),  // conductor
	Sphere(16.5, Point(23, -24.3, -3.6),   Color(0, 0, 0), Color(), CONDUCTOR, &gold),       // conductor
	Sphere(0.0,  Point(0, 24.3, 0),        Color(0, 0, 0), Color(2000, 2000, 2000), DIFFUSE), // puntual
	Sphere(10.5, Point(-23, 24.3, 0),      Color(1, 1, 1), Color(12, 5, 5), DIFFUSE),        // emisor 1
	Sphere(5.0,  Point(23, 24.3, -50),     Color(1, 1, 1), Color(5, 5, 12), DIFFUSE)         // emisor 2
};
const int scene_2a1p_count = 10;

// Puntero global a escena actual
Sphere *current_scene = scene_point;
int current_scene_count = scene_point_count;

// Rayos índices de emisores por escena
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

// Fresnel de conductor exacto por canal
// Entrada: coseno del ángulo entre ωi y ωh, índices η y k
// Salida: reflectancia Fresnel F por canal
double fresnel_conductor(double cos_theta, double eta_val, double k_val) {
	if (cos_theta < 0.0) cos_theta = 0.0;
	if (cos_theta > 1.0) cos_theta = 1.0;

	double cos2 = cos_theta * cos_theta;
	double sin2 = 1.0 - cos2;

	// a²b² = √((η²−k²−sin²θ)² + 4η²k²)
	double eta2 = eta_val * eta_val;
	double k2 = k_val * k_val;
	double term = eta2 - k2 - sin2;
	double a2b2_sq = term * term + 4.0 * eta2 * k2;
	double a2b2 = sqrt(a2b2_sq);

	// A = √((a²b²+η²−k²−sin²θ)/2)
	double A = sqrt((a2b2 + eta2 - k2 - sin2) * 0.5);

	// R⊥ = (a²b²+cos²θ−2A·cosθ) / (a²b²+cos²θ+2A·cosθ)
	double denom_perp = a2b2 + cos2 + 2.0 * A * cos_theta;
	double numer_perp = a2b2 + cos2 - 2.0 * A * cos_theta;
	if (denom_perp < 1e-10) return 1.0; // seguridad
	double R_perp = numer_perp / denom_perp;

	// R∥ = R⊥ · (a²b²·cos²θ+sin⁴θ−2A·cosθ·sin²θ) / (a²b²·cos²θ+sin⁴θ+2A·cosθ·sin²θ)
	double sin4 = sin2 * sin2;
	double denom_para = a2b2 * cos2 + sin4 + 2.0 * A * cos_theta * sin2;
	double numer_para = a2b2 * cos2 + sin4 - 2.0 * A * cos_theta * sin2;
	double R_para = (denom_para < 1e-10) ? 1.0 : R_perp * (numer_para / denom_para);

	// F = (R⊥ + R∥) / 2
	double F = (R_perp + R_para) * 0.5;
	return clamp(F);
}

// Distribución de Beckmann
// Entrada: coseno del ángulo polar θh (entre ωh y normal)
// Salida: valor de D(ωh)
double beckmann_D(double cos_theta_h, double alpha) {
	if (cos_theta_h <= 0.0) return 0.0;

	double cos2 = cos_theta_h * cos_theta_h;
	double cos4 = cos2 * cos2;
	double tan_theta_h_sq = (1.0 - cos2) / cos2;

	// D(ωh) = exp(−tan²θh/α²) / (π·α²·cos⁴θh)
	double exponent = -tan_theta_h_sq / (alpha * alpha);
	double D = exp(exponent) / (M_PI * alpha * alpha * cos4);

	return D;
}

// Función G1 de Smith (para un vector)
// Entrada: vector v (ωi u ωo), normal n, half-vector wh, alpha
// Salida: G1(v) -- factor de sombreado para una dirección
double smith_G1(const Vector &v, const Vector &n, const Vector &wh, double alpha) {
	double cos_v_n = v.dot(n);
	if (cos_v_n <= 0.0) return 0.0;

	// χ⁺((v·ωh)/(v·n)) - verifica que ambos tienen el mismo signo
	double v_wh = v.dot(wh);
	if ((v_wh * cos_v_n) <= 0.0) return 0.0;

	double sin_theta_v = sqrt(1.0 - cos_v_n * cos_v_n);
	if (sin_theta_v < 1e-10) return 1.0; // incidencia normal

	double tan_theta_v = sin_theta_v / cos_v_n;
	double a = 1.0 / (alpha * tan_theta_v);

	if (a >= 1.6) return 1.0;

	// G1 = (3.535a + 2.181a²) / (1 + 2.276a + 2.577a²)
	double a2 = a * a;
	double numer = 3.535 * a + 2.181 * a2;
	double denom = 1.0 + 2.276 * a + 2.577 * a2;

	return clamp(numer / denom);
}

// BRDF de microfacet para conductor
// fr = D(ωh)·G1(ωo)·G1(ωi)·F(ωi·ωh) / (4·(n·ωo)·(n·ωi))
// ωo: dirección de salida, ωi: dirección de incidencia (entrada), n: normal
// eta, k: índices del conductor
Color microfacet_eval(const Vector &wo, const Vector &wi, const Vector &n,
                       const Color &eta, const Color &k, double alpha) {
	double cos_wo_n = wo.dot(n);
	double cos_wi_n = wi.dot(n);

	// Si ωo o ωi están bajo el hemisferio, fr = 0
	if (cos_wo_n <= 0.0 || cos_wi_n <= 0.0) {
		return Color();
	}

	// Half-vector: ωh = (ωo + ωi) / |ωo + ωi|
	Vector wh = wo + wi;
	double wh_len_sq = wh.dot(wh);
	if (wh_len_sq < 1e-10) return Color();
	double wh_len = sqrt(wh_len_sq);
	wh = wh * (1.0 / wh_len);

	double cos_wh_n = wh.dot(n);
	if (cos_wh_n <= 0.0) return Color();

	// Distribución D
	double D = beckmann_D(cos_wh_n, alpha);

	// Término de sombreado G = G1(ωo) * G1(ωi)
	double G1_wo = smith_G1(wo, n, wh, alpha);
	double G1_wi = smith_G1(wi, n, wh, alpha);
	double G = G1_wo * G1_wi;

	// Fresnel F (por canal)
	// θ = acos(ωi·ωh) según la spec
	double cos_wi_wh = wi.dot(wh);
	// Clamp to valid range [0, 1]
	if (cos_wi_wh < 0.0) cos_wi_wh = 0.0;
	if (cos_wi_wh > 1.0) cos_wi_wh = 1.0;
	Color F = Color(
		fresnel_conductor(cos_wi_wh, eta.x, k.x),
		fresnel_conductor(cos_wi_wh, eta.y, k.y),
		fresnel_conductor(cos_wi_wh, eta.z, k.z)
	);

	// BRDF = D·G·F / (4·cos_wo_n·cos_wi_n)
	double denom = 4.0 * cos_wo_n * cos_wi_n;
	if (denom < 1e-10) return Color();

	Color fr = F * ((D * G) / denom);

	// Clamp negativo a 0
	return Color(
		fmax(0.0, fr.x),
		fmax(0.0, fr.y),
		fmax(0.0, fr.z)
	);
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

// Muestreo de BRDF de microfacet (Beckmann)
// Entrada: wo (dirección de salida), dos uniformes (xi1, xi2), alpha
// Salida: wi (dirección de incidencia muestreada), pdf del muestreo
void sample_microfacet(const Vector &wo, const Vector &n, double xi1, double xi2,
                       double alpha, Vector &wi, double &pdf) {
	// Muestrear half-vector según Beckmann
	// θh = atan(√(−α²·ln(1−ξ₁))), φh = 2πξ₂
	double ln_term = log(1.0 - xi1);
	if (ln_term >= 0.0) ln_term = -1e-10; // Seguridad
	double theta_h = atan(sqrt(-alpha * alpha * ln_term));
	double phi_h = 2.0 * M_PI * xi2;

	double cos_theta_h = cos(theta_h);
	double sin_theta_h = sin(theta_h);

	// Construir ωh en base local (n, s, t)
	Vector s, t;
	build_orthonormal_basis(n, s, t);
	Vector wh = s * (sin_theta_h * cos(phi_h)) + t * (sin_theta_h * sin(phi_h)) + n * cos_theta_h;

	// IMPORTANT: Recalculate cos_theta_h from wh.dot(n) to ensure consistency with microfacet_eval
	// This ensures the PDF uses the exact same angle as the BRDF evaluation
	double cos_theta_h_actual = wh.dot(n);

	// Reflejar ωo para obtener ωi
	// ωi = 2(ωh·ωo)ωh − ωo
	double wh_wo = wh.dot(wo);
	wi = wh * (2.0 * wh_wo) - wo;

	// Normalizar wi después de la reflexión
	double wi_norm = sqrt(wi.dot(wi));
	if (wi_norm < 1e-10) {
		pdf = 0.0;
		return;
	}
	wi = wi * (1.0 / wi_norm);

	// PDF: D(ωh)·cosθh / (4·|ωo·ωh|)  — per spec
	double D = beckmann_D(cos_theta_h_actual, alpha);
	double abs_wh_wo = fabs(wh_wo);
	if (abs_wh_wo < 1e-10) {
		pdf = 0.0;
		return;
	}
	double denom = 4.0 * abs_wh_wo;
	pdf = D * cos_theta_h_actual / denom;
	if (pdf < 1e-10) pdf = 0.0;  // Floor to avoid numerical explosion
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

// Debug counters for ISBRDF
long long debug_isbrdf_wi_invalid = 0;
long long debug_isbrdf_no_intersect = 0;
long long debug_isbrdf_no_emitter = 0;
long long debug_isbrdf_total_samples = 0;

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

// Fase 04: Muestreo de importancia de fuentes esféricas

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

// Calcula el valor de color para el rayo dado — fase 04 (microfacets + tres modos)
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

	// Dirección de salida (hacia la cámara)
	Vector wo = r.d * -1.0; // invertir dirección del rayo para obtener wo

	// Si el objeto es un emisor (ke ≠ 0), devolver su emisión
	if (obj.ke.x > 0 || obj.ke.y > 0 || obj.ke.z > 0) {
		return obj.ke;
	}

	// Iluminación directa
	Color L_total = Color();

	// Modo ISBRDF: muestreo de la BRDF del conductor
	if (strcmp(current_mode, "isbrdf") == 0) {
		// Solo funciona con conductores
		if (obj.material_type == CONDUCTOR && obj.conductor) {
			// Sample BRDF direction ONCE per pixel
			double xi1 = erand48(rand_state);
			double xi2 = erand48(rand_state);

			Vector wi;
			double pdf;
			sample_microfacet(wo, n, xi1, xi2, obj.conductor->alpha, wi, pdf);

			debug_isbrdf_total_samples++;

			// Verificar que wi está en el hemisferio y PDF es válido
			if (pdf > 0.0) {
				// Normalizar wi para asegurar que es una dirección válida
				double wi_len_sq = wi.dot(wi);
				if (wi_len_sq > 1e-10) {
					double wi_len = sqrt(wi_len_sq);
					wi = wi * (1.0 / wi_len);
					// Ajustar PDF por el cambio de escala: pdf_norm = pdf_orig / wi_len
					pdf = pdf * wi_len;
				} else {
					return L_total;
				}

				// Verificar que wi está en el hemisferio después de normalizar
				if (wi.dot(n) <= 0.0) {
					return L_total;
				}

				// Cast ray in sampled direction
				Ray sample_ray(x + n * 1e-4, wi);
				double t_sample;
				int id_sample;

				// Check if ray hits any emitter
				if (intersect(sample_ray, t_sample, id_sample)) {
					const Sphere &sample_obj = current_scene[id_sample];
					// If ray hits an emitter, accumulate contribution
					if (sample_obj.ke.x > 0 || sample_obj.ke.y > 0 || sample_obj.ke.z > 0) {
						Color fr = microfacet_eval(wo, wi, n, obj.conductor->eta, obj.conductor->k, obj.conductor->alpha);
						double cos_wi_n = wi.dot(n);
						Color contrib = fr.mult(sample_obj.ke) * (cos_wi_n / pdf);

						// Clamp to 0
						L_total = L_total + Color(
							fmax(0.0, contrib.x),
							fmax(0.0, contrib.y),
							fmax(0.0, contrib.z)
						);
					} else {
						debug_isbrdf_no_emitter++;
					}
				} else {
					debug_isbrdf_no_intersect++;
				}
			} else {
				debug_isbrdf_wi_invalid++;
			}
		}
		return L_total;
	}

	// Modos ISSA (solid angle) e ISAREA (area)
	for (int emitter_idx = 0; emitter_idx < *current_emitters_count; emitter_idx++) {
		int light_id = current_emitters[emitter_idx];
		const Sphere &light = current_scene[light_id];

		// Caso 1: Fuente puntual (r=0)
		if (light.r < 1e-6) {
			// Dirección hacia la fuente
			Vector w = light.p - x;
			double r_dist = sqrt(w.dot(w)); // distancia
			w = w * (1.0 / r_dist); // normalizar

			double costheta = n.dot(w);
			if (costheta > 0.0) {
				// Lanzar rayo de sombra
				Ray shadow_ray(x + n * 1e-4, w);
				double t_shadow;
				int id_shadow;

				// Para una fuente puntual: es visible si no hay obstáculo entre nosotros y la fuente
				bool is_visible = !intersect(shadow_ray, t_shadow, id_shadow) || t_shadow >= r_dist - 1e-4;

				if (is_visible) {
					// Visible
					Color fr;
					if (obj.material_type == CONDUCTOR && obj.conductor) {
						// BRDF de microfacet
						fr = microfacet_eval(wo, w, n, obj.conductor->eta, obj.conductor->k, obj.conductor->alpha);
					} else {
						// BRDF difusa
						fr = obj.c * (1.0 / M_PI);
					}

					Color contrib = fr.mult(light.ke) * (costheta / (r_dist * r_dist));
					L_total = L_total + contrib;
				}
			}
		}
		// Caso 2: Fuentes esféricas (r > 0) con ISSA (solid angle)
		else if (strcmp(current_mode, "issa") == 0) {
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
						Color fr;
						if (obj.material_type == CONDUCTOR && obj.conductor) {
							fr = microfacet_eval(wo, w, n, obj.conductor->eta, obj.conductor->k, obj.conductor->alpha);
						} else {
							fr = obj.c * (1.0 / M_PI);
						}

						Color contrib = fr.mult(light.ke) * (costheta / pdf_omega);
						L_total = L_total + contrib;
					}
				}
			}
		}
		// Caso 3: Fuentes esféricas (r > 0) con ISAREA (area)
		else if (strcmp(current_mode, "isarea") == 0) {
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
							// Visible
							Color fr;
							if (obj.material_type == CONDUCTOR && obj.conductor) {
								fr = microfacet_eval(wo, w, n, obj.conductor->eta, obj.conductor->k, obj.conductor->alpha);
							} else {
								fr = obj.c * (1.0 / M_PI);
							}

							Color contrib = fr.mult(light.ke) * (costheta / pdf_omega);
							L_total = L_total + contrib;
						}
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
		fprintf(stderr, "Escena desconocida: %s (use point, areal, o 2a1p)\n", scene_name);
		return 1;
	}

	// Validar modo
	if (strcmp(current_mode, "issa") != 0 &&
	    strcmp(current_mode, "isarea") != 0 &&
	    strcmp(current_mode, "isbrdf") != 0) {
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
