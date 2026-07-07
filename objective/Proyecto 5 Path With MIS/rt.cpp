// rt: un lanzador de rayos minimalista
// g++ -O3 -fopenmp rt.cpp -o rt
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <omp.h>
#include <time.h>
#define RANDOM() ((double)rand() / (RAND_MAX))
unsigned short xsubi[3] = {1, 2, 3};
//#define RANDOM() (erand48(xsubi))
#define MARGENDEERROR 1e-10

double indice = 1;
class Vector
{
public:
	double x, y, z; // coordenadas x,y,z

	// Constructor del vector, parametros por default en cero
	Vector(double x_ = 0, double y_ = 0, double z_ = 0)
	{
		x = x_;
		y = y_;
		z = z_;
	}

	// operador para suma y resta de vectores
	Vector operator+(const Vector &b) const { return Vector(x + b.x, y + b.y, z + b.z); }
	Vector operator-(const Vector &b) const { return Vector(x - b.x, y - b.y, z - b.z); }
	// operator multiplicacion vector y escalar
	Vector operator*(double b) const { return Vector(x * b, y * b, z * b); }

	// operator % para producto cruz
	Vector operator%(Vector &b) { return Vector(y * b.z - z * b.y, z * b.x - x * b.z, x * b.y - y * b.x); }
	Vector cruz(const Vector &b) const { return Vector((y * b.z) - (z * b.y), (z * b.x) - (x * b.z), (x * b.y) - (y * b.x)); }
	// producto punto con vector b
	double dot(const Vector &b) const { return x * b.x + y * b.y + z * b.z; }

	// producto elemento a elemento (Hadamard product)
	Vector mult(const Vector &b) const { return Vector(x * b.x, y * b.y, z * b.z); }
	// producto elemento a elemento (Hadamard product)
	Vector division(const Vector &b) const { return Vector(x / b.x, y / b.y, z / b.z); }
	// normalizar vector
	Vector &normalize() { return *this = *this * (1.0 / sqrt(x * x + y * y + z * z)); }
	Vector &root() { return *this = Vector(sqrt(x), sqrt(y), sqrt(z)); }
	double magnitude() const
	{
		return sqrt(x * x + y * y + z * z);
	}
	bool isNegative()
	{
		if (x < 0 || y < 0 || z < 0)
		{
			return true;
		}
		else
		{
			return false;
		}
	}
};
typedef Vector Point;
typedef Vector Color;
Vector MakeVector(double theta, double phi)
{

	double x_ = sin(theta) * cos(phi);
	double y_ = sin(theta) * sin(phi);
	double z_ = cos(theta);
	return Vector(x_, y_, z_);
}
void coordinateSystem(const Vector &n, Vector &s, Vector &t)
{
	if (std::abs(n.x) > std::abs(n.y))
	{
		double invLen = 1.0f / std::sqrt(n.x * n.x + n.z * n.z);
		t = Vector(n.z * invLen, 0.0f, -n.x * invLen);
	}
	else
	{
		double invLen = 1.0f / std::sqrt(n.y * n.y + n.z * n.z);
		t = Vector(0.0f, n.z * invLen, -n.y * invLen);
	}
	s = t.cruz(n);
}
Vector LocalToGlobal(const Vector &n, Vector &s, Vector &t, Vector v)
{
	Vector global = Vector();
	global.x = v.x * s.x + t.x * v.y + n.x * v.z;
	global.y = v.x * s.y + t.y * v.y + n.y * v.z;
	global.z = v.x * s.z + t.z * v.y + n.z * v.z;
	return global;
}

Vector GlobalToLocal(const Vector &n, Vector &s, Vector &t, Vector v)
{
	Vector global = Vector();
	global.x = s.x * v.x + s.y * v.y + s.z * v.z;
	global.y = t.x * v.x + t.y * v.y + t.z * v.z;
	global.z = n.x * v.x + n.y * v.y + n.z * v.z;
	return global;
}
class Ray
{
public:
	Point o;
	Vector d; // origen y direcccion del rayo
	Ray(Point o_, Vector d_) : o(o_), d(d_) {}
	Ray() : o(Point()), d(Vector()) {} // constructor
};

class Sphere
{
public:
	double r; // radio de la esfera
	Point p;  // posicion
	Color c;  // color
	Color ke;
	Color eta;
	Color kappa;
	double alfa;
	Sphere(double r_, Point p_, Color c_, Color ke_, Color eta_, Color kappa_, double alfa_) : r(r_), p(p_), c(c_), ke(ke_), eta(eta_), kappa(kappa_), alfa(alfa_) {}

