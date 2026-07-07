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

// Escena: paredes+suelo+techo difusos y dos esferas difusas (comunes a las dos escenas).
// Las fuentes de área (esferas con ke != 0) viven en spheres[] para que los rayos de
// cámara y de sombra las intersecten como cualquier objeto; sus índices se cachean en
// areaLightIdx para el muestreo de importancia (RF-1/RF-2). La fuente puntual (phase-02)
// sigue en una lista aparte: así ningún rayo la intersecta jamás.
std::vector<Sphere> spheres;
std::vector<int> areaLightIdx;

class PointLight
{
public:
	Point p;  // posicion
	Color I;  // intensidad radiante

	PointLight(Point p_, Color I_) : p(p_), I(I_) {}
};
std::vector<PointLight> pointLights;

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

// escenas seleccionables por CLI (RF-4): 1L = escena de la phase-01 (un emisor de área);
// 2A1P = dos emisores de área + una puntual (Proyecto 3, parte 2)
enum SceneType { SCENE_1L, SCENE_2A1P };

void buildScene(SceneType scene) {
	//                    radio  posicion                       albedo              emision
	spheres.push_back(Sphere(1e5,  Point(-1e5 - 49, 0, 0),   Color(.75, .25, .25), Color()));          // pared izq
	spheres.push_back(Sphere(1e5,  Point(1e5 + 49, 0, 0),    Color(.25, .25, .75), Color()));          // pared der
	spheres.push_back(Sphere(1e5,  Point(0, 0, -1e5 - 81.6), Color(.25, .75, .25), Color()));          // pared detras
	spheres.push_back(Sphere(1e5,  Point(0, -1e5 - 40.8, 0), Color(.25, .75, .75), Color()));          // suelo
	spheres.push_back(Sphere(1e5,  Point(0, 1e5 + 40.8, 0),  Color(.75, .75, .25), Color()));          // techo
	spheres.push_back(Sphere(16.5, Point(-23, -24.3, -34.6), Color(.2, .3, .4),    Color()));          // esfera abajo-izq
	spheres.push_back(Sphere(16.5, Point(23, -24.3, -3.6),   Color(.4, .3, .2),    Color()));          // esfera abajo-der

	if (scene == SCENE_1L) {
		spheres.push_back(Sphere(10.5, Point(0, 24.3, 0), Color(1, 1, 1), Color(10, 10, 10)));         // emisor único
	} else { // SCENE_2A1P
		spheres.push_back(Sphere(10.5, Point(-23, 24.3, 0),   Color(1, 1, 1), Color(12, 5, 5)));       // emisor rojizo
		spheres.push_back(Sphere(5,    Point(23, 24.3, -50),  Color(1, 1, 1), Color(5, 5, 12)));       // emisor azulado
		pointLights.push_back(PointLight(Point(0, 24.3, 0), Color(2000, 2000, 2000)));                 // puntual
	}

	for (size_t i = 0; i < spheres.size(); i++)
		if (!isBlack(spheres[i].ke))
			areaLightIdx.push_back((int)i);
}

// base ortonormal local (s, tang) sobre el eje nrm (uniforme en phi, cualquier rotación sirve)
inline void buildBasis(Vector nrm, Vector &s, Vector &tang) {
	if (fabs(nrm.x) > fabs(nrm.y))
		tang = Vector(nrm.z, 0, -nrm.x) * (1.0 / sqrt(nrm.x * nrm.x + nrm.z * nrm.z));
	else
		tang = Vector(0, nrm.z, -nrm.y) * (1.0 / sqrt(nrm.y * nrm.y + nrm.z * nrm.z));
	s = tang % nrm;
}

// prueba de visibilidad: rayo de sombra desde x en dirección w; bloqueado si algo
// intersecta antes de llegar a dist (epsilon de la fase 0 para evitar acné de sombra)
inline bool visible(const Point &x, const Vector &w, double dist) {
	double tHit; int idHit = 0;
	if (intersect(Ray(x, w), tHit, idHit) && tHit < dist - 1e-4)
		return false;
	return true;
}

