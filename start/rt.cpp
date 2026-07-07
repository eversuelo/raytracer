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

// Materiales de la escena (RF-4): difuso (lambertiano) o conductor áspero (microfacet).
// El conductor guarda alpha (rugosidad Beckmann) e (eta,k) — índice de refracción
// complejo por canal RGB. albedo (c) solo aplica a MAT_DIFFUSE.
enum MaterialType { MAT_DIFFUSE, MAT_CONDUCTOR };

class Sphere
{
public:
	double r;	// radio de la esfera
	Point p;	// posicion
	Color c;	// color (albedo, difuso)
	Color ke;	// emision
	MaterialType mat;
	double alpha;	// rugosidad Beckmann (conductor)
	Color eta, k;	// indice de refraccion complejo por canal (conductor)

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

inline Color clampNonNeg(const Color &c) {
	return Color(c.x > 0.0 ? c.x : 0.0, c.y > 0.0 ? c.y : 0.0, c.z > 0.0 ? c.z : 0.0);
}

// convierte un valor de color en [0,1] a un entero en [0,255]
inline int toDisplayValue(const double x) {
	return int( pow( clamp(x), 1.0/2.2 ) * 255 + .5);
}

// Escena: paredes+suelo+techo difusos y dos esferas conductoras ásperas (RF-1: phase-04
// reemplaza las difusas de phases previas por aluminio/oro). Las fuentes de área (esferas
// con ke != 0) viven en spheres[] para que los rayos de cámara y de sombra las intersecten
// como cualquier objeto; sus índices se cachean en areaLightIdx para el muestreo de
// importancia. La fuente puntual sigue en una lista aparte: así ningún rayo la intersecta.
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

// escenas seleccionables por CLI: point = solo fuente puntual; areal = un único emisor
// de área (usada también por el muestreo de BRDF, parte 2); 2a1p = dos emisores de área
// más una puntual (la escena de la phase-03)
enum SceneType { SCENE_POINT, SCENE_AREAL, SCENE_2A1P };

void buildScene(SceneType scene) {
	//                    radio  posicion                       albedo              emision
	spheres.push_back(Sphere(1e5,  Point(-1e5 - 49, 0, 0),   Color(.75, .25, .25), Color()));          // pared izq
	spheres.push_back(Sphere(1e5,  Point(1e5 + 49, 0, 0),    Color(.25, .25, .75), Color()));          // pared der
	spheres.push_back(Sphere(1e5,  Point(0, 0, -1e5 - 81.6), Color(.25, .75, .25), Color()));          // pared detras
	spheres.push_back(Sphere(1e5,  Point(0, -1e5 - 40.8, 0), Color(.25, .75, .75), Color()));          // suelo
	spheres.push_back(Sphere(1e5,  Point(0, 1e5 + 40.8, 0),  Color(.75, .75, .25), Color()));          // techo

	// RF-1: conductores ásperos alpha=0.3 (reemplazan las esferas difusas r=16.5)
	spheres.push_back(Sphere(16.5, Point(-23, -24.3, -34.6), Color(), Color(),
		MAT_CONDUCTOR, 0.3,
		Color(1.66058, 0.88143, 0.521467), Color(9.2282, 6.27077, 4.83803)));                          // aluminio
	spheres.push_back(Sphere(16.5, Point(23, -24.3, -3.6), Color(), Color(),
		MAT_CONDUCTOR, 0.3,
		Color(0.143245, 0.377423, 1.43919), Color(3.98479, 2.3847, 1.60434)));                         // oro

	if (scene == SCENE_POINT) {
		pointLights.push_back(PointLight(Point(0, 24.3, 0), Color(2000, 2000, 2000)));                 // puntual
	} else if (scene == SCENE_AREAL) {
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

// ─── RF-1: BRDF microfacet de conductor (Beckmann D + Smith G + Fresnel exacto) ───────

// D de Beckmann: D(wh) = chi+(cosThetaH)*exp(-tan^2(thetaH)/alpha^2) / (pi*alpha^2*cos^4(thetaH))
inline double beckmannD(double cosThetaH, double alpha) {
	if (cosThetaH <= 0.0)
		return 0.0;
	double cos2 = cosThetaH * cosThetaH;
	double cos4 = cos2 * cos2;
	double tan2 = (1.0 - cos2) / cos2;
	double alpha2 = alpha * alpha;
	return exp(-tan2 / alpha2) / (PI * alpha2 * cos4);
}

// G1 de Smith (aproximación racional de Beckmann): a = 1/(alpha*tanThetaV), con
// chi+((v.wh)/(v.n)) para descartar microfacetas en el lado equivocado.
inline double smithG1(const Vector &v, const Vector &n, const Vector &wh, double alpha) {
	double vDotN = v.dot(n);
	if (vDotN <= 0.0)
		return 0.0;
	double vDotWh = v.dot(wh);
	if (vDotWh / vDotN <= 0.0)
		return 0.0;
	double sin2ThetaV = 1.0 - vDotN * vDotN;
	if (sin2ThetaV <= 1e-12)
		return 1.0; // thetaV ~ 0 => tanThetaV ~ 0 => a -> inf => G1 = 1
	double tanThetaV = sqrt(sin2ThetaV) / vDotN;
	double a = 1.0 / (alpha * tanThetaV);
	if (a >= 1.6)
		return 1.0;
	double a2 = a * a;
	return (3.535 * a + 2.181 * a2) / (1.0 + 2.276 * a + 2.577 * a2);
}

// Fresnel de conductor exacto (Born & Wolf) para un canal, medio incidente eta=1.
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

	return clamp(0.5 * (Rperp + Rpar)); // conservación de energía: F en [0,1]
}

inline Color fresnelConductor(double cosTheta, const Color &eta, const Color &k) {
	return Color(fresnelConductorChannel(cosTheta, eta.x, k.x),
	             fresnelConductorChannel(cosTheta, eta.y, k.y),
	             fresnelConductorChannel(cosTheta, eta.z, k.z));
}

// eval de la BRDF conductora: fr = D*G*F / (4*cosThetaO*cosThetaI), 0 fuera del hemisferio
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

	double cosThetaH = n.dot(wh);
	double D = beckmannD(cosThetaH, obj.alpha);
	if (D <= 0.0)
		return Color();
	double G = smithG1(wo, n, wh, obj.alpha) * smithG1(wi, n, wh, obj.alpha);
	Color F = fresnelConductor(wo.dot(wh), obj.eta, obj.k);

	return F * (D * G / (4.0 * cosO * cosI));
}

// eval de la BRDF difusa (lambertiana): fr = albedo/pi
inline Color evalDiffuse(const Sphere &obj, const Vector &n, const Vector &wi) {
	if (n.dot(wi) <= 0.0)
		return Color();
	return obj.c * (1.0 / PI);
}

inline Color materialEval(const Sphere &obj, const Vector &n, const Vector &wo, const Vector &wi) {
	return (obj.mat == MAT_CONDUCTOR) ? evalConductor(obj, n, wo, wi) : evalDiffuse(obj, n, wi);
}

// pdf del muestreo de BRDF: difuso = coseno hemisférico (cosThetaI/pi); conductor =
// pdf(wi) = D(wh)*cosThetaH / (4*|wo.wh|) (muestreo del half-vector de Beckmann)
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
	double cosThetaH = n.dot(wh);
	double D = beckmannD(cosThetaH, obj.alpha);
	if (D <= 0.0)
		return 0.0;
	double whDotWo = wh.dot(wo);
	if (whDotWo <= 0.0)
		return 0.0;
	return D * cosThetaH / (4.0 * fabs(whDotWo));
}