	// PROYECTO 1
	// determina si el rayo intersecta a esta esfera
	double intersect(const Ray &ray) const
	{
		// regresar distancia si hay intersección
		Vector op = ray.o - p;
		double opd = op.dot(ray.d);
		double discriminant = opd * opd - op.dot(op) + r * r;
		double t0, t1;
		if (discriminant > 0)
		{
			t0 = (opd * -1) - sqrt(discriminant);
			t1 = (opd * -1) + sqrt(discriminant);
			if (t0 > MARGENDEERROR)
			{
				return t0;
			}
			else if (t1 > MARGENDEERROR)
			{
				return t1;
			}
		}
		else if (discriminant == 0)
		{
			return (opd * -1);
		}
		else
		{
			return 0.0;
		}
		// regresar 0.0 si no hay interseccion
		return 0.0;
	}
}; /*
Sphere spheres[] = {
	//Escena: radio, posicion, color
	Sphere(1e5, Point(-1e5 - 49, 0, 0), Color(.75, .25, .25), Color(), Color(), Color(), 0),													 // pared izq
	Sphere(1e5, Point(1e5 + 49, 0, 0), Color(.25, .25, .75), Color(), Color(), Color(), 0),														 // pared der
	Sphere(1e5, Point(0, 0, -1e5 - 81.6), Color(.25, .75, .25), Color(), Color(), Color(), 0),													 // pared detras
	Sphere(1e5, Point(0, -1e5 - 40.8, 0), Color(.25, .75, .75), Color(), Color(), Color(), 0),													 // suelo
	Sphere(1e5, Point(0, 1e5 + 40.8, 0), Color(.75, .75, .25), Color(), Color(), Color(), 0),													 // techo
	Sphere(16.5, Point(-23, -24.3, -34.6), Color(.2, .3, .4), Color(), Color(1.66058, 0.88143, 0.521467), Color(9.2282, 6.27077, 4.83803), 0.3), // esfera abajo-izq
	Sphere(16.5, Point(23, -24.3, -3.6), Color(.4, .3, .2), Color(), Color(0.143245, 0.377423, 1.43919), Color(3.98479, 2.3847, 1.60434), 0.3),	 // esfera abajo-der
	Sphere(0.0, Point(0, 24.3, 0), Color(0, 0, 0), Color(2000, 2000, 2000), Color(), Color(), 0),
	//Sphere(10.5, Point(-23, 24.3, 0), Color(1, 1, 1), Color(12, 5, 5), Color(), Color(), 0),
	//Sphere(5, Point(+23, 24.3, -50), Color(1, 1, 1), Color(5, 5, 12), Color(), Color(), 0)

};

int NoSpheres = 10;*/
Sphere spheres[] = {
	//Escena: radio, posicion, color
	Sphere(1e5, Point(-1e5 - 49, 0, 0), Color(.75, .25, .25), Color(), Color(), Color(), 0),													 // pared izq
	Sphere(1e5, Point(1e5 + 49, 0, 0), Color(.25, .25, .75), Color(), Color(), Color(), 0),														 // pared der
	Sphere(1e5, Point(0, 0, -1e5 - 81.6), Color(.25, .75, .25), Color(), Color(), Color(), 0),													 // pared detras
	Sphere(1e5, Point(0, -1e5 - 40.8, 0), Color(.25, .75, .75), Color(), Color(), Color(), 0),													 // suelo
	Sphere(1e5, Point(0, 1e5 + 40.8, 0), Color(.75, .75, .25), Color(), Color(), Color(), 0),													 // techo
	Sphere(16.5, Point(-23, -24.3, -34.6), Color(.2, .3, .4), Color(), Color(1.66058, 0.88143, 0.521467), Color(9.2282, 6.27077, 4.83803), 0.3), // esfera abajo-izq
	Sphere(16.5, Point(23, -24.3, -3.6), Color(.4, .3, .2), Color(), Color(0.143245, 0.377423, 1.43919), Color(3.98479, 2.3847, 1.60434), 0.3),	 // esfera abajo-der
	Sphere(10.5, Point(0, 24.3, 0), Color(1, 1, 1), Color(10, 10, 10), Color(), Color(), 0),
	//Sphere(5, Point(+23, 24.3, -50), Color(1, 1, 1), Color(5, 5, 12), Color(), Color(), 0)
};
int NoSpheres = 8;
// limita el valor de x a [0,1]
inline double clamp(const double x)
{
	if (x < 0.0)
		return 0.0;
	else if (x > 1.0)
		return 1.0;
	return x;
}

