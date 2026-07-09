// rt: un lanzador de rayos minimalista
 // g++ -O3 -fopenmp rt.cpp -o rt
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <omp.h>
#include <cstring>
#include <vector>

class Vector
{
public:
	double x, y, z;

	Vector(double x_= 0, double y_= 0, double z_= 0){ x=x_; y=y_; z=z_; }

	Vector operator+(const Vector &b) const { return Vector(x + b.x, y + b.y, z + b.z); }
	Vector operator-(const Vector &b) const { return Vector(x - b.x, y - b.y, z - b.z); }
	Vector operator*(double b) const { return Vector(x * b, y * b, z * b); }

	Vector operator%(const Vector&b) const {return Vector(y * b.z - z * b.y, z * b.x - x * b.z, x * b.y - y * b.x);}

	double dot(const Vector &b) const { return x * b.x + y * b.y + z * b.z; }

	Vector mult(const Vector &b) const { return Vector(x * b.x, y * b.y, z * b.z); }

	Vector& normalize(){ return *this = *this * (1.0 / sqrt(x * x + y * y + z * z)); }
};
typedef Vector Point;
typedef Vector Color;

class Ray
{
public:
	Point o;
	Vector d;
	Ray(Point o_, Vector d_) : o(o_), d(d_) {}
};

class Sphere
{
public:
	double r;
	Point p;
	Color c;
	Color ke;

