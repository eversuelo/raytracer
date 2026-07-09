// rt: un lanzador de rayos minimalista
 // g++ -O3 -fopenmp rt.cpp -o rt
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
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
	Vector operator%(const Vector &b) const {return Vector(y * b.z - z * b.y, z * b.x - x * b.z, x * b.y - y * b.x);}
	
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

	// determina si el rayo intersecta a esta esfera
	double intersect(const Ray &ray) const {
		Vector oc = ray.o - p;
		double b = oc.dot(ray.d);
		double det = b * b - (oc.dot(oc) - r * r);
		if (det < 0.0) return 0.0;

		double s = sqrt(det);
		double t1 = -b - s;
		double t2 = -b + s;

		if (t1 > 1e-4) return t1;
		if (t2 > 1e-4) return t2;
		return 0.0;
	}
};

// Esferas globales: inicializar con esferas dummy
Sphere spheres[16] = {
	Sphere(1e5, Point(-1e5 - 49, 0, 0),   Color(.75, .25, .25), Color(0, 0, 0)),     // pared izq
	Sphere(1e5, Point(1e5 + 49, 0, 0),    Color(.25, .25, .75), Color(0, 0, 0)),     // pared der
	Sphere(1e5, Point(0, 0, -1e5 - 81.6), Color(.25, .75, .25), Color(0, 0, 0)),     // pared atrás
	Sphere(1e5, Point(0, -1e5 - 40.8, 0), Color(.25, .75, .75), Color(0, 0, 0)),     // suelo
	Sphere(1e5, Point(0, 1e5 + 40.8, 0),  Color(.75, .75, .25), Color(0, 0, 0)),     // techo
	Sphere(16.5, Point(-23, -24.3, -34.6), Color(.2, .3, .4), Color(0, 0, 0)),       // esfera abajo-izq
	Sphere(16.5, Point(23, -24.3, -3.6),   Color(.4, .3, .2), Color(0, 0, 0)),       // esfera abajo-der
	Sphere(10.5, Point(0, 24.3, 0),        Color(1, 1, 1), Color(10, 10, 10)),        // emisor (dummy)
	Sphere(0, Point(0,0,0), Color(0,0,0), Color(0,0,0)),  // dummy
	Sphere(0, Point(0,0,0), Color(0,0,0), Color(0,0,0)),  // dummy
	Sphere(0, Point(0,0,0), Color(0,0,0), Color(0,0,0)),  // dummy
	Sphere(0, Point(0,0,0), Color(0,0,0), Color(0,0,0)),  // dummy
	Sphere(0, Point(0,0,0), Color(0,0,0), Color(0,0,0)),  // dummy
	Sphere(0, Point(0,0,0), Color(0,0,0), Color(0,0,0)),  // dummy
	Sphere(0, Point(0,0,0), Color(0,0,0), Color(0,0,0)),  // dummy
	Sphere(0, Point(0,0,0), Color(0,0,0), Color(0,0,0))   // dummy
};
int num_spheres = 0;
int num_emitters = 0;  // Número de emisores para iterar en shade_multiple

// Función para inicializar la escena según el parámetro
void init_scene(const std::string &scene_name) {
	if (scene_name == "1L") {
		// Escena 1L: un emisor esférico de la fase 1
		spheres[7] = Sphere(10.5, Point(0, 24.3, 0), Color(1, 1, 1), Color(10, 10, 10));
		num_spheres = 8;
		num_emitters = 1;
	} else if (scene_name == "2A1P") {
		// Escena 2A1P: tres emisores (1 puntual + 2 esféricas)
		spheres[7] = Sphere(0.0, Point(0, 24.3, 0), Color(0, 0, 0), Color(2000, 2000, 2000));    // puntual
		spheres[8] = Sphere(10.5, Point(-23, 24.3, 0), Color(1, 1, 1), Color(12, 5, 5));         // esfera 1
		spheres[9] = Sphere(5, Point(23, 24.3, -50), Color(1, 1, 1), Color(5, 5, 12));           // esfera 2
		num_spheres = 10;
		num_emitters = 3;
	} else {
		fprintf(stderr, "Escena desconocida: %s\n", scene_name.c_str());
		exit(1);
	}
}

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

inline bool intersect(const Ray &r, double &t, int &id) {
	double tmin = 1e20;
	int idmin = -1;
	for (int i = 0; i < num_spheres; i++) {
		double ti = spheres[i].intersect(r);
		if (ti > 0.0 && ti < tmin) {
			tmin = ti;
			idmin = i;
		}
	}
	if (idmin >= 0) {
		t = tmin;
		id = idmin;
		return true;
	}
	return false;
}