// convierte un valor de color en [0,1] a un entero en [0,255]
inline int toDisplayValue(const double x)
{
	return int(pow(clamp(x), 1.0 / 2.2) * 255 + .5);
}

// PROYECTO 1
// calcular la intersección del rayo r con todas las esferas
// regresar true si hubo una intersección, falso de otro modo
// almacenar en t la distancia sobre el rayo en que sucede la interseccion
// almacenar en id el indice de spheres[] de la esfera cuya interseccion es mas cercana
inline bool intersect(const Ray &r, double &t, int &id)
{
	double tNEAR = INFINITY;
	double tARR[NoSpheres];
	bool centinela = false;
	for (int i = 0; i < NoSpheres; i++)
	{

		tARR[i] = spheres[i].intersect(r);
		if (tARR[i] > 0)
		{
			centinela = true;
			if (tARR[i] < tNEAR)
			{
				tNEAR = tARR[i];
				id = i;
				t = tNEAR;
			}
		}
	}
	return centinela;
}
Vector MEsfera(void)
{
	double xiphi = RANDOM();
	double xiteta = RANDOM();
	double theta = acos(1 - 2 * xiteta);
	double phi = 2 * M_PI * xiphi;
	return MakeVector(theta, phi);
}
//Para Fuente de Area
Color ComputeLe(Ray r, Point x, int id2, double &distancia, int sphere)
{
	distancia = (x - r.o).magnitude();
	double t = 0;
	int id = 0;
	//printf("Magnitud:%f\n", magnitud);
	if (intersect(r, t, id))
	{
		if (fabs(distancia - t) <= MARGENDEERROR)
		{
			return spheres[sphere].ke;
		}
		return Color();
	}
	return Color();
}
Color ComputeLe(Ray r, Point x, int id2, int sphere)
{
	double distancia = (r.o - x).magnitude();
	double t = 0;
	int id = 0;
	//printf("Magnitud:%f\n", magnitud);
	if (intersect(r, t, id))
	{
		//printf("d1:%f, t:%f  , id:%d,id2:%d\n", distancia, t, id, id2);
		if (fabs(distancia - t) <= MARGENDEERROR)
		{
			return spheres[sphere].ke * (1 / (distancia * distancia));
		}
		return Color();
	}
	return Color();
}
Vector MCono(double maxtheta)
{
	double xiphi = RANDOM();
	double xiteta = RANDOM(); //RANDOM();
	double theta = acos((1 - xiteta) + (xiteta * cos(maxtheta)));
	double phi = 2 * M_PI * xiphi;
	//printf("%f,%f\n",theta,phi);
	return MakeVector(theta, phi);
}
double PAnguloSolido(double maxtheta)
{
	double probabilidad = 1.0 / (2 * M_PI * (1 - cos(maxtheta)));
	return probabilidad;
}
double PFuenteDeArea(double costheta, double distancia, int sphere)
{
	double area = (4 * M_PI * spheres[sphere].r * spheres[sphere].r);
	double probabilidad = (distancia * distancia) / (area * costheta);
	return probabilidad;
}
// Visibilidad para Angulo Solido
Color Visibility(Ray ray, int sphere)
{
	double t;
	int id;
	if (intersect(ray, t, id))
	{
		//printf("d1:%f, d2:%f, t:%f id:%d \n", distancia, distancia2, t, id);
		if (id == sphere)
		{
			return spheres[sphere].ke;
		}
		return Color();
	}
	return Color();
} /***BRDF*/
void UnmakeVector(double &r, double &theta, double &phi, Vector wi)
{
	r = wi.magnitude();
	theta = acos(wi.z / r);
	phi = atan2(wi.y, wi.x); //Utilizar atan2() por que con atan no es periodico. //Verificar la Radiancia, las formulas., verificar que Wi y Wo esten en el superior. Si estan en hemisferios distintos regresar 0.
							 //printf("Vector Wh: %f,%f,%f,r:%f,theta:%f,phi:%f\n", wi.x, wi.y, wi.z, r, theta, phi);
}
double xPositiva(double val)
{
	double xPositiv;
	if (val > 0)
	{
		xPositiv = 1;
	}
	else
	{
		xPositiv = 0;
	}
	return xPositiv;
}
double n2(double val)
{
	return val * val;
}

