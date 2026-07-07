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

// Material de la superficie (RF-4): difuso (lambertiano) o conductor áspero (microfacet
// Torrance-Sparrow). alpha es la rugosidad de Beckmann; eta/k el índice de refracción
// complejo por canal RGB (solo válidos para MAT_CONDUCTOR).
enum MaterialType { MAT_DIFFUSE, MAT_CONDUCTOR };

class Sphere
{
public:
	double r;	// radio de la esfera
	Point p;	// posicion
	Color c;	// albedo (color difuso; sin uso en conductores)
	Color ke;	// emision (radiancia propia; (0,0,0) si no emite)
	MaterialType mat;
	double alpha;	// rugosidad Beckmann (conductor)
	Color eta, k;	// indice de refraccion complejo por canal (conductor)

	Sphere(): r(0), p(), c(), ke(), mat(MAT_DIFFUSE), alpha(0), eta(), k() {} // default: permite declarar arreglos sin inicializar de inmediato
	Sphere(double r_, Point p_, Color c_, Color ke_,
	       MaterialType mat_ = MAT_DIFFUSE, double alpha_ = 0.0,
	       Color eta_ = Color(), Color k_ = Color())
		: r(r_), p(p_), c(c_), ke(ke_), mat(mat_), alpha(alpha_), eta(eta_), k(k_) {}

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

// recorta cada canal a >=0 (conservación de energía: ninguna contribución debe ser negativa)
inline Color clampNonNeg(const Color &c) {
	return Color(c.x > 0.0 ? c.x : 0.0, c.y > 0.0 ? c.y : 0.0, c.z > 0.0 ? c.z : 0.0);
}

// convierte un valor de color en [0,1] a un entero en [0,255]
inline int toDisplayValue(const double x) {
	return int( pow( clamp(x), 1.0/2.2 ) * 255 + .5);
}

// Escena base (paredes, techo, suelo y los dos conductores ásperos — RF-1 phase-04:
// reemplazan las esferas difusas r=16.5 de fases previas), común a las tres escenas
// seleccionables por CLI; no incluye emisores.
Sphere sceneBase[] = {
	//                    radio  posicion                       albedo               emision
	Sphere(1e5,  Point(-1e5 - 49, 0, 0),   Color(.75, .25, .25), Color()),          // pared izq
	Sphere(1e5,  Point(1e5 + 49, 0, 0),    Color(.25, .25, .75), Color()),          // pared der
	Sphere(1e5,  Point(0, 0, -1e5 - 81.6), Color(.25, .75, .25), Color()),          // pared detras
	Sphere(1e5,  Point(0, -1e5 - 40.8, 0), Color(.25, .75, .75), Color()),          // suelo
	Sphere(1e5,  Point(0, 1e5 + 40.8, 0),  Color(.75, .75, .25), Color()),          // techo
	Sphere(16.5, Point(-23, -24.3, -34.6), Color(), Color(), MAT_CONDUCTOR, 0.3,
	       Color(1.66058, 0.88143, 0.521467), Color(9.2282, 6.27077, 4.83803)),     // aluminio (abajo-izq)
	Sphere(16.5, Point(23, -24.3, -3.6),   Color(), Color(), MAT_CONDUCTOR, 0.3,
	       Color(0.143245, 0.377423, 1.43919), Color(3.98479, 2.3847, 1.60434))     // oro (abajo-der)
};
const int N_BASE = sizeof(sceneBase) / sizeof(Sphere);

// Escena "areal": un solo emisor esférico (la "1L" de las fases 01/03).
Sphere lightsSceneAreal[] = {
	Sphere(10.5, Point(0, 24.3, 0), Color(1, 1, 1), Color(10, 10, 10))
};

// Escena "2a1p" (Proyecto 3, parte 2): dos emisores de área + una fuente puntual.
Sphere lightsScene2A1P[] = {
	Sphere(10.5, Point(-23, 24.3, 0), Color(1, 1, 1), Color(12, 5, 5)), // emisor rojizo
	Sphere(5,    Point(23, 24.3, -50), Color(1, 1, 1), Color(5, 5, 12)) // emisor azulado
};

// La fuente puntual se guarda en una lista aparte, no dentro de spheres[] (ADR-0005,
// phase-02): así ningún rayo puede intersectarla y no hace falta dar trato especial a
// radio 0 en Sphere::intersect.
struct PointLight { Point p; Color I; };
PointLight pointLightsScenePoint[] = {
	{ Point(0, 24.3, 0), Color(2000, 2000, 2000) }
};
PointLight pointLightsScene2A1P[] = {
	{ Point(0, 24.3, 0), Color(2000, 2000, 2000) }
};

// Escena activa, construida por buildScene() según el argumento de CLI (ADR-0006).
const int MAX_SPHERES = N_BASE + 2;
Sphere spheres[MAX_SPHERES];
int nSpheres = 0;

// Índices en spheres[] de los emisores de área de la escena activa (RF-2, phase-03).
int areaLightIdx[2];
int nAreaLights = 0;

PointLight pointLights[1];
int nPointLights = 0;

enum SceneName { SCENE_POINT, SCENE_AREAL, SCENE_2A1P };

void buildScene(SceneName scene) {
	nSpheres = 0;
	for (int i = 0; i < N_BASE; i++)
		spheres[nSpheres++] = sceneBase[i];

	nAreaLights = 0;
	nPointLights = 0;
	if (scene == SCENE_POINT) {
		pointLights[nPointLights++] = pointLightsScenePoint[0];
	} else if (scene == SCENE_AREAL) {
		areaLightIdx[nAreaLights++] = nSpheres;
		spheres[nSpheres++] = lightsSceneAreal[0];
	} else { // SCENE_2A1P
		int n = (int)(sizeof(lightsScene2A1P) / sizeof(Sphere));
		for (int i = 0; i < n; i++) {
			areaLightIdx[nAreaLights++] = nSpheres;
			spheres[nSpheres++] = lightsScene2A1P[i];
		}
		pointLights[nPointLights++] = pointLightsScene2A1P[0];
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

// ─── RF-1: BRDF microfacet de conductor (Torrance-Sparrow: D de Beckmann, G de Smith,
// Fresnel exacto por canal) ─────────────────────────────────────────────────────────

// D(ωh) = χ+(cosθh)·exp(−tan²θh/α²)/(π·α²·cos⁴θh)
inline double beckmannD(double cosThetaH, double alpha) {
	if (cosThetaH <= 0.0)
		return 0.0;
	double cos2 = cosThetaH * cosThetaH;
	double cos4 = cos2 * cos2;
	double tan2 = (1.0 - cos2) / cos2;
	double alpha2 = alpha * alpha;
	return exp(-tan2 / alpha2) / (PI * alpha2 * cos4);
}

// G1 de Smith (aproximación racional de Beckmann): a = 1/(α·tanθv); χ+((v·ωh)/(v·n))
// descarta microfacetas del lado equivocado del hemisferio.
inline double smithG1(const Vector &v, const Vector &n, const Vector &wh, double alpha) {
	double vDotN = v.dot(n);
	if (vDotN <= 0.0)
		return 0.0;
	if (v.dot(wh) / vDotN <= 0.0)
		return 0.0;
	double sin2 = 1.0 - vDotN * vDotN;
	if (sin2 <= 1e-12)
		return 1.0; // θv ~ 0 => tanθv ~ 0 => a → ∞ => G1 = 1
	double tanThetaV = sqrt(sin2) / vDotN;
	double a = 1.0 / (alpha * tanThetaV);
	if (a >= 1.6)
		return 1.0;
	double a2 = a * a;
	return (3.535 * a + 2.181 * a2) / (1.0 + 2.276 * a + 2.577 * a2);
}

// Fresnel de conductor exacto (Born & Wolf) por canal, medio incidente η=1.
inline double fresnelConductorChannel(double cosTheta, double eta, double k) {
	cosTheta = fabs(cosTheta);
	double cos2 = cosTheta * cosTheta;
	double sin2 = 1.0 - cos2;
	double sin4 = sin2 * sin2;
	double eta2 = eta * eta, k2 = k * k;
	double t0 = eta2 - k2 - sin2;
	double a2b2 = sqrt(fmax(0.0, t0 * t0 + 4.0 * eta2 * k2));
	double A = sqrt(fmax(0.0, (a2b2 + t0) / 2.0));
	double twoAcos = 2.0 * A * cosTheta;

	double Rperp = (a2b2 + cos2 - twoAcos) / (a2b2 + cos2 + twoAcos);
	double num = a2b2 * cos2 + sin4 - twoAcos * sin2;
	double den = a2b2 * cos2 + sin4 + twoAcos * sin2;
	double Rpar = Rperp * (num / den);

	return clamp(0.5 * (Rperp + Rpar));
}

inline Color fresnelConductor(double cosTheta, const Color &eta, const Color &k) {
	return Color(fresnelConductorChannel(cosTheta, eta.x, k.x),
	             fresnelConductorChannel(cosTheta, eta.y, k.y),
	             fresnelConductorChannel(cosTheta, eta.z, k.z));
}

// fr = D·G·F / (4·|n·ωi|·|n·ωo|); 0 si ωi u ωo quedan bajo el hemisferio.
inline Color evalConductor(const Sphere &obj, const Vector &n, const Vector &wo, const Vector &wi) {
	double cosO = n.dot(wo);
	double cosI = n.dot(wi);
	if (cosO <= 0.0 || cosI <= 0.0)
		return Color();

	Vector wh = wo + wi;
	double whLen = sqrt(wh.dot(wh));
	if (whLen < 1e-9)
		return Color();
	wh = wh * (1.0 / whLen);

	double D = beckmannD(n.dot(wh), obj.alpha);
	if (D <= 0.0)
		return Color();
	double G = smithG1(wo, n, wh, obj.alpha) * smithG1(wi, n, wh, obj.alpha);
	Color F = fresnelConductor(wo.dot(wh), obj.eta, obj.k); // wo·wh == wi·wh (wh bisecta ambos)

	return F * (D * G / (4.0 * cosO * cosI));
}

// fr = ρ/π (lambertiano); 0 si wi queda bajo el hemisferio.
inline Color evalDiffuse(const Sphere &obj, const Vector &n, const Vector &wi) {
	if (n.dot(wi) <= 0.0)
		return Color();
	return obj.c * (1.0 / PI);
}

// RF-4: API del material — eval(ωo, ωi) → fr.
inline Color materialEval(const Sphere &obj, const Vector &n, const Vector &wo, const Vector &wi) {
	return (obj.mat == MAT_CONDUCTOR) ? evalConductor(obj, n, wo, wi) : evalDiffuse(obj, n, wi);
}

// RF-4: API del material — pdf(ωo, ωi) → p. Difuso: cosθi/π. Conductor (muestreo del
// half-vector de Beckmann): pdf(ωi) = D(ωh)·cosθh/(4|ωo·ωh|).
inline double materialPdf(const Sphere &obj, const Vector &n, const Vector &wo, const Vector &wi) {
	if (obj.mat == MAT_DIFFUSE) {
		double cosI = n.dot(wi);
		return cosI > 0.0 ? cosI / PI : 0.0;
	}
	Vector wh = wo + wi;
	double whLen = sqrt(wh.dot(wh));
	if (whLen < 1e-9)
		return 0.0;
	wh = wh * (1.0 / whLen);
	double whDotWo = wh.dot(wo);
	if (whDotWo <= 0.0)
		return 0.0;
	double D = beckmannD(n.dot(wh), obj.alpha);
	return D * n.dot(wh) / (4.0 * whDotWo);
}

// RF-4: API del material — sample(ωo) → ωi. Difuso: coseno hemisférico
// (θ=acos(√(1−ξ₁)), φ=2πξ₂). Conductor: half-vector de Beckmann
// (θh=atan(√(−α²·ln(1−ξ₁))), φh=2πξ₂), refleja ωo alrededor de ωh.
inline Vector materialSample(const Sphere &obj, const Vector &n, const Vector &wo, unsigned short Xi[3]) {
	double xi1 = erand48(Xi), xi2 = erand48(Xi);
	Vector s, t;
	buildBasis(n, s, t);

	if (obj.mat == MAT_DIFFUSE) {
		double cosTheta = sqrt(1.0 - xi1);
		double sinTheta = sqrt(fmax(0.0, 1.0 - cosTheta * cosTheta));
		double phi = 2.0 * PI * xi2;
		return s * (sinTheta * cos(phi)) + t * (sinTheta * sin(phi)) + n * cosTheta;
	}

	double thetaH = atan(sqrt(-obj.alpha * obj.alpha * log(1.0 - xi1)));
	double phiH = 2.0 * PI * xi2;
	double cosThetaH = cos(thetaH), sinThetaH = sin(thetaH);
	Vector wh = s * (sinThetaH * cos(phiH)) + t * (sinThetaH * sin(phiH)) + n * cosThetaH;
	return wh * (2.0 * wh.dot(wo)) - wo;
}

// Término determinista de las fuentes puntuales (phase-02/03; RF-3 phase-04): se suma en
// cada muestra del estimador para cualquier modo (una puntual r=0 nunca es alcanzada por
// un rayo aleatorio), evaluando la BRDF general del material del punto x.
Color pointLightTerm(const Point &x, const Vector &n, const Vector &wo, const Sphere &obj) {
	Color L;
	for (int i = 0; i < nPointLights; i++) {
		const PointLight &light = pointLights[i];
		Vector toLight = light.p - x;
		double d2 = toLight.dot(toLight);
		double dist = sqrt(d2);
		Vector wi = toLight * (1.0 / dist);

		double cosTheta = n.dot(wi);
		if (cosTheta <= 0.0 || !visible(x, wi, dist))
			continue;

		Color fr = materialEval(obj, n, wo, wi);
		L = L + clampNonNeg(fr.mult(light.I) * (cosTheta / d2));
	}
	return L;
}

// RF-2: muestreo de área de una fuente esférica. x' = c + r·ω con ω uniforme en la
// esfera; pdf_ω = d²/(4πr²·cosθ'); contribución fr(ωo,ωi)·Le·cosθ/pdf_ω, evaluando la
// BRDF general del material del punto sombreado (RF-1: microfacet en conductores).
Color sampleAreaLight(const Sphere &light, const Sphere &obj, const Point &x, const Vector &n,
                       const Vector &wo, unsigned short Xi[3]) {
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
	Color fr = materialEval(obj, n, wo, wi);
	return clampNonNeg(fr.mult(light.ke) * (cosTheta / pdf));
}

// RF-2: muestreo del cono subtendido por la fuente (ángulo sólido). pdf_ω =
// 1/(2π(1−cosθmax)); visible si el primer hit del rayo (x,ω) es esta misma fuente.
Color sampleSolidAngle(const Sphere &light, int lightIdx, const Sphere &obj, const Point &x,
                        const Vector &n, const Vector &wo, unsigned short Xi[3]) {
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
	Color fr = materialEval(obj, n, wo, wi);
	return clampNonNeg(fr.mult(light.ke) * (cosTheta / pdf));
}

// Modos "isarea"/"issa" (RF-2 phase-03/04): por cada muestra, itera TODAS las fuentes de
// área con el método elegido y suma sus contribuciones, más el término determinista de
// cada puntual (sin selección aleatoria de fuente, sin dividir entre el número de luces).
enum EstimatorMode { MODE_ISAREA, MODE_ISSA, MODE_ISBRDF };
EstimatorMode g_mode = MODE_ISSA;

Color shadeImportance(const Ray &r, unsigned short Xi[3]) {
	double t; int id = 0;
	if (!intersect(r, t, id))
		return Color();

	const Sphere &obj = spheres[id];
	if (emits(obj.ke))
		return obj.ke;

	Point x = r.o + r.d * t;
	Vector n = (x - obj.p).normalize();
	Vector wo = r.d * -1.0;

	Color L;
	for (int k = 0; k < nAreaLights; k++) {
		int idx = areaLightIdx[k];
		const Sphere &light = spheres[idx];
		L = L + (g_mode == MODE_ISAREA
			? sampleAreaLight(light, obj, x, n, wo, Xi)
			: sampleSolidAngle(light, idx, obj, x, n, wo, Xi));
	}
	return L + pointLightTerm(x, n, wo, obj);
}

// Modo "isbrdf" (RF-3 phase-04): muestrea ωi según la distribución del material (coseno
// hemisférico en difusos, half-vector de Beckmann en conductores) y solo suma si el rayo
// alcanza un emisor: fr⊙Le·(n·ωi)/pdf. Más el término determinista de cada puntual.
Color shadeBrdf(const Ray &r, unsigned short Xi[3]) {
	double t; int id = 0;
	if (!intersect(r, t, id))
		return Color();

	const Sphere &obj = spheres[id];
	if (emits(obj.ke))
		return obj.ke;

	Point x = r.o + r.d * t;
	Vector n = (x - obj.p).normalize();
	Vector wo = r.d * -1.0;

	Vector wi = materialSample(obj, n, wo, Xi);
	Color mcTerm;
	double cosI = n.dot(wi);
	if (cosI > 0.0) {
		double t2; int id2 = 0;
		if (intersect(Ray(x, wi), t2, id2) && emits(spheres[id2].ke)) {
			double pdf = materialPdf(obj, n, wo, wi);
			if (pdf > 0.0) {
				Color fr = materialEval(obj, n, wo, wi);
				mcTerm = clampNonNeg(fr.mult(spheres[id2].ke) * (cosI / pdf));
			}
		}
	}

	return mcTerm + pointLightTerm(x, n, wo, obj);
}

// Calcula UNA muestra del color para el rayo dado, según el modo activo.
Color shade(const Ray &r, unsigned short Xi[3]) {
	return (g_mode == MODE_ISBRDF) ? shadeBrdf(r, Xi) : shadeImportance(r, Xi);
}

int main(int argc, char *argv[]) {
	int w = 1024, h = 768; // image resolution

	// CLI (mismo orden que la fase 3): ./rt <modo> <spp> <escena>
	// modo   ∈ {issa, isarea, isbrdf}
	// escena ∈ {point, areal, 2a1p}
	const char *modeName = argc > 1 ? argv[1] : "issa";
	int spp = argc > 2 ? atoi(argv[2]) : 32;
	const char *sceneName = argc > 3 ? argv[3] : "areal";
	if (spp < 1) spp = 1;

	if (strcmp(modeName, "isarea") == 0) g_mode = MODE_ISAREA;
	else if (strcmp(modeName, "isbrdf") == 0) g_mode = MODE_ISBRDF;
	else g_mode = MODE_ISSA;

	SceneName scene = SCENE_AREAL;
	if (strcmp(sceneName, "point") == 0) scene = SCENE_POINT;
	else if (strcmp(sceneName, "2a1p") == 0) scene = SCENE_2A1P;
	buildScene(scene);

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