// Construir base ortonormal sobre n, devolver (t, s) para pasar local a global
inline void build_basis(const Vector &n, Vector &t, Vector &s) {
	if (fabs(n.x) > fabs(n.y)) {
		double inv = 1.0 / sqrt(n.x * n.x + n.z * n.z);
		t = Vector(n.z, 0, -n.x) * inv;
	} else {
		double inv = 1.0 / sqrt(n.y * n.y + n.z * n.z);
		t = Vector(0, n.z, -n.y) * inv;
	}
	s = t % n;
}

// Tres muestreadores de direcciones
struct Sample {
	Vector wi;
	double pdf;
};

Sample uniformsphere(unsigned short *xi, const Vector &n) {
	double xi1 = erand48(xi);
	double xi2 = erand48(xi);

	double cos_theta = 1.0 - 2.0 * xi1;
	double sin_theta = sqrt(1.0 - cos_theta * cos_theta);
	double phi = 2.0 * M_PI * xi2;

	Vector t, s;
	build_basis(n, t, s);

	double wx = sin_theta * cos(phi);
	double wy = sin_theta * sin(phi);
	double wz = cos_theta;

	Vector wi = s * wx + t * wy + n * wz;
	Sample result = {wi, 1.0 / (4.0 * M_PI)};
	return result;
}

Sample uniformhemi(unsigned short *xi, const Vector &n) {
	double xi1 = erand48(xi);
	double xi2 = erand48(xi);

	double cos_theta = xi1;
	double sin_theta = sqrt(1.0 - cos_theta * cos_theta);
	double phi = 2.0 * M_PI * xi2;

	Vector t, s;
	build_basis(n, t, s);

	double wx = sin_theta * cos(phi);
	double wy = sin_theta * sin(phi);
	double wz = cos_theta;

	Vector wi = s * wx + t * wy + n * wz;
	Sample result = {wi, 1.0 / (2.0 * M_PI)};
	return result;
}

Sample cosinehemi(unsigned short *xi, const Vector &n) {
	double xi1 = erand48(xi);
	double xi2 = erand48(xi);

	double cos_theta = sqrt(1.0 - xi1);
	double sin_theta = sqrt(xi1);
	double phi = 2.0 * M_PI * xi2;

	Vector t, s;
	build_basis(n, t, s);

	double wx = sin_theta * cos(phi);
	double wy = sin_theta * sin(phi);
	double wz = cos_theta;

	Vector wi = s * wx + t * wy + n * wz;
	double pdf = cos_theta / M_PI;
	Sample result = {wi, pdf};
	return result;
}


// Muestreo de área de fuente: muestra x' = c + r·ω uniforme en esfera,
// convierte pdf a ángulo sólido
struct LightSample {
	Vector omega;      // dirección hacia el punto muestreado en la fuente
	double pdf_omega;  // pdf en ángulo sólido
	Point xprime;      // punto muestreado en la fuente
};

LightSample sampleAreaLight(unsigned short *xi, const Point &x, const Sphere &light) {
	if (light.r <= 1e-6) {
		// Puntual: no usar este método
		LightSample result = {{0,0,0}, 1.0, light.p};
		return result;
	}

	// Muestrear punto uniforme en superficie de la esfera
	double u1 = erand48(xi);
	double u2 = erand48(xi);

	// Coordenadas esféricas uniformes
	double cos_theta = 1.0 - 2.0 * u1;     // cos(θ) = 1 - 2ξ₁
	double sin_theta = sqrt(1.0 - cos_theta * cos_theta);
	double phi = 2.0 * M_PI * u2;          // φ = 2πξ₂

	// Muestrear dirección ω uniforme en la esfera (en coordenadas globales)
	// Usar eje x fijo para evitar singularidad en build_basis
	Vector ex(1, 0, 0);
	Vector ey, ez;
	build_basis(ex, ey, ez);

	// Transformar a coordenadas globales: ω = (cos(θ), sin(θ)cos(φ), sin(θ)sin(φ))
	Vector omega_dir_sample = ex * cos_theta + ey * (sin_theta * cos(phi)) + ez * (sin_theta * sin(phi));

	// x' = c + r·ω
	Point xprime = light.p + omega_dir_sample * light.r;

	// Dirección x→x' y distancia
	Vector delta = xprime - x;
	double d = sqrt(delta.dot(delta));
	Vector omega_dir = delta * (1.0 / d);

	// Normal en x' (apunta radialmente desde c de la esfera)
	Vector n_xprime = omega_dir_sample;  // Ya está normalizado

	// Coseno en x': ángulo entre n_xprime y dirección x'→x
	// omega_dir es x→x', así que (x'→x) = -omega_dir
	double cos_theta_prime = -n_xprime.dot(omega_dir);

	// Evitar singularidad si cos_theta_prime <= 0
	if (cos_theta_prime <= 0) {
		LightSample result = {{0,0,0}, 1e20, xprime};
		return result;
	}

	// pdf en ángulo sólido: d² / (4πr² · cos(θ'))
	double pdf_omega = d * d / (4.0 * M_PI * light.r * light.r * cos_theta_prime);

	LightSample result;
	result.omega = omega_dir;
	result.pdf_omega = pdf_omega;
	result.xprime = xprime;
	return result;
}