double n4(double val)
{
	return val * val * val * val;
}
double LocalCostheta(Vector Wh)
{
	Color normal = Vector(0, 0, 1);
	return normal.dot(Wh);
}
double LocalSintheta(Vector Wh)
{
	return sqrt(1 - LocalCostheta(Wh) * LocalCostheta(Wh));
}
double LocalTangente(Vector Wh)
{
	return LocalSintheta(Wh) / LocalCostheta(Wh);
}
double G1(Vector V, Vector Wh, double alfa)
{
	Vector n = Vector(0, 0, 1);
	double xPositiv = xPositiva((V.dot(Wh) / V.dot(n)));
	double a = 1.0 / (alfa * LocalTangente(V));
	if (a < 1.6)
	{
		return ((3.535 * a + 2.181 * n2(a)) / (1 + 2.276 * a + 2.577 * n2(a))) * xPositiv;
	}
	else
	{
		return 1 * xPositiv;
	}
}
double G(Vector Wi, Vector Wo, Vector Wh, Vector n, double alfa)
{

	return G1(Wi, Wh, alfa) * G1(Wo, Wh, alfa);
}
double D(Vector Wh, int id)
{

	double r, theta, phi;
	UnmakeVector(r, theta, phi, Wh);
	//printf("Vector Wh: %f,%f,%f,theta:%f,R:%f\n", Wh.x, Wh.y, Wh.z, theta, r);
	return (xPositiva(Wh.z) / (M_PI * n2(spheres[id].alfa) * n4(LocalCostheta(Wh)))) * exp(-1 * (n2(LocalTangente(Wh))) / n2(spheres[id].alfa));
}
//Hacer una funcion que reciba el vector y regrese seno, otra, coseno, y otra tan, que sea el vector local.