	Sphere(double r_, Point p_, Color c_, Color ke_ = Color()): r(r_), p(p_), c(c_), ke(ke_) {}

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

struct Light {
	double r;
	Point p;
	Color c;
	Color le;
	bool isPoint;
};

Sphere spheres[] = {
	Sphere(1e5,  Point(-1e5 - 49, 0, 0),   Color(.75, .25, .25), Color(0, 0, 0)),
	Sphere(1e5,  Point(1e5 + 49, 0, 0),    Color(.25, .25, .75), Color(0, 0, 0)),
	Sphere(1e5,  Point(0, 0, -1e5 - 81.6), Color(.25, .75, .25), Color(0, 0, 0)),
	Sphere(1e5,  Point(0, -1e5 - 40.8, 0), Color(.25, .75, .75), Color(0, 0, 0)),
	Sphere(1e5,  Point(0, 1e5 + 40.8, 0),  Color(.75, .75, .25), Color(0, 0, 0)),
	Sphere(16.5, Point(-23, -24.3, -34.6), Color(.2, .3, .4), Color(0, 0, 0)),
	Sphere(16.5, Point(23, -24.3, -3.6),   Color(.4, .3, .2), Color(0, 0, 0)),
	Sphere(10.5, Point(0, 24.3, 0),        Color(1, 1, 1), Color(10, 10, 10))
};

std::vector<Light> lights;

const char *samplingMode = "solidangle";
const char *sceneMode = "1L";

inline double clamp(const double x) {
	if(x < 0.0)
		return 0.0;
	else if(x > 1.0)
		return 1.0;
	return x;
}

inline double rng(unsigned short *seed) {
	return erand48(seed);
}

void localBasis(const Vector &n, Vector &s, Vector &t) {
	if (fabs(n.x) > fabs(n.y)) {
		t = Vector(n.z, 0.0, -n.x);
		double len = sqrt(t.x * t.x + t.z * t.z);
		t = t * (1.0 / len);
	} else {
		t = Vector(0.0, n.z, -n.y);
		double len = sqrt(t.y * t.y + t.z * t.z);
		t = t * (1.0 / len);
	}
	s = t % n;
}

struct Sample {
	Vector d;
	double pdf;
};

Sample sampleDirection(const Vector &n, unsigned short *seed) {
	double xi1 = rng(seed);
	double xi2 = rng(seed);

	Vector s, t;
	localBasis(n, s, t);

	double costheta = sqrt(fmax(0.0, 1.0 - xi1));
	double sintheta = sqrt(fmax(0.0, 1.0 - costheta * costheta));
	double phi = 2.0 * M_PI * xi2;
	Vector local(sintheta * cos(phi), sintheta * sin(phi), costheta);
	Vector d = s * local.x + t * local.y + n * local.z;
	return {d.normalize(), costheta / M_PI};
}

struct LightSample {
	Vector wi;
	double pdf;
	Color le;
	bool valid;
};

LightSample sampleAreaLight(const Light &light, const Point &x, const Vector &n,
                             unsigned short *seed) {
	double xi1 = rng(seed);
	double xi2 = rng(seed);

	double costheta_sample = 1.0 - 2.0 * xi1;
	double sintheta_sample = sqrt(fmax(0.0, 1.0 - costheta_sample * costheta_sample));
	double phi = 2.0 * M_PI * xi2;

	Vector sampleDir(sintheta_sample * cos(phi), costheta_sample, sintheta_sample * sin(phi));
	sampleDir.normalize();

	Point xp = light.p + sampleDir * light.r;
	Vector toXp = xp - x;
	double d = sqrt(toXp.dot(toXp));

	if (d < 1e-6) {
		return {Vector(), 0, Color(), false};
	}

	Vector wi = toXp * (1.0 / d);
	Vector n_light = sampleDir;
	double costheta_prime = fmax(0.0, -n_light.dot(wi));

	if (costheta_prime <= 0.0) {
		return {Vector(), 0, Color(), false};
	}

	double pdf = (d * d) / (4.0 * M_PI * light.r * light.r * costheta_prime);

	double costheta_x = fmax(0.0, n.dot(wi));
	if (costheta_x <= 0.0) {
		return {Vector(), 0, Color(), false};
	}

	Ray shadowRay(x + n * 1e-4, wi);
	double t_first = 1e20;
	int id_first = -1;
	for (int i = 0; i < 8; i++) {
		double dist = spheres[i].intersect(shadowRay);
		if (dist > 0.0 && dist < t_first) {
			t_first = dist;
			id_first = i;
		}
	}

	if (id_first < 0) {
		return {Vector(), 0, Color(), false};
	}

	Point hitPoint = shadowRay.o + shadowRay.d * t_first;
	Vector toCenter = light.p - hitPoint;
	double toCenterDist = sqrt(toCenter.dot(toCenter));

	Color contrib = light.le * (costheta_x / pdf) * (1.0 / M_PI);
	return {wi, pdf, contrib, true};
}

LightSample sampleSolidAngleLight(const Light &light, const Point &x, const Vector &n,
                                   unsigned short *seed) {
	double xi1 = rng(seed);
	double xi2 = rng(seed);

	Vector W = light.p - x;
	double dist_to_light = sqrt(W.dot(W));
	W = W * (1.0 / dist_to_light);

	double r_over_d = light.r / dist_to_light;
	double costheta_max = sqrt(fmax(0.0, 1.0 - r_over_d * r_over_d));
	double costheta = 1.0 - xi1 * (1.0 - costheta_max);
	double sintheta = sqrt(fmax(0.0, 1.0 - costheta * costheta));
	double phi = 2.0 * M_PI * xi2;

	Vector s, t;
	localBasis(W, s, t);

	Vector local(sintheta * cos(phi), sintheta * sin(phi), costheta);
	Vector wi = s * local.x + t * local.y + W * local.z;
	wi.normalize();

	double pdf = 1.0 / (2.0 * M_PI * (1.0 - costheta_max));

	double costheta_x = fmax(0.0, n.dot(wi));
	if (costheta_x <= 0.0) {
		return {Vector(), 0, Color(), false};
	}

	Ray shadowRay(x + n * 1e-4, wi);
	double t_first = 1e20;
	int id_first = -1;
	for (int i = 0; i < 8; i++) {
		double dist = spheres[i].intersect(shadowRay);
		if (dist > 0.0 && dist < t_first) {
			t_first = dist;
			id_first = i;
		}
	}

	if (id_first < 0) {
		return {Vector(), 0, Color(), false};
	}

	Point hitPoint = shadowRay.o + shadowRay.d * t_first;
	Vector toCenter = light.p - hitPoint;
	double toCenterDist = sqrt(toCenter.dot(toCenter));

	// Visibility check: verify that the ray hits the light sphere
	// Allow a small tolerance for numerical precision
	if (fabs(toCenterDist - light.r) > 0.1) {
		return {Vector(), 0, Color(), false};
	}

	Color contrib = light.le * (costheta_x / pdf) * (1.0 / M_PI);
	return {wi, pdf, contrib, true};
}

Color samplePointLight(const Light &light, const Point &x, const Vector &n) {
	Vector toLight = light.p - x;
	double distToLight = sqrt(toLight.dot(toLight));
	toLight = toLight * (1.0 / distToLight);

	double costheta = n.dot(toLight);
	if (costheta <= 0.0) {
		return Color();
	}

	Ray shadowRay(x + n * 1e-4, toLight);

	bool occluded = false;
	for (int i = 0; i < 8; i++) {
		double dist = spheres[i].intersect(shadowRay);
		if (dist > 0.0 && dist < distToLight - 1e-4) {
			occluded = true;
			break;
		}
	}

	if (occluded) {
		return Color();
	}

	double distSq = distToLight * distToLight;
	Color L = light.le * (costheta / (M_PI * distSq));

	return L;
}

inline int toDisplayValue(const double x) {
	return int( pow( clamp(x), 1.0/2.2 ) * 255 + .5);
}

inline bool intersect(const Ray &r, double &t, int &id) {
	t = 1e20;
	bool found = false;
	for (int i = 0; i < 8; i++) {
		double dist = spheres[i].intersect(r);
		if (dist > 0.0 && dist < t) {
			t = dist;
			id = i;
			found = true;
		}
	}
	return found;
}

Color shade(const Ray &r, unsigned short *seed) {
	double t;
	int id = 0;
	if (!intersect(r, t, id))
		return Color();

	const Sphere &obj = spheres[id];

	if (obj.ke.x > 0.0 || obj.ke.y > 0.0 || obj.ke.z > 0.0) {
		return obj.ke;
	}

	Point x = r.o + r.d * t;
	Vector n = (x - obj.p);
	n.normalize();

	Color L;

	if (strcmp(samplingMode, "coshemi") == 0) {
		Sample sample = sampleDirection(n, seed);
		Vector wi = sample.d;
		double pdf = sample.pdf;

		Ray shadowRay(x + n * 1e-4, wi);

		double t_shadow;
		int id_shadow = 0;
		if (!intersect(shadowRay, t_shadow, id_shadow)) {
			L = Color();
		} else {
			const Sphere &target = spheres[id_shadow];

			if (target.ke.x <= 0.0 && target.ke.y <= 0.0 && target.ke.z <= 0.0) {
				L = Color();
			} else {
				double costheta = fmax(0.0, n.dot(wi));
				L = obj.c.mult(target.ke) * (costheta / (M_PI * pdf));
			}
		}

		for (const Light &light : lights) {
			if (light.isPoint) {
				Color pointContrib = samplePointLight(light, x, n);
				L = L + pointContrib.mult(obj.c);
			}
		}
	} else {
		L = Color();

		for (const Light &light : lights) {
			if (light.isPoint) {
				Color pointContrib = samplePointLight(light, x, n);
				L = L + pointContrib.mult(obj.c);
			} else {
				LightSample ls;
				if (strcmp(samplingMode, "arealight") == 0) {
					ls = sampleAreaLight(light, x, n, seed);
				} else {
					ls = sampleSolidAngleLight(light, x, n, seed);
				}

				if (ls.valid) {
					L = L + ls.le.mult(obj.c);
				}
			}
		}
	}

	return Color(clamp(L.x), clamp(L.y), clamp(L.z));
}


int main(int argc, char *argv[]) {
	int w = 1024, h = 768;

	Ray camera( Point(0, 11.2, 214), Vector(0, -0.042612, -1).normalize() );

	Vector cx = Vector( w * 0.5095 / h, 0., 0.);
	Vector cy = (cx % camera.d).normalize() * 0.5095;

	if (argc < 4) {
		fprintf(stderr, "Usage: ./rt <mode> <spp> <scene>\n");
		fprintf(stderr, "  mode: arealight, solidangle, coshemi\n");
		fprintf(stderr, "  scene: 1L, 2A1P\n");
		return 1;
	}

	samplingMode = argv[1];
	int spp = atoi(argv[2]);
	sceneMode = argv[3];

	if (strcmp(sceneMode, "1L") == 0) {
		Light light;
		light.r = 10.5;
		light.p = Point(0, 24.3, 0);
		light.c = Color(1, 1, 1);
		light.le = Color(10, 10, 10);
		light.isPoint = false;
		lights.push_back(light);
	} else if (strcmp(sceneMode, "2A1P") == 0) {
		Light pointLight;
		pointLight.r = 0.0;
		pointLight.p = Point(0, 24.3, 0);
		pointLight.c = Color(0, 0, 0);
		pointLight.le = Color(2000, 2000, 2000);
		pointLight.isPoint = true;
		lights.push_back(pointLight);

		Light areaLight1;
		areaLight1.r = 10.5;
		areaLight1.p = Point(-23, 24.3, 0);
		areaLight1.c = Color(1, 1, 1);
		areaLight1.le = Color(12, 5, 5);
		areaLight1.isPoint = false;
		lights.push_back(areaLight1);

		Light areaLight2;
		areaLight2.r = 5.0;
		areaLight2.p = Point(23, 24.3, -50);
		areaLight2.c = Color(1, 1, 1);
		areaLight2.le = Color(5, 5, 12);
		areaLight2.isPoint = false;
		lights.push_back(areaLight2);
	} else {
		fprintf(stderr, "Unknown scene: %s\n", sceneMode);
		return 1;
	}

	Color *pixelColors = new Color[w * h];

	#pragma omp parallel for schedule(static)
	for(int y = 0; y < h; y++)
	{
		fprintf(stderr,"\r%5.2f%%",100.*y/(h-1));
		for(int x = 0; x < w; x++ ) {
			int idx = (h - y - 1) * w + x;

			unsigned short seed[3];
			seed[0] = (unsigned short)(idx & 0xFFFF);
			seed[1] = (unsigned short)((idx >> 16) & 0xFFFF);
			seed[2] = 0;

			Vector cameraRayDir = cx * ( double(x)/w - .5) + cy * ( double(y)/h - .5) + camera.d;
			Ray cameraRay(camera.o, cameraRayDir.normalize());

			Color pixelValue = Color();
			for (int s = 0; s < spp; s++) {
				pixelValue = pixelValue + shade(cameraRay, seed);
			}
			pixelValue = pixelValue * (1.0 / spp);

			pixelColors[idx] = Color(clamp(pixelValue.x), clamp(pixelValue.y), clamp(pixelValue.z));
		}
	}

	fprintf(stderr,"\n");

	FILE *f = fopen("image.ppm", "w");
	fprintf(f, "P3\n%d %d\n%d\n", w, h, 255);
	for (int p = 0; p < w * h; p++)
	{
		fprintf(f,"%d %d %d ", toDisplayValue(pixelColors[p].x), toDisplayValue(pixelColors[p].y),
			toDisplayValue(pixelColors[p].z));
	}
	fclose(f);

	delete[] pixelColors;

	return 0;
}