// término determinista de las fuentes puntuales (phase-02, RF-1/RF-2): se suma en cada
// muestra del estimador, para cualquier modo (RF-3 — una puntual r=0 nunca es alcanzada
// por un rayo aleatorio, así que necesita su propio término cerrado)
inline Color pointLightTerm(const Point &x, const Vector &n, const Color &albedo) {
	Color L;
	for (size_t i = 0; i < pointLights.size(); i++) {
		const PointLight &light = pointLights[i];

		Vector toLight = light.p - x;
		double r2 = toLight.dot(toLight);
		double dist = sqrt(r2);
		Vector w = toLight * (1.0 / dist);

		double cosTheta = n.dot(w);
		if (cosTheta <= 0.0)
			continue;
		if (!visible(x, w, dist))
			continue;

		Color fr = albedo * (1.0 / PI);
		L = L + fr.mult(light.I) * (cosTheta / r2);
	}
	return L;
}

// métodos de muestreo de importancia de fuentes esféricas, seleccionables por CLI (RF-4)
enum EstimatorMode { MODE_AREALIGHT, MODE_SOLIDANGLE, MODE_COSHEMI };
EstimatorMode estimatorMode = MODE_COSHEMI;

// RF-1: muestreo de área. x' = c + r·ω con ω uniforme en la esfera; pdf_ω = d²/(4πr²·cosθ');
// contribución (albedo/π)·Le·cosθ/pdf_ω con rayo de sombra hacia x'.
Color sampleAreaLight(const Sphere &light, const Point &x, const Vector &n, const Color &albedo, unsigned short *Xi) {
	double xi1 = erand48(Xi);
	double xi2 = erand48(Xi);
	double cosThetaL = 1 - 2 * xi1;
	double sin2L = 1.0 - cosThetaL * cosThetaL;
	double sinThetaL = sin2L > 0.0 ? sqrt(sin2L) : 0.0;
	double phiL = 2 * PI * xi2;
	Vector omega(sinThetaL * cos(phiL), sinThetaL * sin(phiL), cosThetaL);

	Point xp = light.p + omega * light.r;
	Vector toXp = xp - x;
	double d2 = toXp.dot(toXp);
	double d = sqrt(d2);
	Vector wi = toXp * (1.0 / d);

	double cosThetaSurface = n.dot(wi);
	double cosThetaLight = -omega.dot(wi); // n_luz · (dirección x'→x)
	if (cosThetaSurface <= 0.0 || cosThetaLight <= 0.0)
		return Color();
	if (!visible(x, wi, d))
		return Color();

	double pdf = d2 / (4 * PI * light.r * light.r * cosThetaLight);
	Color fr = albedo * (1.0 / PI);
	return fr.mult(light.ke) * (cosThetaSurface / pdf);
}

// RF-2: muestreo del ángulo sólido (cono hacia la fuente). pdf_ω = 1/(2π(1−cosθmax));
// visible si el PRIMER hit del rayo (x, ω) es esta misma fuente. Misma contribución que RF-1.
Color sampleSolidAngle(const Sphere &light, int lightIdx, const Point &x, const Vector &n, const Color &albedo, unsigned short *Xi) {
	Vector toCenter = light.p - x;
	double dist2 = toCenter.dot(toCenter);
	double dist = sqrt(dist2);
	Vector W = toCenter * (1.0 / dist);

	double sinMax2 = (light.r * light.r) / dist2;
	double cosThetaMax = sinMax2 < 1.0 ? sqrt(1.0 - sinMax2) : 0.0;

	Vector s, tang;
	buildBasis(W, s, tang);

	double xi1 = erand48(Xi);
	double xi2 = erand48(Xi);
	double cosTheta = 1 - xi1 + xi1 * cosThetaMax;
	double sin2 = 1.0 - cosTheta * cosTheta;
	double sinTheta = sin2 > 0.0 ? sqrt(sin2) : 0.0;
	double phi = 2 * PI * xi2;
	Vector wi = s * (sinTheta * cos(phi)) + tang * (sinTheta * sin(phi)) + W * cosTheta;

	double cosThetaSurface = n.dot(wi);
	if (cosThetaSurface <= 0.0)
		return Color();

	double tHit; int idHit = 0;
	if (!intersect(Ray(x, wi), tHit, idHit) || idHit != lightIdx)
		return Color();

	double pdf = 1.0 / (2 * PI * (1 - cosThetaMax));
	Color fr = albedo * (1.0 / PI);
	return fr.mult(light.ke) * (cosThetaSurface / pdf);
}