double UpDown(double eta, double kappa, double theta)
{
	double etav = eta / indice;
	double kappav = kappa / indice;
	double A2B2 = sqrt(n2(n2(etav) - n2(kappav) - n2(sin(theta))) + 4 * n2(etav) * n2(kappav));
	double A = sqrt(0.5 * (A2B2 + n2(etav) - n2(kappav) - n2(sin(theta))));
	double r = (A2B2 + n2(cos(theta)) - 2 * A * cos(theta)) / (A2B2 + n2(cos(theta)) + 2 * A * cos(theta));
	return r;
}
double Parallel(double rUpDown, double eta, double kappa, double theta)
{
	double etav = eta / indice;
	double kappav = kappa / indice;
	double A2B2 = sqrt(n2(n2(etav) - n2(kappav) - n2(sin(theta))) + 4 * n2(etav) * n2(kappav));
	double A = sqrt(0.5 * (A2B2 + n2(etav) - n2(kappav) - n2(sin(theta))));
	double rP = rUpDown * ((A2B2 * n2(cos(theta)) + n4(sin(theta)) - 2 * A * cos(theta) * n2(sin(theta))) / (A2B2 * n2(cos(theta)) + n4(sin(theta)) + 2 * A * cos(theta) * n2(sin(theta))));
	return rP;
}
Vector Fresnel2(Vector etan, Vector kappan, double theta)
{

	Vector eta = etan * (1 / indice);
	Vector kappa = kappan * (1 / indice);
	Vector sintheta = Vector(sin(theta), sin(theta), sin(theta));
	Vector A2B2 = ((eta.mult(eta) - kappa.mult(kappa) - sintheta).mult(eta.mult(eta) - kappa.mult(kappa) - sintheta) + eta.mult(eta).mult(kappa).mult(kappa) * 4).root();
	Vector A = ((A2B2 + (eta.mult(eta)) - kappa.mult(kappa) - sintheta) * 0.5).root();
	Vector costheta = Vector(cos(theta), cos(theta), cos(theta));
	Vector numeradorUpDown = A2B2 + costheta.mult(costheta) - (costheta.mult(A)) * 2;
	Vector denominadorUpDown = A2B2 + costheta.mult(costheta) + (costheta.mult(A)) * 2;
	Vector rUpDown = numeradorUpDown.division(denominadorUpDown);
	Vector numeradoP = A2B2.mult(costheta).mult(costheta) + sintheta.mult(sintheta).mult(sintheta).mult(sintheta) - (costheta.mult(A)).mult(sintheta).mult(sintheta) * 2;
	Vector denominadorP = A2B2.mult(costheta).mult(costheta) + sintheta.mult(sintheta).mult(sintheta).mult(sintheta) + (costheta.mult(A)).mult(sintheta).mult(sintheta) * 2;
	Vector rParallel = rUpDown.mult(numeradoP.division(denominadorP));
	return (rUpDown + rParallel) * 0.5;
}
Vector Fresnel(Vector etan, Vector kappan, double theta)
{
	double rUx = UpDown(etan.x, kappan.x, theta);
	double rUy = UpDown(etan.y, kappan.y, theta);
	double rUz = UpDown(etan.z, kappan.z, theta);
	Vector rUpDown = Vector(rUx, rUy, rUz);
	double rPx = Parallel(rUx, etan.x, kappan.x, theta);
	double rPy = Parallel(rUy, etan.y, kappan.y, theta);
	double rPz = Parallel(rUz, etan.z, kappan.z, theta);
	Vector rParallel = Vector(rPx, rPy, rPz);
	return (rUpDown + rParallel) * 0.5;
}
Color BRDF(Vector Wo, Vector Wh, Vector Wi, Vector n, int id)
{
	if (spheres[id].eta.x != 0)
	{
		Vector normal = Vector(0, 0, 1);
		if (normal.dot(Wo) < 0 || normal.dot(Wi) < 0)
		{
			//printf("Abajo Hemisferio\n");
			return Color();
		}

		Vector F = Fresnel2(spheres[id].eta, spheres[id].kappa, acos(Wi.dot(Wh)));
		double Gval = G(Wi, Wo, Wh, normal, spheres[id].alfa);
		double Dval = D(Wh, id);
		double denominador = (4 * fabs(normal.dot(Wi)) * fabs(normal.dot(Wo)));
		if (!isfinite(denominador * Dval * Gval))
		{
			printf("Ya valio \n");
		}
		Color color = F * (Gval * Dval) * (1 / denominador);
		if (!isfinite(color.x) || !isfinite(color.y) || !isfinite(color.z))
		{
			printf("Ya valio \n");
		}
		if (color.x < 0 || color.y < 0 || color.z < 0)
		{
			printf("Ya valio Color Negativo\n");
		}
		return color;
	}
	else
	{
		return spheres[id].c * (1 / M_PI);
	}
}

/**Cosine Hemisphere BRDF*/
double PCosHemisferico(double costheta)
{
	//Probabilidad Coseno Hemisferico
	return (1.00 / M_PI) * costheta;
}
Vector MCosHemisferico(void)
{
	double xiphi = RANDOM();  //RANDOM();
	double xiteta = RANDOM(); //RANDOM();
	double theta = acos(sqrt(1 - xiteta));
	double phi = 2 * M_PI * xiphi;
	return MakeVector(theta, phi);
}
Vector MWh(double alfa)
{
	//NOTA : Checar diapositiva 47
	double xiphi = RANDOM();   //RANDOM();
	double xitheta = RANDOM(); //RANDOM();
	double theta = atan(sqrt(alfa * alfa * -1 * log(1 - xitheta)));
	double phi = 2 * M_PI * xiphi;
	return MakeVector(theta, phi);
}
Vector MakeWr(Vector n, Vector Wo)
{
	return Wo * -1 + n * 2 * (n.dot(Wo));
}
double PBRDF(Vector Wh, Vector Wo, int id)
{
	Vector normal = Vector(0, 0, 1);
	return (D(Wh, id) * Wh.dot(normal)) / (4 * fabs(Wo.dot(Wh)));
}