// Muestreo de ángulo sólido: muestrea direcciones uniformes en el cono
// subtendido por la esfera desde x
LightSample sampleSolidAngle(unsigned short *xi, const Point &x, const Sphere &light) {
	if (light.r <= 1e-6) {
		// Si es puntual, no usar este muestreador
		LightSample result = {{0,0,0}, 1.0, light.p};
		return result;
	}

	// Eje del cono: W = normalize(c - x)
	Vector W = light.p - x;
	double dist_to_center = sqrt(W.dot(W));
	W = W * (1.0 / dist_to_center);

	// cos(θmax) = √(1 - (r/d)²)
	double ratio = light.r / dist_to_center;
	if (ratio > 1.0) ratio = 1.0;  // Evitar NaN si x está dentro de la esfera
	double cos_theta_max = sqrt(1.0 - ratio * ratio);

	// Muestrear dirección en el cono
	double u1 = erand48(xi);
	double u2 = erand48(xi);

	double cos_theta = 1.0 - u1 + u1 * cos_theta_max;
	double sin_theta = sqrt(1.0 - cos_theta * cos_theta);
	double phi = 2.0 * M_PI * u2;

	// Base ortogonal con W como eje principal
	Vector ey, ez;
	build_basis(W, ey, ez);

	// Dirección muestreada en coordenadas globales
	Vector omega = W * cos_theta + ey * (sin_theta * cos(phi)) + ez * (sin_theta * sin(phi));

	// Encontrar el punto de intersección en la esfera
	// Intersectar rayo (x, omega) con esfera (light.p, light.r)
	// (x + t*omega - light.p)·(x + t*omega - light.p) = r²
	Vector oc = x - light.p;
	double a = omega.dot(omega);
	double b = 2.0 * oc.dot(omega);
	double c = oc.dot(oc) - light.r * light.r;
	double disc = b * b - 4.0 * a * c;

	Point xprime = light.p;  // default
	if (disc >= 0) {
		double t1 = (-b - sqrt(disc)) / (2.0 * a);
		double t2 = (-b + sqrt(disc)) / (2.0 * a);
		double t_hit = (t1 > 1e-4) ? t1 : t2;
		if (t_hit > 1e-4) {
			xprime = x + omega * t_hit;
		}
	}

	// pdf_ω = 1 / (2π(1 - cos(θmax)))
	double pdf_omega = 1.0 / (2.0 * M_PI * (1.0 - cos_theta_max));

	LightSample result;
	result.omega = omega;
	result.pdf_omega = pdf_omega;
	result.xprime = xprime;
	return result;
}

