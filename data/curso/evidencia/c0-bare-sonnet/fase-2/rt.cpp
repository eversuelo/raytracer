// rt: un lanzador de rayos minimalista
 // g++ -O3 -fopenmp rt.cpp -o rt
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <omp.h>
#include <vector>

static const double PI = 3.14159265358979323846;

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
	Vector operator%(Vector&b){return Vector(y * b.z - z * b.y, z * b.x - x * b.z, x * b.y - y * b.x);}

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
	Color ke;	// emision

	Sphere(double r_, Point p_, Color c_, Color ke_): r(r_), p(p_), c(c_), ke(ke_) {}

	// determina si el rayo intersecta a esta esfera
	double intersect(const Ray &ray) const {
		// regresar distancia si hay intersección
		// regresar 0.0 si no hay interseccion
		Vector oc = ray.o - p;
		double b = oc.dot(ray.d);
		double det = b * b - (oc.dot(oc) - r * r);
		if (det < 0)
			return 0.0;
		double sq = sqrt(det);
		double t1 = -b - sq;
		if (t1 > 1e-4)
			return t1;
		double t2 = -b + sq;
		if (t2 > 1e-4)
			return t2;
		return 0.0;
	}
};

// Escena: paredes+suelo+techo difusos y dos esferas difusas (comunes a todos los modos).
// El emisor esférico (phase-01) y la fuente puntual (phase-02) son mutuamente excluyentes
// y se agregan en buildScene() según el modo.
std::vector<Sphere> spheres;

// RF-3 (phase-02): la fuente puntual se representa como lista aparte, NO como esfera de
// spheres[] — así ningún rayo (cámara o sombra) la intersecta jamás y el techo bajo la
// luz queda "sin esfera visible" según pide el criterio de aceptación.
class PointLight
{
public:
	Point p;  // posicion
	Color I;  // intensidad radiante

	PointLight(Point p_, Color I_) : p(p_), I(I_) {}
};
std::vector<PointLight> pointLights;