Color ComputeLe(Ray r)
{
	double t = 0;
	int id = 0;
	if (intersect(r, t, id))
	{
		return spheres[id].ke;
	}
	return Color();
}
inline double PowerHeuristic(double fPdf, double gPdf)
{
	double f2 = fPdf * fPdf, g2 = gPdf * gPdf;
	return f2 / (f2 + g2);
}
Vector CosHemisphere(Vector n, Point x, int id, Ray &WiG, Vector Wo, double &Probabilidad)
{
	//Para Coseno Hemisferico
	Vector Wi, Wh, fr, s = Vector(), t = Vector();
	Color temp;
	coordinateSystem(n, s, t);
	if (spheres[id].eta.x != 0)
	{
		Vector normal = Vector(0, 0, 1);
		Wh = MWh(spheres[id].alfa);
		Wi = MakeWr(Wh, Wo);
		//Wi = MCosHemisferico();
		//Wh = (Wi + Wo).normalize();
		fr = BRDF(Wo, Wh, Wi, normal, id);
		Vector WiGlobal = LocalToGlobal(n, s, t, Wi);
		double costhetaWi = n.dot(WiGlobal);
		if (costhetaWi < 0)
		{
			return Color();
		}
		/*************Para Wo**************/
		Vector WoGlobal = LocalToGlobal(n, s, t, Wo);
		double costhetaWo = n.dot(WoGlobal);
		if (costhetaWo < 0)
		{
			return Color();
		}
		WiG = Ray(x, WiGlobal);
		Color Le = ComputeLe(WiG);
		Probabilidad = PBRDF(Wh, Wo, id);
		//double invP = (1 / PCosHemisferico(costhetaWi));
		//printf("%f\n", (1 / PBRDF(Wh, Wo, id)));
		temp = (fr.mult(Le) * costhetaWi) * (1 / PBRDF(Wh, Wo, id));

		return temp;
	}
	else
	{

		fr = spheres[id].c * (1 / M_PI);
		Vector WiLocal = MCosHemisferico();
		Vector WiGlobal = LocalToGlobal(n, s, t, WiLocal);
		double costhetaWi = n.dot(WiGlobal);
		if (costhetaWi < 0)
		{
			return Color();
		}
		/*************Para Wo**************/
		Vector WoGlobal = LocalToGlobal(n, s, t, Wo);
		double costhetaWo = n.dot(WoGlobal);
		if (costhetaWo < 0)
		{
			return Color();
		}
		Probabilidad = PCosHemisferico(costhetaWi);
		WiG = Ray(x, WiGlobal);
		Color Le = ComputeLe(WiG);
		temp = (fr.mult(Le) * costhetaWi) * (1 / PCosHemisferico(costhetaWi));
		return temp;
	}
}
double PAnguloSolido(int FuenteLuminosa, Vector W)
{
	//printf("Vector W:%f,%f,%f,\nr:%f,magnitude():%f,asin:%f\n", W.x, W.y, W.z, spheres[FuenteLuminosa].r, W.magnitude(), asin(spheres[FuenteLuminosa].r / W.magnitude()));
	double maxtheta = asin(spheres[FuenteLuminosa].r / W.magnitude());
	return PAnguloSolido(maxtheta);
}

/**********************Metodos de Renderizado*******************************/
Color AnguloSolido(Vector n, Point x, int FuenteLuminosa, int id, Vector &WiG, Vector Wo, double &Probabilidad)
{
	Vector s;
	Vector t;
	Vector W = spheres[FuenteLuminosa].p - x; //De donde intersectamos hacia la fuente luminosa
	double maxtheta = asin(spheres[FuenteLuminosa].r / W.magnitude());
	Vector WMuestra = MCono(maxtheta);
	W.normalize();
	coordinateSystem(W, s, t);
	Vector WMuestreada = LocalToGlobal(W, s, t, WMuestra);
	double costheta = n.dot(WMuestreada);

	Ray r = Ray(x, WMuestreada);
	Color Le = Visibility(r, FuenteLuminosa);
	coordinateSystem(n, s, t);
	Vector Wi = GlobalToLocal(n, s, t, WMuestreada);
	WiG = WMuestra;
	double costhetaWo = n.dot(LocalToGlobal(n, s, t, Wo));
	if (costheta < 0)
	{
		return Color();
	}
	if (costhetaWo < 0)
	{
		return Color();
	}
	Vector Wh = (Wo + Wi) * (1 / (Wo + Wi).magnitude());
	Color fr = BRDF(Wo, Wh, Wi, n, id);
	Probabilidad = PAnguloSolido(maxtheta);
	Color newC = (fr.mult(Le) * costheta) * (1 / PAnguloSolido(maxtheta)); //) * (1 / PAnguloSolido(costheta, distancia));(4 * M_PI)
	if (newC.isNegative())
	{
		newC = Color();
	}
	return newC;
}