// Iluminación directa por Monte Carlo con muestreo de importancia de fuentes
// Itera TODAS las fuentes (esféricas y puntuales)
Color shade_multiple(const Ray &r, unsigned short *xi,
                      const std::string &sampling_mode) {
	double t;
	int id = 0;

	if (!intersect(r, t, id))
		return Color();	// negro si no hay intersección

	const Sphere &obj = spheres[id];

	// Si el objeto emite, devolver su emisión (clamp a blanco)
	if (obj.ke.x > 0 || obj.ke.y > 0 || obj.ke.z > 0) {
		Color result = obj.ke;
		result.x = clamp(result.x);
		result.y = clamp(result.y);
		result.z = clamp(result.z);
		return result;
	}

	// Punto de intersección y normal
	Point x = r.o + r.d * t;
	Vector n = (x - obj.p);
	n.normalize();

	Color radiance = Color();

	// Iterar todas las fuentes emisoras
	for (int emitter_idx = 0; emitter_idx < num_emitters; emitter_idx++) {
		// Índice de la esfera emisora en el array
		int sphere_idx = num_spheres - num_emitters + emitter_idx;
		const Sphere &light = spheres[sphere_idx];

		if (light.r <= 1e-6) {
			// Fuente puntual: usar término determinista
			Vector w = light.p - x;
			double r_dist = sqrt(w.dot(w));
			w = w * (1.0 / r_dist);

			double cos_theta = n.dot(w);
			if (cos_theta > 0) {
				// Lanzar rayo de sombra
				Ray shadowRay(x + n * 1e-4, w);
				double t_hit;
				int id_hit;

				if (intersect(shadowRay, t_hit, id_hit)) {
					if (t_hit >= r_dist - 1e-4) {
						// Visible
						Vector fr = obj.c * (1.0 / M_PI);
						Color contrib = fr.mult(light.ke) * (cos_theta / (r_dist * r_dist));
						radiance = radiance + contrib;
					}
				}
			}
		} else {
			// Fuente esférica: usar muestreo de importancia
			LightSample light_sample;

			if (sampling_mode == "arealight") {
				light_sample = sampleAreaLight(xi, x, light);
			} else if (sampling_mode == "solidangle") {
				light_sample = sampleSolidAngle(xi, x, light);
			} else {
				// No debería pasar
				continue;
			}

			Vector omega = light_sample.omega;
			double pdf_omega = light_sample.pdf_omega;
			Point xprime = light_sample.xprime;

			double d = sqrt((xprime - x).dot(xprime - x));

			// Coseno en x
			double cos_theta = n.dot(omega);
			if (cos_theta > 0 && pdf_omega > 0) {
				// Lanzar rayo de sombra desde x hacia x'
				Ray shadowRay(x + n * 1e-4, omega);

				double t_hit;
				int id_hit;
				bool visible = false;

				if (intersect(shadowRay, t_hit, id_hit)) {
					// Verificar si el rayo golpea la fuente
					if (id_hit == sphere_idx) {
						visible = true;
					}
				}

				if (visible) {
					Vector fr = obj.c * (1.0 / M_PI);
					Color contrib = fr.mult(light.ke) * (cos_theta / pdf_omega);
					radiance = radiance + contrib;
				}
			}
		}
	}

	return radiance;
}

// Iluminación directa coseno-hemisférico + término determinista de puntuales (fase 1 extendida)
Color shade_coshemi(const Ray &r, unsigned short *xi) {
	double t;
	int id = 0;

	if (!intersect(r, t, id))
		return Color();	// negro si no hay intersección

	const Sphere &obj = spheres[id];

	// Si el objeto emite, devolver su emisión
	if (obj.ke.x > 0 || obj.ke.y > 0 || obj.ke.z > 0) {
		Color result = obj.ke;
		result.x = clamp(result.x);
		result.y = clamp(result.y);
		result.z = clamp(result.z);
		return result;
	}

	// Punto de intersección y normal
	Point x = r.o + r.d * t;
	Vector n = (x - obj.p);
	n.normalize();

	Color radiance = Color();

	// Muestrear dirección coseno-hemisférica
	Sample sample = cosinehemi(xi, n);
	Vector wi = sample.wi;
	double pdf = sample.pdf;

	// Rayo de muestreo
	Ray sampleRay(x + n * 1e-4, wi);

	double t_hit;
	int id_hit;
	if (intersect(sampleRay, t_hit, id_hit)) {
		const Sphere &emitter = spheres[id_hit];
		Color Le = emitter.ke;

		// Calcular radiancia: L = fr ⊙ Le · (n·wi) / pdf
		double cos_theta = n.dot(wi);
		if (cos_theta > 0) {
			Vector fr = obj.c * (1.0 / M_PI);
			radiance = fr.mult(Le) * (cos_theta / pdf);
		}
	}

	// Agregar término determinista de puntuales (que nunca son alcanzadas por rayo aleatorio)
	for (int emitter_idx = 0; emitter_idx < num_emitters; emitter_idx++) {
		int sphere_idx = num_spheres - num_emitters + emitter_idx;
		const Sphere &light = spheres[sphere_idx];

		if (light.r <= 1e-6) {
			// Fuente puntual: término determinista
			Vector w = light.p - x;
			double r_dist = sqrt(w.dot(w));
			w = w * (1.0 / r_dist);

			double cos_theta = n.dot(w);
			if (cos_theta > 0) {
				// Lanzar rayo de sombra
				Ray shadowRay(x + n * 1e-4, w);
				double t_hit;
				int id_hit;

				if (intersect(shadowRay, t_hit, id_hit)) {
					if (t_hit >= r_dist - 1e-4) {
						// Visible
						Vector fr = obj.c * (1.0 / M_PI);
						Color contrib = fr.mult(light.ke) * (cos_theta / (r_dist * r_dist));
						radiance = radiance + contrib;
					}
				}
			}
		}
	}

	return radiance;
}