// muestreo de la BRDF (RF-3): difuso = coseno hemisférico; conductor = half-vector de
// Beckmann (thetaH = atan(sqrt(-alpha^2*ln(1-xi1)))) reflejando wo alrededor de wh.
inline Vector materialSample(const Sphere &obj, const Vector &n, const Vector &wo, unsigned short *Xi) {
	double xi1 = erand48(Xi);
	double xi2 = erand48(Xi);
	Vector s, tang;
	buildBasis(n, s, tang);

	if (obj.mat == MAT_DIFFUSE) {
		double phi = 2 * PI * xi2;
		double cosTheta = sqrt(1 - xi1);
		double sin2 = 1.0 - cosTheta * cosTheta;
		double sinTheta = sin2 > 0.0 ? sqrt(sin2) : 0.0;
		return s * (sinTheta * cos(phi)) + tang * (sinTheta * sin(phi)) + n * cosTheta;
	}

	double thetaH = atan(sqrt(-obj.alpha * obj.alpha * log(1.0 - xi1)));
	double phiH = 2 * PI * xi2;
	double cosThetaH = cos(thetaH);
	double sinThetaH = sin(thetaH);
	Vector wh = s * (sinThetaH * cos(phiH)) + tang * (sinThetaH * sin(phiH)) + n * cosThetaH;
	double whDotWo = wh.dot(wo);
	return wh * (2.0 * whDotWo) - wo;
}

