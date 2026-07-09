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

Sphere spheres[] = {
	Sphere(1e5, Point(-1e5 - 49, 0, 0),   Color(.75, .25, .25), Color(0, 0, 0)),     // pared izq
	Sphere(1e5, Point(1e5 + 49, 0, 0),    Color(.25, .25, .75), Color(0, 0, 0)),     // pared der
	Sphere(1e5, Point(0, 0, -1e5 - 81.6), Color(.25, .75, .25), Color(0, 0, 0)),     // pared atrás
	Sphere(1e5, Point(0, -1e5 - 40.8, 0), Color(.25, .75, .75), Color(0, 0, 0)),     // suelo
	Sphere(1e5, Point(0, 1e5 + 40.8, 0),  Color(.75, .75, .25), Color(0, 0, 0)),     // techo
	Sphere(16.5, Point(-23, -24.3, -34.6), Color(.2, .3, .4), Color(0, 0, 0)),       // esfera abajo-izq
	Sphere(16.5, Point(23, -24.3, -3.6),   Color(.4, .3, .2), Color(0, 0, 0)),       // esfera abajo-der
	Sphere(10.5, Point(0, 24.3, 0),        Color(1, 1, 1), Color(10, 10, 10))        // emisor
};

// Fuente puntual para fase 02 (radio 0, nunca intersectada; se usa directamente en shade_point)
Point light_pos(0, 24.3, 0);
Color light_intensity(4000, 4000, 4000);
bool pointLightMode = false;  // flag para modo fuente puntual

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
	double tmin = 1e20;
	int idmin = -1;
	int limit = 8;
	if (pointLightMode) limit = 7;  // En modo puntual, ignorar esfera emisora (índice 7)
	for (int i = 0; i < limit; i++) {
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

// Iluminación directa determinista con fuente puntual (fase 02)
Color shade_point(const Ray &r) {
	double t;
	int id = 0;

	if (!intersect(r, t, id))
		return Color();	// negro si no hay intersección

	const Sphere &obj = spheres[id];

	// Punto de intersección y normal
	Point x = r.o + r.d * t;
	Vector n = (x - obj.p);
	n.normalize();

	// Dirección hacia la fuente puntual
	Vector w = light_pos - x;
	double r_dist = sqrt(w.dot(w));  // distancia a la fuente
	w.normalize();  // normalizar dirección

	// Coseno del ángulo entre normal y dirección a la fuente
	double cos_theta = n.dot(w);
	if (cos_theta <= 0) {
		return Color();  // Lado opuesto, no se ilumina
	}

	// Lanzar rayo de sombra desde x hacia la fuente
	Ray shadowRay(x + n * 1e-4, w);

	double t_hit;
	int id_hit;
	if (intersect(shadowRay, t_hit, id_hit)) {
		if (t_hit < r_dist - 1e-4) {
			return Color();  // Hay sombra: una esfera bloquea la luz
		}
	}

	// Calcular radiancia: L = (albedo/π) ⊙ I · cosθ / r²
	Vector fr = obj.c * (1.0 / M_PI);
	Color radiance = fr.mult(light_intensity) * (cos_theta / (r_dist * r_dist));

	return radiance;
}

// Iluminación directa por Monte Carlo
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
	if (argc < 2) {
		fprintf(stderr, "Uso: %s <point|sampler> [spp]\n", argv[0]);
		fprintf(stderr, "  point: iluminación directa determinista con fuente puntual (fase 02)\n");
		fprintf(stderr, "  sampler: uniformsphere, uniformhemi, cosinehemi (fase 01, con spp)\n");
		fprintf(stderr, "  spp: número de muestras por píxel (requerido para sampler)\n");
		return 1;
	}

	std::string mode(argv[1]);
	int w = 1024, h = 768;
	Ray camera(Point(0, 11.2, 214), Vector(0, -0.042612, -1).normalize());
	Vector cx = Vector(w * 0.5095 / h, 0., 0.);
	Vector cy = (cx % camera.d).normalize() * 0.5095;
	Color *pixelColors = new Color[w * h];

	if (mode == "point") {
		// Modo fase 02: iluminación determinista con fuente puntual
		pointLightMode = true;

		#pragma omp parallel for schedule(static)
		for(int y = 0; y < h; y++)
		{
			#pragma omp critical
			fprintf(stderr,"\r%5.2f%%",100.*y/(h-1));

			for(int x = 0; x < w; x++ ) {
				int idx = (h - y - 1) * w + x;

				Vector cameraRayDir = cx * (double(x)/w - .5) + cy * (double(y)/h - .5) + camera.d;
				Ray primaryRay(camera.o, cameraRayDir.normalize());

				// Sin muestreo, un rayo por píxel, shading determinista
				Color pixelValue = shade_point(primaryRay);
				pixelColors[idx] = Color(clamp(pixelValue.x), clamp(pixelValue.y), clamp(pixelValue.z));
			}
		}

		fprintf(stderr,"\n");

		FILE *f = fopen("image-plight.ppm", "w");
		fprintf(f, "P3\n%d %d\n%d\n", w, h, 255);
		for (int p = 0; p < w * h; p++) {
			fprintf(f,"%d %d %d ", toDisplayValue(pixelColors[p].x), toDisplayValue(pixelColors[p].y),
				toDisplayValue(pixelColors[p].z));
		}
		fclose(f);

	} else {
		// Modo fase 01: iluminación Monte Carlo con esferas emisoras
		if (argc < 3) {
			fprintf(stderr, "Uso: %s <sampler> <spp>\n", argv[0]);
			fprintf(stderr, "  sampler: uniformsphere, uniformhemi, cosinehemi\n");
			fprintf(stderr, "  spp: número de muestras por píxel\n");
			return 1;
		}

		std::string sampler_name = mode;
		int spp = atoi(argv[2]);

		Sample (*sampler)(unsigned short *, const Vector &);
		if (sampler_name == "uniformsphere") {
			sampler = &uniformsphere;
		} else if (sampler_name == "uniformhemi") {
			sampler = &uniformhemi;
		} else if (sampler_name == "cosinehemi") {
			sampler = &cosinehemi;
		} else {
			fprintf(stderr, "Sampler desconocido: %s\n", sampler_name.c_str());
			return 1;
		}

		pointLightMode = false;  // Asegurar que mode MC no ignore la esfera emisora

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
					Color sample = shade(primaryRay, xi, sampler);
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
	}

	delete[] pixelColors;

	return 0;
}