// arma la escena: geometría base siempre presente; agrega el emisor esférico (modos MC
// de la phase-01) o la fuente puntual (modo "point" de la phase-02), nunca ambos
void buildScene(bool pointMode) {
	//                    radio  posicion                       albedo              emision
	spheres.push_back(Sphere(1e5,  Point(-1e5 - 49, 0, 0),   Color(.75, .25, .25), Color()));          // pared izq
	spheres.push_back(Sphere(1e5,  Point(1e5 + 49, 0, 0),    Color(.25, .25, .75), Color()));          // pared der
	spheres.push_back(Sphere(1e5,  Point(0, 0, -1e5 - 81.6), Color(.25, .75, .25), Color()));          // pared detras
	spheres.push_back(Sphere(1e5,  Point(0, -1e5 - 40.8, 0), Color(.25, .75, .75), Color()));          // suelo
	spheres.push_back(Sphere(1e5,  Point(0, 1e5 + 40.8, 0),  Color(.75, .75, .25), Color()));          // techo
	spheres.push_back(Sphere(16.5, Point(-23, -24.3, -34.6), Color(.2, .3, .4),    Color()));          // esfera abajo-izq
	spheres.push_back(Sphere(16.5, Point(23, -24.3, -3.6),   Color(.4, .3, .2),    Color()));          // esfera abajo-der

	if (pointMode)
		pointLights.push_back(PointLight(Point(0, 24.3, 0), Color(4000, 4000, 4000)));
	else
		spheres.push_back(Sphere(10.5, Point(0, 24.3, 0), Color(1, 1, 1), Color(10, 10, 10))); // esfera arriba (emisor)
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

// calcular la intersección del rayo r con todas las esferas
// regresar true si hubo una intersección, falso de otro modo
// almacenar en t la distancia sobre el rayo en que sucede la interseccion
// almacenar en id el indice de spheres[] de la esfera cuya interseccion es mas cercana
inline bool intersect(const Ray &r, double &t, int &id) {
	int n = (int)spheres.size();
	t = 1e20;
	for (int i = 0; i < n; i++) {
		double d = spheres[i].intersect(r);
		if (d > 0.0 && d < t) {
			t = d;
			id = i;
		}
	}
	return t < 1e20;
}

inline bool isBlack(const Color &c) { return c.x == 0.0 && c.y == 0.0 && c.z == 0.0; }

// muestreadores de dirección seleccionables por CLI (RF-2)
enum SamplerType { SAMPLER_UNIFORM_SPHERE, SAMPLER_UNIFORM_HEMI, SAMPLER_COSINE_HEMI };
SamplerType samplerType = SAMPLER_COSINE_HEMI;

// Calcula el valor de color para el rayo dado: estimador Monte Carlo de
// iluminación directa (una muestra) para emisores esféricos con materiales difusos.
Color shade(const Ray &r, unsigned short *Xi) {
	double t;
	int id = 0;
	// determinar que esfera (id) y a que distancia (t) el rayo intersecta
	if (!intersect(r, t, id))
		return Color(); // el rayo no intersecto objeto, negro

	const Sphere &obj = spheres[id];

	// aclaración del enunciado: si el rayo de cámara toca directamente un emisor,
	// se regresa su emisión (si no, la fuente aparecería negra)
	if (!isBlack(obj.ke))
		return obj.ke;

	// determinar coordenadas y normal en el punto de interseccion
	Point x = r.o + r.d * t;
	Vector n = (x - obj.p).normalize();

	// base ortonormal local sobre n
	Vector s, tang;
	if (fabs(n.x) > fabs(n.y))
		tang = Vector(n.z, 0, -n.x) * (1.0 / sqrt(n.x * n.x + n.z * n.z));
	else
		tang = Vector(0, n.z, -n.y) * (1.0 / sqrt(n.y * n.y + n.z * n.z));
	s = tang % n;

	// muestrear una dirección local (sinθ·cosφ, sinθ·senφ, cosθ)
	double xi1 = erand48(Xi);
	double xi2 = erand48(Xi);
	double phi = 2 * PI * xi2;
	double cosTheta, pdf;
	switch (samplerType) {
	case SAMPLER_UNIFORM_SPHERE:
		cosTheta = 1 - 2 * xi1;
		pdf = 1.0 / (4 * PI);
		break;
	case SAMPLER_UNIFORM_HEMI:
		cosTheta = xi1;
		pdf = 1.0 / (2 * PI);
		break;
	default: // SAMPLER_COSINE_HEMI
		cosTheta = sqrt(1 - xi1);
		pdf = cosTheta / PI;
		break;
	}
	double sin2 = 1.0 - cosTheta * cosTheta;
	double sinTheta = sin2 > 0.0 ? sqrt(sin2) : 0.0;
	double wx = sinTheta * cos(phi);
	double wy = sinTheta * sin(phi);
	double wz = cosTheta;

	// llevar la dirección local a la base global (s, tang, n)
	Vector wi = s * wx + tang * wy + n * wz;

	// lanzar el rayo de la muestra (la visibilidad va implícita en esta intersección)
	double tShadow;
	int idShadow = 0;
	Color Le;
	if (intersect(Ray(x, wi), tShadow, idShadow))
		Le = spheres[idShadow].ke;

	Color fr = obj.c * (1.0 / PI);
	double cosThetaN = n.dot(wi);
	return fr.mult(Le) * (cosThetaN / pdf);
}

// Calcula el valor de color para el rayo dado: iluminación directa determinista con
// fuentes puntuales (phase-02, RF-1/RF-2). Sin Monte Carlo: un término cerrado por
// fuente, sombras duras por rayo de sombra, 1 muestra por pixel (sin jitter).
Color shadePoint(const Ray &r) {
	double t;
	int id = 0;
	if (!intersect(r, t, id))
		return Color(); // el rayo no intersecto objeto, negro

	const Sphere &obj = spheres[id];
	Point x = r.o + r.d * t;
	Vector n = (x - obj.p).normalize();

	Color L;
	for (size_t i = 0; i < pointLights.size(); i++) {
		const PointLight &light = pointLights[i];

		Vector toLight = light.p - x;
		double r2 = toLight.dot(toLight);
		double dist = sqrt(r2);
		Vector w = toLight * (1.0 / dist);

		double cosTheta = n.dot(w);
		if (cosTheta <= 0.0)
			continue; // fuente detras de la superficie: contribucion 0

		// rayo de sombra: si algo bloquea antes de llegar a la fuente, contribucion 0
		// (epsilon 1e-4 de la fase 0 para evitar acne de sombra)
		double tShadow;
		int idShadow = 0;
		if (intersect(Ray(x, w), tShadow, idShadow) && tShadow < dist - 1e-4)
			continue;

		Color fr = obj.c * (1.0 / PI);
		L = L + fr.mult(light.I) * (cosTheta / r2);
	}
	return L;
}


int main(int argc, char *argv[]) {
	int w = 1024, h = 768; // image resolution

	// CLI: ./rt <sampler|point> <spp>  (spp no aplica en modo "point")
	const char *samplerName = argc > 1 ? argv[1] : "cosinehemi";
	int spp = argc > 2 ? atoi(argv[2]) : 32;
	bool pointMode = strcmp(samplerName, "point") == 0;

	if (strcmp(samplerName, "uniformsphere") == 0)
		samplerType = SAMPLER_UNIFORM_SPHERE;
	else if (strcmp(samplerName, "uniformhemi") == 0)
		samplerType = SAMPLER_UNIFORM_HEMI;
	else
		samplerType = SAMPLER_COSINE_HEMI;

	buildScene(pointMode);

	// fija la posicion de la camara y la dirección en que mira
	Ray camera( Point(0, 11.2, 214), Vector(0, -0.042612, -1).normalize() );

	// parametros de la camara
	Vector cx = Vector( w * 0.5095 / h, 0., 0.);
	Vector cy = (cx % camera.d).normalize() * 0.5095;

	// auxiliar para valor de pixel y matriz para almacenar la imagen
	Color *pixelColors = new Color[w * h];

	// usar openmp para paralelizar el ciclo: cada hilo computara un renglon (ciclo interior),
	// el estado del RNG se siembra por-pixel (RF-4): misma imagen sin importar el numero de hilos
	#pragma omp parallel for schedule(dynamic, 1)
	for(int y = 0; y < h; y++)
	{
		// recorre todos los pixeles de la imagen
		fprintf(stderr,"\r%5.2f%%",100.*y/(h-1));
		for(int x = 0; x < w; x++ ) {
			int idx = (h - y - 1) * w + x; // index en 1D para una imagen 2D x,y son invertidos

			// estado del RNG sembrado deterministicamente con el indice del pixel
			unsigned short Xi[3] = {
				(unsigned short)(idx & 0xFFFF),
				(unsigned short)((idx >> 16) & 0xFFFF),
				0
			};

			// para el pixel actual, computar la dirección que un rayo debe tener
			// (sin jitter: el mismo rayo de cámara para las N muestras del pixel)
			Vector cameraRayDir = cx * ( double(x)/w - .5) + cy * ( double(y)/h - .5) + camera.d;
			Ray primaryRay( camera.o, cameraRayDir.normalize() );

			// modo puntual (phase-02): 1 rayo por pixel, sin MC; modos MC (phase-01):
			// promediar N muestras del estimador Monte Carlo
			Color pixelValue;
			if (pointMode) {
				pixelValue = shadePoint(primaryRay);
			} else {
				Color accum;
				for (int sample = 0; sample < spp; sample++)
					accum = accum + shade(primaryRay, Xi);
				pixelValue = accum * (1.0 / spp);
			}

			// limitar los tres valores de color del pixel a [0,1] (despues de promediar)
			pixelColors[idx] = Color(clamp(pixelValue.x), clamp(pixelValue.y), clamp(pixelValue.z));
		}
	}

	fprintf(stderr,"\n");

	FILE *f = fopen(pointMode ? "image-plight.ppm" : "image.ppm", "w");
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