// Modo "coshemi": estimador coseno-hemisférico de la phase-01 (una muestra) más el
// término determinista de cada fuente puntual (RF-3).
Color shadeCosHemi(const Ray &r, unsigned short *Xi) {
	double t; int id = 0;
	if (!intersect(r, t, id))
		return Color(); // el rayo no intersecto objeto, negro

	const Sphere &obj = spheres[id];

	// si el rayo de cámara toca directamente un emisor, se regresa su emisión
	if (!isBlack(obj.ke))
		return obj.ke;

	Point x = r.o + r.d * t;
	Vector n = (x - obj.p).normalize();

	Vector s, tang;
	buildBasis(n, s, tang);

	double xi1 = erand48(Xi);
	double xi2 = erand48(Xi);
	double phi = 2 * PI * xi2;
	double cosTheta = sqrt(1 - xi1);
	double pdf = cosTheta / PI;
	double sin2 = 1.0 - cosTheta * cosTheta;
	double sinTheta = sin2 > 0.0 ? sqrt(sin2) : 0.0;
	Vector wi = s * (sinTheta * cos(phi)) + tang * (sinTheta * sin(phi)) + n * cosTheta;

	double tShadow; int idShadow = 0;
	Color Le;
	if (intersect(Ray(x, wi), tShadow, idShadow))
		Le = spheres[idShadow].ke;

	Color fr = obj.c * (1.0 / PI);
	double cosThetaN = n.dot(wi);
	Color mcTerm = fr.mult(Le) * (cosThetaN / pdf);

	return mcTerm + pointLightTerm(x, n, obj.c);
}

// Modos "arealight"/"solidangle": RF-3 — por cada muestra, itera TODAS las fuentes de
// área con el método elegido y SUMA sus contribuciones, más el término determinista de
// cada puntual (sin selección aleatoria de fuente, sin dividir entre el número de luces).
Color shadeImportance(const Ray &r, unsigned short *Xi, EstimatorMode mode) {
	double t; int id = 0;
	if (!intersect(r, t, id))
		return Color();

	const Sphere &obj = spheres[id];
	if (!isBlack(obj.ke))
		return obj.ke;

	Point x = r.o + r.d * t;
	Vector n = (x - obj.p).normalize();

	Color value;
	for (size_t k = 0; k < areaLightIdx.size(); k++) {
		int lightIdx = areaLightIdx[k];
		const Sphere &light = spheres[lightIdx];
		if (mode == MODE_AREALIGHT)
			value = value + sampleAreaLight(light, x, n, obj.c, Xi);
		else
			value = value + sampleSolidAngle(light, lightIdx, x, n, obj.c, Xi);
	}
	value = value + pointLightTerm(x, n, obj.c);
	return value;
}

int main(int argc, char *argv[]) {
	int w = 1024, h = 768; // image resolution

	// CLI (RF-4): ./rt <modo> <spp> <escena>
	// modo   ∈ {arealight, solidangle, coshemi}
	// escena ∈ {1L, 2A1P}
	const char *modeName = argc > 1 ? argv[1] : "coshemi";
	int spp = argc > 2 ? atoi(argv[2]) : 32;
	const char *sceneName = argc > 3 ? argv[3] : "1L";

	if (strcmp(modeName, "arealight") == 0)
		estimatorMode = MODE_AREALIGHT;
	else if (strcmp(modeName, "solidangle") == 0)
		estimatorMode = MODE_SOLIDANGLE;
	else
		estimatorMode = MODE_COSHEMI;

	SceneType scene = (strcmp(sceneName, "2A1P") == 0) ? SCENE_2A1P : SCENE_1L;
	buildScene(scene);

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

			// promediar N muestras del estimador Monte Carlo (RF-3: cada muestra ya
			// suma todas las fuentes de la escena)
			Color accum;
			for (int sample = 0; sample < spp; sample++) {
				Color s = (estimatorMode == MODE_COSHEMI)
					? shadeCosHemi(primaryRay, Xi)
					: shadeImportance(primaryRay, Xi, estimatorMode);
				accum = accum + s;
			}
			Color pixelValue = accum * (1.0 / spp);

			// limitar los tres valores de color del pixel a [0,1] (despues de promediar)
			pixelColors[idx] = Color(clamp(pixelValue.x), clamp(pixelValue.y), clamp(pixelValue.z));
		}
	}

	fprintf(stderr,"\n");

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