// término determinista de las fuentes puntuales: se suma en cada muestra del estimador,
// para cualquier modo (una puntual r=0 nunca es alcanzada por un rayo aleatorio, así que
// necesita su propio término cerrado), evaluando la BRDF general del material del punto x.
inline Color pointLightTerm(const Point &x, const Vector &n, const Vector &wo, const Sphere &obj) {
	Color L;
	for (size_t i = 0; i < pointLights.size(); i++) {
		const PointLight &light = pointLights[i];

		Vector toLight = light.p - x;
		double r2 = toLight.dot(toLight);
		double dist = sqrt(r2);
		Vector wi = toLight * (1.0 / dist);

		double cosTheta = n.dot(wi);
		if (cosTheta <= 0.0)
			continue;
		if (!visible(x, wi, dist))
			continue;

		Color fr = materialEval(obj, n, wo, wi);
		L = L + clampNonNeg(fr.mult(light.I) * (cosTheta / r2));
	}
	return L;
}

// métodos de muestreo de importancia de fuentes esféricas / BRDF, seleccionables por CLI
enum EstimatorMode { MODE_ISAREA, MODE_ISSA, MODE_ISBRDF };
EstimatorMode estimatorMode = MODE_ISSA;

// muestreo de área. x' = c + r·ω con ω uniforme en la esfera; pdf_ω = d²/(4πr²·cosθ');
// contribución fr(wo,wi)·Le·cosθ/pdf_ω con rayo de sombra hacia x', evaluando la BRDF
// general del material (RF-2: microfacet en conductores, coseno hemisférico en difusos).
Color sampleAreaLight(const Sphere &light, const Sphere &obj, const Point &x, const Vector &n,
                       const Vector &wo, unsigned short *Xi) {
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
	Color fr = materialEval(obj, n, wo, wi);
	return clampNonNeg(fr.mult(light.ke) * (cosThetaSurface / pdf));
}

// muestreo del ángulo sólido (cono hacia la fuente). pdf_ω = 1/(2π(1−cosθmax));
// visible si el PRIMER hit del rayo (x, ω) es esta misma fuente. Misma contribución,
// evaluando la BRDF general del material.
Color sampleSolidAngle(const Sphere &light, int lightIdx, const Sphere &obj, const Point &x,
                        const Vector &n, const Vector &wo, unsigned short *Xi) {
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
	Color fr = materialEval(obj, n, wo, wi);
	return clampNonNeg(fr.mult(light.ke) * (cosThetaSurface / pdf));
}