// Iluminación directa por Monte Carlo (fase 01, original)
Color shade(const Ray &r, unsigned short *xi, Sample (*sampler)(unsigned short *, const Vector &)) {
	double t;
	int id = 0;

	if (!intersect(r, t, id))
		return Color();	// negro si no hay intersección

	const Sphere &obj = spheres[id];

	// Si el objeto emite, devolver su emisión
	if (obj.ke.x > 0 || obj.ke.y > 0 || obj.ke.z > 0) {
		return obj.ke;
	}

	// Punto de intersección y normal
	Point x = r.o + r.d * t;
	Vector n = (x - obj.p);
	n.normalize();

	// Muestrear dirección de salida
	Sample sample = sampler(xi, n);
	Vector wi = sample.wi;
	double pdf = sample.pdf;

	// Lanzar rayo desde x en dirección wi
	Ray shadowRay(x + n * 1e-4, wi);

	double t_hit;
	int id_hit;
	Color radiance = Color();

	if (intersect(shadowRay, t_hit, id_hit)) {
		const Sphere &emitter = spheres[id_hit];
		Color Le = emitter.ke;

		// Calcular radiancia: L = fr ⊙ Le · (n·wi) / pdf
		double cos_theta = n.dot(wi);
		if (cos_theta > 0) {
			Vector fr = obj.c * (1.0 / M_PI);  // BRDF difusa = albedo / π
			radiance = fr.mult(Le) * (cos_theta / pdf);
		}
	}

	return radiance;
}


int main(int argc, char *argv[]) {
	if (argc < 4) {
		fprintf(stderr, "Uso: %s <modo> <spp> <escena>\n", argv[0]);
		fprintf(stderr, "  modo: arealight, solidangle, coshemi\n");
		fprintf(stderr, "  spp: número de muestras por píxel\n");
		fprintf(stderr, "  escena: 1L, 2A1P\n");
		return 1;
	}

	std::string mode(argv[1]);
	int spp = atoi(argv[2]);
	std::string scene_name(argv[3]);

	// Validar modo
	if (mode != "arealight" && mode != "solidangle" && mode != "coshemi") {
		fprintf(stderr, "Modo desconocido: %s\n", mode.c_str());
		return 1;
	}

	// Inicializar la escena
	init_scene(scene_name);

	int w = 1024, h = 768;
	Ray camera(Point(0, 11.2, 214), Vector(0, -0.042612, -1).normalize());
	Vector cx = Vector(w * 0.5095 / h, 0., 0.);
	Vector cy = (cx % camera.d).normalize() * 0.5095;
	Color *pixelColors = new Color[w * h];

	#pragma omp parallel for schedule(static)
	for(int y = 0; y < h; y++)
	{
		#pragma omp critical
		fprintf(stderr,"\r%5.2f%%",100.*y/(h-1));

		for(int x = 0; x < w; x++ ) {
			int idx = (h - y - 1) * w + x;

			Vector cameraRayDir = cx * (double(x)/w - .5) + cy * (double(y)/h - .5) + camera.d;
			Ray primaryRay(camera.o, cameraRayDir.normalize());

			Color pixelValue = Color();

			// Inicializar RNG con semilla basada en índice de píxel
			unsigned short xi[3];
			xi[0] = (idx >> 16) & 0xFFFF;
			xi[1] = (idx >> 0) & 0xFFFF;
			xi[2] = 1;

			// Promediar spp muestras
			for (int s = 0; s < spp; s++) {
				Color sample;
				if (mode == "coshemi") {
					sample = shade_coshemi(primaryRay, xi);
				} else if (mode == "arealight" || mode == "solidangle") {
					sample = shade_multiple(primaryRay, xi, mode);
				}
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
