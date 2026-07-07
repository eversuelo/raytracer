// rt: un lanzador de rayos minimalista
// g++ -O3 -fopenmp rt.cpp -o rt
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <omp.h>

const double PI = 3.14159265358979323846;

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

// producto cruz que acepta argumentos const (operator% exige referencias no-const)
inline Vector cross(const Vector &a, const Vector &b) {
	return Vector(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

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
	Color ke;	// emision (radiancia propia; (0,0,0) si no emite)

	Sphere(): r(0), p(), c(), ke() {} // default: permite declarar arreglos sin inicializar de inmediato
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

// Escena base (paredes, techo, suelo y dos esferas difusas), común a las dos escenas
// seleccionables por CLI; no incluye emisores.
Sphere sceneBase[] = {
	//                    radio  posicion                       albedo               emision
	Sphere(1e5,  Point(-1e5 - 49, 0, 0),   Color(.75, .25, .25), Color()),          // pared izq
	Sphere(1e5,  Point(1e5 + 49, 0, 0),    Color(.25, .25, .75), Color()),          // pared der
	Sphere(1e5,  Point(0, 0, -1e5 - 81.6), Color(.25, .75, .25), Color()),          // pared detras
	Sphere(1e5,  Point(0, -1e5 - 40.8, 0), Color(.25, .75, .75), Color()),          // suelo
	Sphere(1e5,  Point(0, 1e5 + 40.8, 0),  Color(.75, .75, .25), Color()),          // techo
	Sphere(16.5, Point(-23, -24.3, -34.6), Color(.2, .3, .4),    Color()),          // esfera abajo-izq
	Sphere(16.5, Point(23, -24.3, -3.6),   Color(.4, .3, .2),    Color())           // esfera abajo-der
};
const int N_BASE = sizeof(sceneBase) / sizeof(Sphere);

// Escena "1L" (phase-01): un solo emisor esférico.
Sphere lightsScene1L[] = {
	Sphere(10.5, Point(0, 24.3, 0), Color(1, 1, 1), Color(10, 10, 10))
};

// Escena "2A1P" (Proyecto 3, parte 2): dos emisores de área + una fuente puntual.
Sphere lightsScene2A1P[] = {
	Sphere(10.5, Point(-23, 24.3, 0), Color(1, 1, 1), Color(12, 5, 5)), // emisor rojizo
	Sphere(5,    Point(23, 24.3, -50), Color(1, 1, 1), Color(5, 5, 12)) // emisor azulado
};

// La fuente puntual se guarda en una lista aparte, no dentro de spheres[] (ADR-0005,
// phase-02): así ningún rayo puede intersectarla y no hace falta dar trato especial a
// radio 0 en Sphere::intersect.
struct PointLight { Point p; Color I; };
PointLight pointLightsScene2A1P[] = {
	{ Point(0, 24.3, 0), Color(2000, 2000, 2000) }
};

// Escena activa, construida por buildScene() según el argumento de CLI (RF-4).
const int MAX_SPHERES = N_BASE + 2;
Sphere spheres[MAX_SPHERES];
int nSpheres = 0;

// Índices en spheres[] de los emisores de área de la escena activa (RF-1/RF-2).
int areaLightIdx[2];
int nAreaLights = 0;

PointLight pointLights[1];
int nPointLights = 0;

enum SceneName { SCENE_1L, SCENE_2A1P };

void buildScene(SceneName scene) {
	nSpheres = 0;
	for (int i = 0; i < N_BASE; i++)
		spheres[nSpheres++] = sceneBase[i];

	nAreaLights = 0;
	nPointLights = 0;
	if (scene == SCENE_1L) {
		int n = (int)(sizeof(lightsScene1L) / sizeof(Sphere));
		for (int i = 0; i < n; i++) {
			areaLightIdx[nAreaLights++] = nSpheres;
			spheres[nSpheres++] = lightsScene1L[i];
		}
	} else { // SCENE_2A1P
		int n = (int)(sizeof(lightsScene2A1P) / sizeof(Sphere));
		for (int i = 0; i < n; i++) {
			areaLightIdx[nAreaLights++] = nSpheres;
			spheres[nSpheres++] = lightsScene2A1P[i];
		}
		int np = (int)(sizeof(pointLightsScene2A1P) / sizeof(PointLight));
		for (int i = 0; i < np; i++)
			pointLights[nPointLights++] = pointLightsScene2A1P[i];
	}
}

// calcular la intersección del rayo r con todas las esferas de la escena activa
// regresar true si hubo una intersección, falso de otro modo
// almacenar en t la distancia sobre el rayo en que sucede la interseccion
// almacenar en id el indice de spheres[] de la esfera cuya interseccion es mas cercana
inline bool intersect(const Ray &r, double &t, int &id) {
	t = 1e20;
	bool hitAny = false;
	for (int i = 0; i < nSpheres; i++) {
		double d = spheres[i].intersect(r);
		if (d > 0.0 && d < t) {
			t = d;
			id = i;
			hitAny = true;
		}
	}
	return hitAny;
}

inline bool emits(const Color &ke) { return ke.x != 0.0 || ke.y != 0.0 || ke.z != 0.0; }

// prueba de visibilidad: bloqueada si algo intersecta antes de llegar a dist
// (epsilon de la fase 0, ya incluido en Sphere::intersect, evita acné de sombra)
inline bool visible(const Point &x, const Vector &w, double dist) {
	double tHit; int idHit = 0;
	return !(intersect(Ray(x, w), tHit, idHit) && tHit < dist - 1e-4);
}

// base ortonormal local (s, t) sobre el eje nrm
inline void buildBasis(const Vector &nrm, Vector &s, Vector &t) {
	if (fabs(nrm.x) > fabs(nrm.y))
		t = Vector(nrm.z, 0.0, -nrm.x) * (1.0 / sqrt(nrm.x * nrm.x + nrm.z * nrm.z));
	else
		t = Vector(0.0, nrm.z, -nrm.y) * (1.0 / sqrt(nrm.y * nrm.y + nrm.z * nrm.z));
	s = cross(t, nrm);
}

// Término determinista de las fuentes puntuales (phase-02, RF-3): se suma en cada
// muestra del estimador para cualquier modo — una puntual r=0 nunca es alcanzada por
// un rayo de dirección aleatoria.
Color pointLightTerm(const Point &x, const Vector &n, const Color &albedo) {
	Color L;
	for (int i = 0; i < nPointLights; i++) {
		const PointLight &light = pointLights[i];
		Vector toLight = light.p - x;
		double d2 = toLight.dot(toLight);
		double dist = sqrt(d2);
		Vector w = toLight * (1.0 / dist);

		double cosTheta = n.dot(w);
		if (cosTheta <= 0.0 || !visible(x, w, dist))
			continue;

		Color fr = albedo * (1.0 / PI);
		L = L + fr.mult(light.I) * (cosTheta / d2);
	}
	return L;
}

// RF-1: muestreo de área de una fuente esférica. x' = c + r·ω con ω uniforme en la
// esfera; pdf_ω = d²/(4πr²·cosθ'); contribución (albedo/π)·Le·cosθ/pdf_ω.
Color sampleAreaLight(const Sphere &light, const Point &x, const Vector &n, const Color &albedo, unsigned short Xi[3]) {
	double xi1 = erand48(Xi), xi2 = erand48(Xi);
	double cosThetaL = 1.0 - 2.0 * xi1;
	double sinThetaL = sqrt(fmax(0.0, 1.0 - cosThetaL * cosThetaL));
	double phiL = 2.0 * PI * xi2;
	Vector omega(sinThetaL * cos(phiL), sinThetaL * sin(phiL), cosThetaL);

	Point xp = light.p + omega * light.r;
	Vector toXp = xp - x;
	double d2 = toXp.dot(toXp);
	double d = sqrt(d2);
	Vector wi = toXp * (1.0 / d);

	double cosTheta = n.dot(wi);
	double cosThetaLight = -omega.dot(wi); // n_luz · (dirección x'→x)
	if (cosTheta < 0.0 || cosThetaLight <= 0.0)
		return Color();
	if (!visible(x, wi, d))
		return Color();

	double pdf = d2 / (4.0 * PI * light.r * light.r * cosThetaLight);
	Color fr = albedo * (1.0 / PI);
	return fr.mult(light.ke) * (cosTheta / pdf);
}

// RF-2: muestreo del cono subtendido por la fuente (ángulo sólido). pdf_ω =
// 1/(2π(1−cosθmax)); visible si el primer hit del rayo (x,ω) es esta misma fuente.
Color sampleSolidAngle(const Sphere &light, int lightIdx, const Point &x, const Vector &n, const Color &albedo, unsigned short Xi[3]) {
	Vector toCenter = light.p - x;
	double d2 = toCenter.dot(toCenter);
	double d = sqrt(d2);
	Vector W = toCenter * (1.0 / d);

	double sin2Max = fmin(1.0, (light.r * light.r) / d2);
	double cosThetaMax = sqrt(fmax(0.0, 1.0 - sin2Max));

	Vector s, t;
	buildBasis(W, s, t);

	double xi1 = erand48(Xi), xi2 = erand48(Xi);
	double cosTheta2 = 1.0 - xi1 + xi1 * cosThetaMax;
	double sinTheta2 = sqrt(fmax(0.0, 1.0 - cosTheta2 * cosTheta2));
	double phi = 2.0 * PI * xi2;
	Vector wi = s * (sinTheta2 * cos(phi)) + t * (sinTheta2 * sin(phi)) + W * cosTheta2;

	double cosTheta = n.dot(wi);
	if (cosTheta <= 0.0)
		return Color();

	double tHit; int idHit = 0;
	if (!intersect(Ray(x, wi), tHit, idHit) || idHit != lightIdx)
		return Color();

	double pdf = 1.0 / (2.0 * PI * (1.0 - cosThetaMax));
	Color fr = albedo * (1.0 / PI);
	return fr.mult(light.ke) * (cosTheta / pdf);
}

// Modo "coshemi": estimador coseno-hemisférico de la phase-01 (una muestra) más el
// término determinista de cada fuente puntual (RF-3).
Color shadeCosHemi(const Ray &r, unsigned short Xi[3]) {
	double t; int id = 0;
	if (!intersect(r, t, id))
		return Color(); // el rayo no intersecto objeto: negro

	const Sphere &obj = spheres[id];
	if (emits(obj.ke))
		return obj.ke; // el rayo de camara toca un emisor: su radiancia

	Point x = r.o + r.d * t;
	Vector n = (x - obj.p).normalize();

	Vector s, tg;
	buildBasis(n, s, tg);
	double xi1 = erand48(Xi), xi2 = erand48(Xi);
	double cosTheta = sqrt(1.0 - xi1);
	double sinTheta = sqrt(fmax(0.0, 1.0 - cosTheta * cosTheta));
	double phi = 2.0 * PI * xi2;
	Vector wi = s * (sinTheta * cos(phi)) + tg * (sinTheta * sin(phi)) + n * cosTheta;
	double pdf = cosTheta / PI;

	double t2; int id2 = 0;
	Color Le = intersect(Ray(x, wi), t2, id2) ? spheres[id2].ke : Color();

	Color fr = obj.c * (1.0 / PI);
	Color mcTerm = fr.mult(Le) * (cosTheta / pdf);

	return mcTerm + pointLightTerm(x, n, obj.c);
}

// Modos "arealight"/"solidangle": RF-3 — por cada muestra, itera TODAS las fuentes de
// área con el método elegido y suma sus contribuciones, más el término determinista de
// cada puntual (sin selección aleatoria de fuente, sin dividir entre el número de luces).
enum EstimatorMode { MODE_AREALIGHT, MODE_SOLIDANGLE, MODE_COSHEMI };
EstimatorMode g_mode = MODE_COSHEMI;

Color shadeImportance(const Ray &r, unsigned short Xi[3]) {
	double t; int id = 0;
	if (!intersect(r, t, id))
		return Color();

	const Sphere &obj = spheres[id];
	if (emits(obj.ke))
		return obj.ke;

	Point x = r.o + r.d * t;
	Vector n = (x - obj.p).normalize();

	Color L;
	for (int k = 0; k < nAreaLights; k++) {
		int idx = areaLightIdx[k];
		const Sphere &light = spheres[idx];
		L = L + (g_mode == MODE_AREALIGHT
			? sampleAreaLight(light, x, n, obj.c, Xi)
			: sampleSolidAngle(light, idx, x, n, obj.c, Xi));
	}
	return L + pointLightTerm(x, n, obj.c);
}

// Calcula UNA muestra del color para el rayo dado, según el modo activo.
Color shade(const Ray &r, unsigned short Xi[3]) {
	return (g_mode == MODE_COSHEMI) ? shadeCosHemi(r, Xi) : shadeImportance(r, Xi);
}

int main(int argc, char *argv[]) {
	int w = 1024, h = 768; // image resolution

	// CLI (RF-4): ./rt <modo> <spp> <escena>
	// modo   ∈ {arealight, solidangle, coshemi}
	// escena ∈ {1L, 2A1P}
	const char *modeName = argc > 1 ? argv[1] : "coshemi";
	int spp = argc > 2 ? atoi(argv[2]) : 32;
	const char *sceneName = argc > 3 ? argv[3] : "1L";
	if (spp < 1) spp = 1;

	if (strcmp(modeName, "arealight") == 0) g_mode = MODE_AREALIGHT;
	else if (strcmp(modeName, "solidangle") == 0) g_mode = MODE_SOLIDANGLE;
	else g_mode = MODE_COSHEMI;

	buildScene(strcmp(sceneName, "2A1P") == 0 ? SCENE_2A1P : SCENE_1L);

	// fija la posicion de la camara y la dirección en que mira
	Ray camera( Point(0, 11.2, 214), Vector(0, -0.042612, -1).normalize() );

	// parametros de la camara
	Vector cx = Vector( w * 0.5095 / h, 0., 0.);
	Vector cy = (cx % camera.d).normalize() * 0.5095;

	// auxiliar para valor de pixel y matriz para almacenar la imagen
	Color *pixelColors = new Color[w * h];

	// cada hilo computa un renglon (ciclo interior); el RNG por pixel hace la imagen determinista
	#pragma omp parallel for schedule(dynamic, 1)
	for(int y = 0; y < h; y++)
	{
		fprintf(stderr,"\r%5.2f%%",100.*y/(h-1));
		for(int x = 0; x < w; x++ ) {
			int idx = (h - y - 1) * w + x; // index en 1D para una imagen 2D x,y son invertidos

			// para el pixel actual, el rayo de camara es identico en las N muestras (sin jitter)
			Vector cameraRayDir = cx * ( double(x)/w - .5) + cy * ( double(y)/h - .5) + camera.d;
			Ray cameraRay( camera.o, cameraRayDir.normalize() );

			// estado erand48 sembrado de forma determinista por indice de pixel (x,y):
			// misma imagen con 1 o N hilos, sin rand()/srand() global ni dependencia del hilo
			unsigned int seedIdx = (unsigned int)y * w + x;
			unsigned int mixed = seedIdx * 2654435761u; // hash multiplicativo: decorrelaciona pixeles vecinos
			unsigned short Xi[3] = {
				(unsigned short)(mixed & 0xFFFF),
				(unsigned short)((mixed >> 16) & 0xFFFF),
				(unsigned short)((seedIdx >> 8) ^ 0x330E)
			};

			Color sum;
			for (int s = 0; s < spp; s++)
				sum = sum + shade(cameraRay, Xi);
			Color pixelValue = sum * (1.0 / spp);

			// limitar los tres valores de color del pixel a [0,1] despues de promediar
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