// Modos "isarea"/"issa" (RF-2): por cada muestra, itera TODAS las fuentes de área con el
// método elegido y SUMA sus contribuciones, más el término determinista de cada puntual
// (sin selección aleatoria de fuente, sin dividir entre el número de luces).
Color shadeImportance(const Ray &r, unsigned short *Xi, EstimatorMode mode) {
	double t; int id = 0;
	if (!intersect(r, t, id))
		return Color();

	const Sphere &obj = spheres[id];
	if (!isBlack(obj.ke))
		return obj.ke;

	Point x = r.o + r.d * t;
	Vector n = (x - obj.p).normalize();
	Vector wo = r.d * -1.0;

	Color value;
	for (size_t k = 0; k < areaLightIdx.size(); k++) {
		int lightIdx = areaLightIdx[k];
		const Sphere &light = spheres[lightIdx];
		if (mode == MODE_ISAREA)
			value = value + sampleAreaLight(light, obj, x, n, wo, Xi);
		else
			value = value + sampleSolidAngle(light, lightIdx, obj, x, n, wo, Xi);
	}
	value = value + pointLightTerm(x, n, wo, obj);
	return value;
}

// Modo "isbrdf" (RF-3): muestrea wi según la distribución del material (coseno hemisférico
// en difusos, half-vector de Beckmann en conductores) y solo suma si el rayo alcanza un
// emisor: fr⊙Le·(n·ωi)/pdf. Más el término determinista de cada puntual.
Color shadeBrdf(const Ray &r, unsigned short *Xi) {
	double t; int id = 0;
	if (!intersect(r, t, id))
		return Color();

	const Sphere &obj = spheres[id];
	if (!isBlack(obj.ke))
		return obj.ke;

	Point x = r.o + r.d * t;
	Vector n = (x - obj.p).normalize();
	Vector wo = r.d * -1.0;

	Vector wi = materialSample(obj, n, wo, Xi);
	double cosI = n.dot(wi);
	Color mcTerm;
	if (cosI > 0.0) {
		double tShadow; int idShadow = 0;
		if (intersect(Ray(x, wi), tShadow, idShadow) && !isBlack(spheres[idShadow].ke)) {
			Color Le = spheres[idShadow].ke;
			double pdf = materialPdf(obj, n, wo, wi);
			if (pdf > 0.0) {
				Color fr = materialEval(obj, n, wo, wi);
				mcTerm = clampNonNeg(fr.mult(Le) * (cosI / pdf));
			}
		}
	}

	return mcTerm + pointLightTerm(x, n, wo, obj);
}

int main(int argc, char *argv[]) {
	int w = 1024, h = 768; // image resolution

	// CLI: ./rt <modo> <spp> <escena>
	// modo   ∈ {issa, isarea, isbrdf}
	// escena ∈ {point, areal, 2a1p}
	const char *modeName = argc > 1 ? argv[1] : "issa";
	int spp = argc > 2 ? atoi(argv[2]) : 32;
	const char *sceneName = argc > 3 ? argv[3] : "areal";

	if (strcmp(modeName, "isarea") == 0)
		estimatorMode = MODE_ISAREA;
	else if (strcmp(modeName, "isbrdf") == 0)
		estimatorMode = MODE_ISBRDF;
	else
		estimatorMode = MODE_ISSA;

	SceneType scene = SCENE_AREAL;
	if (strcmp(sceneName, "point") == 0)
		scene = SCENE_POINT;
	else if (strcmp(sceneName, "2a1p") == 0)
		scene = SCENE_2A1P;
	buildScene(scene);

	// fija la posicion de la camara y la dirección en que mira
	Ray camera( Point(0, 11.2, 214), Vector(0, -0.042612, -1).normalize() );

	// parametros de la camara
	Vector cx = Vector( w * 0.5095 / h, 0., 0.);
	Vector cy = (cx % camera.d).normalize() * 0.5095;

	// auxiliar para valor de pixel y matriz para almacenar la imagen
	Color *pixelColors = new Color[w * h];

	// usar openmp para paralelizar el ciclo: cada hilo computara un renglon (ciclo interior),
	// el estado del RNG se siembra por-pixel: misma imagen sin importar el numero de hilos
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

			// promediar N muestras del estimador Monte Carlo (cada muestra ya
			// suma todas las fuentes de la escena)
			Color accum;
			for (int sample = 0; sample < spp; sample++) {
				Color s = (estimatorMode == MODE_ISBRDF)
					? shadeBrdf(primaryRay, Xi)
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