Color FuentePuntual(Vector n, Point x, int FuenteLuminosa, int id, Vector Wo)
{
	Vector W = spheres[FuenteLuminosa].p - x;
	W.normalize();
	Vector Winv = W * -1;
	double costheta = n.dot(W);
	Color Le = ComputeLe(Ray(spheres[FuenteLuminosa].p, Winv), x, id, FuenteLuminosa);
	Vector s, t;
	//printf("Le:%f,%f,%f\n",Le.x,Le.y,Le.z);
	coordinateSystem(n, s, t);
	Vector Wi = GlobalToLocal(n, s, t, W.normalize());
	Vector Wh = (Wo + Wi) * (1 / (Wo + Wi).magnitude());
	Color fr = BRDF(Wo, Wh, Wi, n, id);

	return (fr.mult(Le) * costheta);
}

Color EstimadorMonteCarlo(Ray ray, Vector n, Point x, int id)
{
	Color temp = Color();
	Vector s;
	Vector t;
	coordinateSystem(n, s, t);
	Vector WoL = GlobalToLocal(n, s, t, ray.d * -1);

	for (int FuenteLuminosa = 0; NoSpheres > FuenteLuminosa; FuenteLuminosa++)
	{
		if ((spheres[FuenteLuminosa].ke.x > 0 || spheres[FuenteLuminosa].ke.y > 0 || spheres[FuenteLuminosa].ke.z > 0))
		{
			if (spheres[FuenteLuminosa].r > 0)
			{
				Vector WiL, WhF;
				Ray WiF;
				double Pff, Pll, Plf, Pfl;
				Color Luz = AnguloSolido(n, x, FuenteLuminosa, id, WiL, WoL, Pll);
				if (spheres[id].eta.x != 0)
				{
					Plf = PBRDF((WiL + WoL).normalize(), WoL, id);
					if (0 > (Plf))
					{
						printf("Vector BRDF:%f,\n", Plf);
					}
				}
				else
				{
					Plf = PCosHemisferico(n.dot(LocalToGlobal(n, s, t, WiL)));
					if (0 > (Plf))
					{
						printf("Vector Cos:%f,\n", Plf);
					}
				}
				Color fr = CosHemisphere(n, x, id, WiF, WoL, Pff);
				if (Visibility(WiF, FuenteLuminosa).x != 0) //Check Visibility
				{
					fr = fr;

					Pfl = PAnguloSolido(FuenteLuminosa, spheres[FuenteLuminosa].p - x);
				}
				else
				{
					fr = Color();
					Pfl = 0;
				}

				//Debe ser la probabilidad de Wo Respecto a angulo solido

				//printf("Vector WiF.d:%f,%f,%f\n", WiF.d.x, WiF.d.y, WiF.d.z);
				//printf("Pll:%f,Plf:%f,Pff:%f,Pfl:%f\n", Pll, Plf, Pff, Pfl);
				double Wl = PowerHeuristic(Pll, Plf);
				double Wf = PowerHeuristic(Pff, Pfl);
				fr = fr * Wf;
				Luz = Luz * Wl;
				//printf("%lf+%lf=%lf\n",Wf,Wl,Wf+Wl);
				temp = temp + Luz + fr;
			}
			else
			{
				temp = temp + FuentePuntual(n, x, FuenteLuminosa, id, WoL);
			}
		}
	}
	return temp;
}
Vector muestreoBRDF(int id, Vector WoL, Vector n, double &pr, Vector &fr)
{
	Vector s, t;
	coordinateSystem(n, s, t);
	if (spheres[id].eta.x != 0)
	{

		Vector Wh = MWh(spheres[id].alfa);
		Vector Wi = MakeWr(Wh, WoL);
		Vector WiGlobal = LocalToGlobal(n, s, t, Wi);
		pr = PBRDF(Wh, WoL, id);
		Vector normal = Vector(0, 0, 1);
		fr = BRDF(WoL, Wh, Wi, normal, id);
		return WiGlobal;
	}
	else
	{
		Vector WiLocal = MCosHemisferico();
		Vector WiGlobal = LocalToGlobal(n, s, t, WiLocal);
		pr = PCosHemisferico(WiGlobal.dot(n));
		fr = spheres[id].c * (1 / M_PI);
		return WiGlobal;
	}
}

// Calcula el valor de color para el rayo dado
Color shade(const Ray &r, int bounce)
{
	double t;
	int id = 0;
	// si ray escapa
	if (!intersect(r, t, id))
		return Color(); // el rayo no intersecto objeto, return Vector() == negro

	const Sphere &obj = spheres[id];

	// PROYECTO 1
	// determinar coordenadas del punto de interseccion
	Point x = r.o + r.d * t;

	// determinar la dirección normal en el punto de interseccion
	Vector n = x - obj.p;
	n.normalize();
	if (obj.ke.x > 1e-8 && obj.ke.y > 1e-8 && obj.ke.z > 1e-8)
	{
		if (bounce == 1)
			return obj.ke;
		else
			return Color();
	}
	else
	{ // determinar el color que se regresara
		Vector s, t, fr;
		coordinateSystem(n, s, t);
		Vector WoL = GlobalToLocal(n, s, t, r.d * -1);
		double pr;
		Vector Wr = muestreoBRDF(id, WoL, n, pr, fr);
		Ray nuevoRayo(x, Wr);
		Color Le = EstimadorMonteCarlo(r, n, x, id);
		if(bounce==10)
		return Le;
		Color Lind = (fr * (n.dot(Wr) / pr)).mult(shade(nuevoRayo, bounce + 1));
		if (!isfinite(Lind.x) || !isfinite(Lind.y) || !isfinite(Lind.z))
		{
			printf("Ya valio \n");
		}
		return Le + Lind;
	}
}
/**int interface()
{
	printf("Ingrese el método a utilizar en caso de que no se use una Fuente Luminosa Puntual\n");
	printf("3) Muestreo	BRDF\n");
	printf("2) Muestreo	Fuente de Area\n");
	printf("1) Muestreo Angulo Solido\n");
	int opcion;
	scanf("%d", &opcion);
	return opcion;
}*/
int main(int argc, char *argv[])
{

	srand(time(NULL));
	int w = 1024, h = 768; // image resolution
	double muestras = atof(argv[1]);
	printf("%f\n", muestras);
	int opcion = 1; //interface();
	// fija la posicion de la camara y la dirección en que mira
	Ray camera(Point(0, 11.2, 214), Vector(0, -0.042612, -1).normalize());

	// parametros de la camara
	Vector cx = Vector(w * 0.5095 / h, 0., 0.);
	Vector cy = (cx % camera.d).normalize() * 0.5095;

	// auxiliar para valor de pixel y matriz para almacenar la imagen
	Color *pixelColors = new Color[w * h];

	// PROYECTO 1
	// usar openmp para paralelizar el ciclo: cada hilo computara un renglon (ciclo interior),
#pragma omp parallel for schedule(dynamic, 1)
	for (int y = 0; y < h; y++)
	{
		// recorre todos los pixeles de la imagen
		fprintf(stderr, "\r%5.2f%%", 100. * y / (h - 1));
		for (int x = 0; x < w; x++)
		{
			int idx = (h - y - 1) * w + x; // index en 1D para una imagen 2D x,y son invertidos
			Color pixelValue = Color();	   // pixelValue en negro por ahora
			for (int i = 0; i < muestras; i++)
			{
				double dx = RANDOM() - 0.5, dy = RANDOM() - 0.5;

				// para el pixel actual, computar la dirección que un rayo debe tener
				Vector cameraRayDir = cx * (double(x + dx) / w - .5) + cy * (double(y + dy) / h - .5) + camera.d;

				// computar el color del pixel para el punto que intersectó el rayo desde la camara
				pixelValue = pixelValue + shade(Ray(camera.o, cameraRayDir.normalize()), opcion);
			}
			pixelValue = pixelValue * (1.0 / muestras);
			// limitar los tres valores de color del pixel a [0,1]
			pixelColors[idx] = Color(clamp(pixelValue.x), clamp(pixelValue.y), clamp(pixelValue.z));
		}
	}

	fprintf(stderr, "\n");

	// PROYECTO 1
	// Investigar formato ppm
	FILE *f = fopen("image.ppm", "w");
	// escribe cabecera del archivo ppm, ancho, alto y valor maximo de color
	fprintf(f, "P3\n%d %d\n%d\n", w, h, 255);
	for (int p = 0; p < w * h; p++)
	{ // escribe todos los valores de los pixeles
		fprintf(f, "%d %d %d ", toDisplayValue(pixelColors[p].x), toDisplayValue(pixelColors[p].y),
				toDisplayValue(pixelColors[p].z));
	}
	fclose(f);

	delete[] pixelColors;

	return 0;
}
