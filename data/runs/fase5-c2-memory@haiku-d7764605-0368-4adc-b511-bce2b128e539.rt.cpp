// rt: raytracer con microfacets
// g++ -O3 -fopenmp rt.cpp -o rt
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <omp.h>

class Vector
{
public:
	double x, y, z;

	Vector(double x_= 0, double y_= 0, double z_= 0){ x=x_; y=y_; z=z_; }

	Vector operator+(const Vector &b) const { return Vector(x + b.x, y + b.y, z + b.z); }
	Vector operator-(const Vector &b) const { return Vector(x - b.x, y - b.y, z - b.z); }
	Vector operator-() const { return Vector(-x, -y, -z); }
	Vector operator*(double b) const { return Vector(x * b, y * b, z * b); }

	Vector operator%(const Vector &b) const {return Vector(y * b.z - z * b.y, z * b.x - x * b.z, x * b.y - y * b.x);}

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

inline void build_basis(const Vector &n, Vector &t, Vector &s);

struct Sample {
	Vector wi;
	double pdf;
};

class Material {
public:
	virtual ~Material() {}
	virtual Sample sample(unsigned short *xi, const Vector &n, const Vector &wo) const = 0;
	virtual Color eval(const Vector &n, const Vector &wo, const Vector &wi) const = 0;
	virtual double pdf(const Vector &n, const Vector &wo, const Vector &wi) const = 0;
};

class DiffuseMaterial : public Material {
public:
	Color albedo;
	DiffuseMaterial(Color albedo_) : albedo(albedo_) {}

	Sample sample(unsigned short *xi, const Vector &n, const Vector &wo) const {
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
		Sample result = {wi, cos_theta / M_PI};
		return result;
	}

	Color eval(const Vector &n, const Vector &wo, const Vector &wi) const {
		double cos_theta = n.dot(wi);
		if (cos_theta <= 0) return Color();
		return albedo * (1.0 / M_PI);
	}

	double pdf(const Vector &n, const Vector &wo, const Vector &wi) const {
		double cos_theta = n.dot(wi);
		if (cos_theta <= 0) return 0.0;
		return cos_theta / M_PI;
	}
};

class ConductorMaterial : public Material {
public:
	Color eta, k;
	double alpha;

	ConductorMaterial(Color eta_, Color k_, double alpha_) : eta(eta_), k(k_), alpha(alpha_) {}

	Color fresnelConductor(double cosTheta, double eta_ch, double k_ch) const {
		if (cosTheta < 0) cosTheta = -cosTheta;
		double cosTheta2 = cosTheta * cosTheta;
		double sinTheta2 = 1.0 - cosTheta2;

		double eta2 = eta_ch * eta_ch;
		double k2 = k_ch * k_ch;

		double a2b2_term = eta2 - k2 - sinTheta2;
		double a2b2 = sqrt(a2b2_term * a2b2_term + 4.0 * eta2 * k2);
		double A = sqrt((a2b2 + eta2 - k2 - sinTheta2) / 2.0);

		double R_perp_num = a2b2 + cosTheta2 - 2.0 * A * cosTheta;
		double R_perp_den = a2b2 + cosTheta2 + 2.0 * A * cosTheta;
		double R_perp = R_perp_num / R_perp_den;

		double sinTheta4 = sinTheta2 * sinTheta2;
		double a2b2_cos2 = a2b2 * cosTheta2;
		double A_cosTheta_sin2 = A * cosTheta * sinTheta2;

		double R_parallel_num = R_perp * (a2b2_cos2 + sinTheta4 - 2.0 * A_cosTheta_sin2);
		double R_parallel_den = a2b2_cos2 + sinTheta4 + 2.0 * A_cosTheta_sin2;
		double R_parallel = R_parallel_num / R_parallel_den;

		return Color((R_perp + R_parallel) / 2.0, 0, 0);
	}

	Color beckmannD(const Vector &wh, const Vector &n) const {
		double cosTheta_h = n.dot(wh);
		if (cosTheta_h < 1e-6) return Color();

		double tanTheta = sqrt(1.0 - cosTheta_h * cosTheta_h) / cosTheta_h;
		double alpha2 = alpha * alpha;
		double exp_term = exp(-(tanTheta * tanTheta) / alpha2);
		double cos4 = cosTheta_h * cosTheta_h * cosTheta_h * cosTheta_h;

		double result = exp_term / (M_PI * alpha2 * cos4);
		return Color(result, 0, 0);
	}

	double smithG1(const Vector &v, const Vector &n, const Vector &wh) const {
		double tanTheta_v = sqrt(1.0 - fabs(n.dot(v)) * fabs(n.dot(v))) / fabs(n.dot(v));
		double a = 1.0 / (alpha * tanTheta_v);

		double chi_plus = (v.dot(wh) / v.dot(n) > 0) ? 1.0 : 0.0;

		if (a >= 1.6) return chi_plus;

		double a2 = a * a;
		double num = 3.535 * a + 2.181 * a2;
		double den = 1.0 + 2.276 * a + 2.577 * a2;
		return chi_plus * (num / den);
	}

	Color smithG(const Vector &wo, const Vector &wi, const Vector &wh, const Vector &n) const {
		double G1_o = smithG1(wo, n, wh);
		double G1_i = smithG1(wi, n, wh);
		return Color(G1_o * G1_i, 0, 0);
	}

	Sample sample(unsigned short *xi, const Vector &n, const Vector &wo) const {
		double xi1 = erand48(xi);
		double xi2 = erand48(xi);

		double theta_h = atan(sqrt(-alpha * alpha * log(1.0 - xi1)));
		double phi_h = 2.0 * M_PI * xi2;

		Vector t, s;
		build_basis(n, t, s);

		double sin_theta_h = sin(theta_h);
		double cos_theta_h = cos(theta_h);

		Vector wh = s * (sin_theta_h * cos(phi_h)) + t * (sin_theta_h * sin(phi_h)) + n * cos_theta_h;
		wh.normalize();

		double wi_dot_wh = 2.0 * (wo.dot(wh));
		Vector wi = wh * wi_dot_wh - wo;

		double cos_theta_h_calc = n.dot(wh);
		double pdf_val = 0.0;
		if (fabs(wo.dot(wh)) > 1e-6) {
			Color D = beckmannD(wh, n);
			pdf_val = D.x * cos_theta_h_calc / (4.0 * fabs(wo.dot(wh)));
		}

		Sample result = {wi, pdf_val};
		return result;
	}

	Color eval(const Vector &n, const Vector &wo, const Vector &wi) const {
		double cosTheta_o = n.dot(wo);
		double cosTheta_i = n.dot(wi);

		if (cosTheta_o <= 0 || cosTheta_i <= 0) return Color();

		Vector wh = (wo + wi);
		wh.normalize();

		Color D = beckmannD(wh, n);
		Color G = smithG(wo, wi, wh, n);

		double cosTheta_wh = wo.dot(wh);
		double F_r = fresnelConductor(cosTheta_wh, eta.x, k.x).x;
		double F_g = fresnelConductor(cosTheta_wh, eta.y, k.y).x;
		double F_b = fresnelConductor(cosTheta_wh, eta.z, k.z).x;

		double denom = 4.0 * cosTheta_o * cosTheta_i;
		if (denom < 1e-6) return Color();

		Color fr = Color(
			D.x * G.x * F_r / denom,
			D.x * G.x * F_g / denom,
			D.x * G.x * F_b / denom
		);

		return Color(fmax(0, fr.x), fmax(0, fr.y), fmax(0, fr.z));
	}

	double pdf(const Vector &n, const Vector &wo, const Vector &wi) const {
		Vector wh = (wo + wi);
		wh.normalize();

		double cos_theta_h = n.dot(wh);
		if (fabs(wo.dot(wh)) < 1e-6 || cos_theta_h < 1e-6) return 0.0;

		Color D = beckmannD(wh, n);
		return D.x * cos_theta_h / (4.0 * fabs(wo.dot(wh)));
	}
};

class Sphere
{
public:
	double r;
	Point p;
	Color ke;
	Material *material;

	Sphere() : r(0), p(), ke(), material(nullptr) {}
	Sphere(double r_, Point p_, Material *mat_, Color ke_ = Color()) : r(r_), p(p_), material(mat_), ke(ke_) {}

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

Sphere spheres[16];
int num_spheres = 0;
int num_emitters = 0;

void init_scene(const std::string &scene_name) {
	spheres[0] = Sphere(1e5, Point(-1e5 - 49, 0, 0), new DiffuseMaterial(Color(.75, .25, .25)), Color(0, 0, 0));
	spheres[1] = Sphere(1e5, Point(1e5 + 49, 0, 0), new DiffuseMaterial(Color(.25, .25, .75)), Color(0, 0, 0));
	spheres[2] = Sphere(1e5, Point(0, 0, -1e5 - 81.6), new DiffuseMaterial(Color(.25, .75, .25)), Color(0, 0, 0));
	spheres[3] = Sphere(1e5, Point(0, -1e5 - 40.8, 0), new DiffuseMaterial(Color(.25, .75, .75)), Color(0, 0, 0));
	spheres[4] = Sphere(1e5, Point(0, 1e5 + 40.8, 0), new DiffuseMaterial(Color(.75, .75, .25)), Color(0, 0, 0));

	if (scene_name == "point" || scene_name == "areal" || scene_name == "multil") {
		spheres[5] = Sphere(16.5, Point(-23, -24.3, -34.6),
			new ConductorMaterial(Color(1.66058, 0.88143, 0.521467), Color(9.2282, 6.27077, 4.83803), 0.3),
			Color(0, 0, 0));
		spheres[6] = Sphere(16.5, Point(23, -24.3, -3.6),
			new ConductorMaterial(Color(0.143245, 0.377423, 1.43919), Color(3.98479, 2.3847, 1.60434), 0.3),
			Color(0, 0, 0));
	} else {
		spheres[5] = Sphere(16.5, Point(-23, -24.3, -34.6),
			new ConductorMaterial(Color(1.66058, 0.88143, 0.521467), Color(9.2282, 6.27077, 4.83803), 0.3),
			Color(0, 0, 0));
		spheres[6] = Sphere(16.5, Point(23, -24.3, -3.6),
			new ConductorMaterial(Color(0.143245, 0.377423, 1.43919), Color(3.98479, 2.3847, 1.60434), 0.3),
			Color(0, 0, 0));
	}

	if (scene_name == "point") {
		spheres[7] = Sphere(0.0, Point(0, 24.3, 0), new DiffuseMaterial(Color(0, 0, 0)), Color(4000, 4000, 4000));
		num_spheres = 8;
		num_emitters = 1;
	} else if (scene_name == "areal") {
		spheres[7] = Sphere(10.5, Point(0, 24.3, 0), new DiffuseMaterial(Color(1, 1, 1)), Color(10, 10, 10));
		num_spheres = 8;
		num_emitters = 1;
	} else if (scene_name == "multil") {
		spheres[5] = Sphere(16.5, Point(-23, -24.3, -34.6),
			new ConductorMaterial(Color(1.66058, 0.88143, 0.521467), Color(9.2282, 6.27077, 4.83803), 0.03),
			Color(0, 0, 0));
		spheres[6] = Sphere(16.5, Point(23, -24.3, -3.6),
			new ConductorMaterial(Color(0.143245, 0.377423, 1.43919), Color(3.98479, 2.3847, 1.60434), 0.3),
			Color(0, 0, 0));
		spheres[7] = Sphere(0.0, Point(0, 24.3, 0), new DiffuseMaterial(Color(0, 0, 0)), Color(2000, 2000, 2000));
		spheres[8] = Sphere(10.5, Point(-23, 24.3, 0), new DiffuseMaterial(Color(1, 1, 1)), Color(12, 5, 5));
		spheres[9] = Sphere(5, Point(23, 24.3, -50), new DiffuseMaterial(Color(1, 1, 1)), Color(5, 5, 12));
		num_spheres = 10;
		num_emitters = 3;
	} else if (scene_name == "2a1p") {
		spheres[7] = Sphere(0.0, Point(0, 24.3, 0), new DiffuseMaterial(Color(0, 0, 0)), Color(2000, 2000, 2000));
		spheres[8] = Sphere(10.5, Point(-23, 24.3, 0), new DiffuseMaterial(Color(1, 1, 1)), Color(12, 5, 5));
		spheres[9] = Sphere(5, Point(23, 24.3, -50), new DiffuseMaterial(Color(1, 1, 1)), Color(5, 5, 12));
		num_spheres = 10;
		num_emitters = 3;
	} else {
		fprintf(stderr, "Escena desconocida: %s\n", scene_name.c_str());
		exit(1);
	}
}

inline double clamp(const double x) {
	if(x < 0.0)
		return 0.0;
	else if(x > 1.0)
		return 1.0;
	return x;
}

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


struct LightSample {
	Vector omega;
	double pdf_omega;
	Point xprime;
};

LightSample sampleAreaLight(unsigned short *xi, const Point &x, const Sphere &light) {
	if (light.r <= 1e-6) {
		LightSample result = {{0,0,0}, 1.0, light.p};
		return result;
	}

	double u1 = erand48(xi);
	double u2 = erand48(xi);

	double cos_theta = 1.0 - 2.0 * u1;
	double sin_theta = sqrt(1.0 - cos_theta * cos_theta);
	double phi = 2.0 * M_PI * u2;

	Vector ex(1, 0, 0);
	Vector ey, ez;
	build_basis(ex, ey, ez);

	Vector omega_dir_sample = ex * cos_theta + ey * (sin_theta * cos(phi)) + ez * (sin_theta * sin(phi));
	Point xprime = light.p + omega_dir_sample * light.r;

	Vector delta = xprime - x;
	double d = sqrt(delta.dot(delta));
	Vector omega_dir = delta * (1.0 / d);

	Vector n_xprime = omega_dir_sample;
	double cos_theta_prime = -n_xprime.dot(omega_dir);

	if (cos_theta_prime <= 0) {
		LightSample result = {{0,0,0}, 1e20, xprime};
		return result;
	}

	double pdf_omega = d * d / (4.0 * M_PI * light.r * light.r * cos_theta_prime);

	LightSample result;
	result.omega = omega_dir;
	result.pdf_omega = pdf_omega;
	result.xprime = xprime;
	return result;
}

LightSample sampleSolidAngle(unsigned short *xi, const Point &x, const Sphere &light) {
	if (light.r <= 1e-6) {
		LightSample result = {{0,0,0}, 1.0, light.p};
		return result;
	}

	Vector W = light.p - x;
	double dist_to_center = sqrt(W.dot(W));
	W = W * (1.0 / dist_to_center);

	double ratio = light.r / dist_to_center;
	if (ratio > 1.0) ratio = 1.0;
	double cos_theta_max = sqrt(1.0 - ratio * ratio);

	double u1 = erand48(xi);
	double u2 = erand48(xi);

	double cos_theta = 1.0 - u1 + u1 * cos_theta_max;
	double sin_theta = sqrt(1.0 - cos_theta * cos_theta);
	double phi = 2.0 * M_PI * u2;

	Vector ey, ez;
	build_basis(W, ey, ez);

	Vector omega = W * cos_theta + ey * (sin_theta * cos(phi)) + ez * (sin_theta * sin(phi));

	Vector oc = x - light.p;
	double a = omega.dot(omega);
	double b = 2.0 * oc.dot(omega);
	double c = oc.dot(oc) - light.r * light.r;
	double disc = b * b - 4.0 * a * c;

	Point xprime = light.p;
	if (disc >= 0) {
		double t1 = (-b - sqrt(disc)) / (2.0 * a);
		double t2 = (-b + sqrt(disc)) / (2.0 * a);
		double t_hit = (t1 > 1e-4) ? t1 : t2;
		if (t_hit > 1e-4) {
			xprime = x + omega * t_hit;
		}
	}

	double pdf_omega = 1.0 / (2.0 * M_PI * (1.0 - cos_theta_max));

	LightSample result;
	result.omega = omega;
	result.pdf_omega = pdf_omega;
	result.xprime = xprime;
	return result;
}

double pdfSolidAngleLight(const Point &x, const Sphere &light, const Vector &omega) {
	if (light.r <= 1e-6) return 0.0;

	Vector W = light.p - x;
	double dist_to_center = sqrt(W.dot(W));
	W = W * (1.0 / dist_to_center);

	double ratio = light.r / dist_to_center;
	if (ratio > 1.0) ratio = 1.0;
	double cos_theta_max = sqrt(1.0 - ratio * ratio);

	double cos_theta = W.dot(omega);
	if (cos_theta < cos_theta_max) return 0.0;

	return 1.0 / (2.0 * M_PI * (1.0 - cos_theta_max));
}

Color shade_issa(const Ray &r, unsigned short *xi) {
	double t;
	int id = 0;

	if (!intersect(r, t, id))
		return Color();

	const Sphere &obj = spheres[id];

	if (obj.ke.x > 0 || obj.ke.y > 0 || obj.ke.z > 0) {
		return obj.ke;
	}

	if (!obj.material) return Color();

	Point x = r.o + r.d * t;
	Vector n = (x - obj.p);
	n.normalize();

	Vector wo = -r.d;
	Color radiance = Color();

	for (int emitter_idx = 0; emitter_idx < num_emitters; emitter_idx++) {
		int sphere_idx = num_spheres - num_emitters + emitter_idx;
		const Sphere &light = spheres[sphere_idx];

		if (light.r <= 1e-6) {
			Vector w = light.p - x;
			double r_dist = sqrt(w.dot(w));
			w = w * (1.0 / r_dist);

			double cos_theta = n.dot(w);
			if (cos_theta > 0) {
				Ray shadowRay(x + n * 1e-4, w);
				double t_hit;
				int id_hit;
				bool visible = true;

				if (intersect(shadowRay, t_hit, id_hit)) {
					if (t_hit < r_dist - 1e-3) {
						visible = false;
					}
				}

				if (visible) {
					Color fr = obj.material->eval(n, wo, w);
					Color contrib = fr.mult(light.ke) * (cos_theta / (r_dist * r_dist));
					radiance = radiance + contrib;
				}
			}
		} else {
			LightSample light_sample = sampleSolidAngle(xi, x, light);

			Vector omega = light_sample.omega;
			double pdf_omega = light_sample.pdf_omega;

			double cos_theta = n.dot(omega);
			if (cos_theta > 0 && pdf_omega > 0 && pdf_omega < 1e10) {
				Ray shadowRay(x + n * 1e-4, omega);

				double t_hit;
				int id_hit;
				bool visible = false;

				if (intersect(shadowRay, t_hit, id_hit)) {
					if (id_hit == sphere_idx) {
						visible = true;
					}
				}

				if (visible) {
					Color fr = obj.material->eval(n, wo, omega);
					Color contrib = fr.mult(light.ke) * (cos_theta / pdf_omega);
					radiance = radiance + contrib;
				}
			}
		}
	}

	return radiance;
}

Color shade_isarea(const Ray &r, unsigned short *xi) {
	double t;
	int id = 0;

	if (!intersect(r, t, id))
		return Color();

	const Sphere &obj = spheres[id];

	if (obj.ke.x > 0 || obj.ke.y > 0 || obj.ke.z > 0) {
		return obj.ke;
	}

	Point x = r.o + r.d * t;
	Vector n = (x - obj.p);
	n.normalize();

	Vector wo = -r.d;
	Color radiance = Color();

	for (int emitter_idx = 0; emitter_idx < num_emitters; emitter_idx++) {
		int sphere_idx = num_spheres - num_emitters + emitter_idx;
		const Sphere &light = spheres[sphere_idx];

		if (light.r <= 1e-6) {
			Vector w = light.p - x;
			double r_dist = sqrt(w.dot(w));
			w = w * (1.0 / r_dist);

			double cos_theta = n.dot(w);
			if (cos_theta > 0) {
				Ray shadowRay(x + n * 1e-4, w);
				double t_hit;
				int id_hit;
				bool visible = true;

				if (intersect(shadowRay, t_hit, id_hit)) {
					if (t_hit < r_dist - 1e-3) {
						visible = false;
					}
				}

				if (visible) {
					Color fr = obj.material->eval(n, wo, w);
					Color contrib = fr.mult(light.ke) * (cos_theta / (r_dist * r_dist));
					radiance = radiance + contrib;
				}
			}
		} else {
			LightSample light_sample = sampleAreaLight(xi, x, light);

			Vector omega = light_sample.omega;
			double pdf_omega = light_sample.pdf_omega;

			double cos_theta = n.dot(omega);
			if (cos_theta > 0 && pdf_omega > 0 && pdf_omega < 1e10) {
				Ray shadowRay(x + n * 1e-4, omega);

				double t_hit;
				int id_hit;
				bool visible = false;

				if (intersect(shadowRay, t_hit, id_hit)) {
					if (id_hit == sphere_idx) {
						visible = true;
					}
				}

				if (visible) {
					Color fr = obj.material->eval(n, wo, omega);
					Color contrib = fr.mult(light.ke) * (cos_theta / pdf_omega);
					radiance = radiance + contrib;
				}
			}
		}
	}

	return radiance;
}

Color shade_isbrdf(const Ray &r, unsigned short *xi) {
	double t;
	int id = 0;

	if (!intersect(r, t, id))
		return Color();

	const Sphere &obj = spheres[id];

	if (obj.ke.x > 0 || obj.ke.y > 0 || obj.ke.z > 0) {
		return obj.ke;
	}

	if (!obj.material) return Color();

	Point x = r.o + r.d * t;
	Vector n = (x - obj.p);
	n.normalize();

	Vector wo = -r.d;

	Sample sample = obj.material->sample(xi, n, wo);
	Vector wi = sample.wi;
	double pdf = sample.pdf;

	if (pdf < 1e-6) return Color();

	Ray sampleRay(x + n * 1e-4, wi);

	double t_hit;
	int id_hit;
	Color radiance = Color();

	if (intersect(sampleRay, t_hit, id_hit)) {
		const Sphere &emitter = spheres[id_hit];
		Color Le = emitter.ke;

		if (Le.x > 0 || Le.y > 0 || Le.z > 0) {
			double cos_theta = n.dot(wi);
			if (cos_theta > 0) {
				Color fr = obj.material->eval(n, wo, wi);
				radiance = fr.mult(Le) * (cos_theta / pdf);
			}
		}
	}

	return radiance;
}

Color shade_mis(const Ray &r, unsigned short *xi) {
	double t;
	int id = 0;

	if (!intersect(r, t, id))
		return Color();

	const Sphere &obj = spheres[id];

	if (obj.ke.x > 0 || obj.ke.y > 0 || obj.ke.z > 0) {
		return obj.ke;
	}

	if (!obj.material) return Color();

	Point x = r.o + r.d * t;
	Vector n = (x - obj.p);
	n.normalize();

	Vector wo = -r.d;
	Color radiance = Color();

	for (int emitter_idx = 0; emitter_idx < num_emitters; emitter_idx++) {
		int sphere_idx = num_spheres - num_emitters + emitter_idx;
		const Sphere &light = spheres[sphere_idx];

		if (light.r <= 1e-6) {
			Vector w = light.p - x;
			double r_dist = sqrt(w.dot(w));
			w = w * (1.0 / r_dist);

			double cos_theta = n.dot(w);
			if (cos_theta > 0) {
				Ray shadowRay(x + n * 1e-4, w);
				double t_hit;
				int id_hit;
				bool visible = true;

				if (intersect(shadowRay, t_hit, id_hit)) {
					if (t_hit < r_dist - 1e-3) {
						visible = false;
					}
				}

				if (visible) {
					Color fr = obj.material->eval(n, wo, w);
					Color contrib = fr.mult(light.ke) * (cos_theta / (r_dist * r_dist));
					radiance = radiance + contrib;
				}
			}
		} else {
			double u1 = erand48(xi);
			double u2 = erand48(xi);

			Vector W = light.p - x;
			double dist_to_center = sqrt(W.dot(W));
			W = W * (1.0 / dist_to_center);

			double ratio = light.r / dist_to_center;
			if (ratio > 1.0) ratio = 1.0;
			double cos_theta_max = sqrt(1.0 - ratio * ratio);

			double cos_theta_l = 1.0 - u1 + u1 * cos_theta_max;
			double sin_theta_l = sqrt(1.0 - cos_theta_l * cos_theta_l);
			double phi_l = 2.0 * M_PI * u2;

			Vector ey, ez;
			build_basis(W, ey, ez);
			Vector omega_l = W * cos_theta_l + ey * (sin_theta_l * cos(phi_l)) + ez * (sin_theta_l * sin(phi_l));

			Vector oc = x - light.p;
			double a = omega_l.dot(omega_l);
			double b = 2.0 * oc.dot(omega_l);
			double c = oc.dot(oc) - light.r * light.r;
			double disc = b * b - 4.0 * a * c;

			Point xprime_l = light.p;
			if (disc >= 0) {
				double t1 = (-b - sqrt(disc)) / (2.0 * a);
				double t2 = (-b + sqrt(disc)) / (2.0 * a);
				double t_hit = (t1 > 1e-4) ? t1 : t2;
				if (t_hit > 1e-4) {
					xprime_l = x + omega_l * t_hit;
				}
			}

			double pdf_light_l = 1.0 / (2.0 * M_PI * (1.0 - cos_theta_max));

			double cos_theta_light = n.dot(omega_l);
			if (cos_theta_light > 0 && pdf_light_l > 0 && pdf_light_l < 1e10) {
				Ray shadowRay(x + n * 1e-4, omega_l);

				double t_hit;
				int id_hit;
				bool visible = false;

				if (intersect(shadowRay, t_hit, id_hit)) {
					if (id_hit == sphere_idx) {
						visible = true;
					}
				}

				if (visible) {
					Color fr = obj.material->eval(n, wo, omega_l);
					double pdf_brdf_l = obj.material->pdf(n, wo, omega_l);

					double w_light = pdf_light_l * pdf_light_l / (pdf_light_l * pdf_light_l + pdf_brdf_l * pdf_brdf_l);
					if (isfinite(w_light) && w_light >= 0 && w_light <= 1.0) {
						Color contrib = fr.mult(light.ke) * (cos_theta_light / pdf_light_l) * w_light;
						radiance = radiance + contrib;
					}
				}
			}

			Sample sample = obj.material->sample(xi, n, wo);
			Vector wr = sample.wi;
			double pdf_brdf_r = sample.pdf;

			if (pdf_brdf_r > 1e-6 && isfinite(pdf_brdf_r)) {
				Ray sampleRay(x + n * 1e-4, wr);

				double t_hit;
				int id_hit;

				if (intersect(sampleRay, t_hit, id_hit)) {
					if (id_hit == sphere_idx) {
						double cos_theta_brdf = n.dot(wr);
						if (cos_theta_brdf > 0) {
							Color fr = obj.material->eval(n, wo, wr);
							double pdf_light_r = pdfSolidAngleLight(x, light, wr);

							double w_brdf = pdf_brdf_r * pdf_brdf_r / (pdf_brdf_r * pdf_brdf_r + pdf_light_r * pdf_light_r);
							if (isfinite(w_brdf) && w_brdf >= 0 && w_brdf <= 1.0) {
								Color contrib = fr.mult(light.ke) * (cos_theta_brdf / pdf_brdf_r) * w_brdf;
								radiance = radiance + contrib;
							}
						}
					}
				}
			}
		}
	}

	return radiance;
}

Color shade_ptexp_recursive(const Ray &r, int bounce, int max_bounce, unsigned short *xi) {
	double t;
	int id = 0;

	if (!intersect(r, t, id))
		return Color();

	const Sphere &obj = spheres[id];

	if (obj.ke.x > 0 || obj.ke.y > 0 || obj.ke.z > 0) {
		if (bounce == 1) {
			return obj.ke;
		} else {
			return Color();
		}
	}

	if (!obj.material) return Color();

	Point x = r.o + r.d * t;
	Vector n = (x - obj.p);
	n.normalize();

	Vector wo = -r.d;

	Color Ld = Color();

	for (int emitter_idx = 0; emitter_idx < num_emitters; emitter_idx++) {
		int sphere_idx = num_spheres - num_emitters + emitter_idx;
		const Sphere &light = spheres[sphere_idx];

		if (light.r <= 1e-6) {
			Vector w = light.p - x;
			double r_dist = sqrt(w.dot(w));
			w = w * (1.0 / r_dist);

			double cos_theta = n.dot(w);
			if (cos_theta > 0) {
				Ray shadowRay(x + n * 1e-4, w);
				double t_hit;
				int id_hit;
				bool visible = true;

				if (intersect(shadowRay, t_hit, id_hit)) {
					if (t_hit < r_dist - 1e-3) {
						visible = false;
					}
				}

				if (visible) {
					Color fr = obj.material->eval(n, wo, w);
					Color contrib = fr.mult(light.ke) * (cos_theta / (r_dist * r_dist));
					Ld = Ld + contrib;
				}
			}
		} else {
			double u1 = erand48(xi);
			double u2 = erand48(xi);

			Vector W = light.p - x;
			double dist_to_center = sqrt(W.dot(W));
			W = W * (1.0 / dist_to_center);

			double ratio = light.r / dist_to_center;
			if (ratio > 1.0) ratio = 1.0;
			double cos_theta_max = sqrt(1.0 - ratio * ratio);

			double cos_theta_l = 1.0 - u1 + u1 * cos_theta_max;
			double sin_theta_l = sqrt(1.0 - cos_theta_l * cos_theta_l);
			double phi_l = 2.0 * M_PI * u2;

			Vector ey, ez;
			build_basis(W, ey, ez);
			Vector omega_l = W * cos_theta_l + ey * (sin_theta_l * cos(phi_l)) + ez * (sin_theta_l * sin(phi_l));

			Vector oc = x - light.p;
			double a = omega_l.dot(omega_l);
			double b = 2.0 * oc.dot(omega_l);
			double c = oc.dot(oc) - light.r * light.r;
			double disc = b * b - 4.0 * a * c;

			Point xprime_l = light.p;
			if (disc >= 0) {
				double t1 = (-b - sqrt(disc)) / (2.0 * a);
				double t2 = (-b + sqrt(disc)) / (2.0 * a);
				double t_hit = (t1 > 1e-4) ? t1 : t2;
				if (t_hit > 1e-4) {
					xprime_l = x + omega_l * t_hit;
				}
			}

			double pdf_light = 1.0 / (2.0 * M_PI * (1.0 - cos_theta_max));

			double cos_theta_light = n.dot(omega_l);
			if (cos_theta_light > 0 && pdf_light > 0 && pdf_light < 1e10) {
				Ray shadowRay(x + n * 1e-4, omega_l);

				double t_hit;
				int id_hit;
				bool visible = false;

				if (intersect(shadowRay, t_hit, id_hit)) {
					if (id_hit == sphere_idx) {
						visible = true;
					}
				}

				if (visible) {
					Color fr = obj.material->eval(n, wo, omega_l);
					double pdf_brdf = obj.material->pdf(n, wo, omega_l);

					double w_light = pdf_light * pdf_light / (pdf_light * pdf_light + pdf_brdf * pdf_brdf);
					Color contrib = fr.mult(light.ke) * (cos_theta_light / pdf_light) * w_light;
					Ld = Ld + contrib;
				}
			}

			Sample sample = obj.material->sample(xi, n, wo);
			Vector wi = sample.wi;
			double pdf_brdf = sample.pdf;

			if (pdf_brdf > 1e-6) {
				Ray sampleRay(x + n * 1e-4, wi);

				double t_hit;
				int id_hit;

				if (intersect(sampleRay, t_hit, id_hit)) {
					if (id_hit == sphere_idx) {
						double cos_theta_brdf = n.dot(wi);
						if (cos_theta_brdf > 0) {
							Color fr = obj.material->eval(n, wo, wi);
							double pdf_light_eval = obj.material->pdf(n, wo, wi);

							double w_brdf = pdf_brdf * pdf_brdf / (pdf_brdf * pdf_brdf + pdf_light * pdf_light);
							if (w_brdf > 0 && w_brdf <= 1.0) {
								Color contrib = fr.mult(light.ke) * (cos_theta_brdf / pdf_brdf) * w_brdf;
								Ld = Ld + contrib;
							}
						}
					}
				}
			}
		}
	}

	if (bounce == max_bounce) {
		return Ld;
	}

	Sample sample = obj.material->sample(xi, n, wo);
	Vector wr = sample.wi;
	double pr = sample.pdf;

	if (pr > 1e-6 && !isnan(pr) && isfinite(pr)) {
		double cos_r = n.dot(wr);
		if (cos_r > 0) {
			Ray nextRay(x + n * 1e-4, wr);
			Color Lind = shade_ptexp_recursive(nextRay, bounce + 1, max_bounce, xi);

			Color fr = obj.material->eval(n, wo, wr);
			Color contrib = fr.mult(Lind) * (cos_r / pr);
			return Ld + contrib;
		}
	}

	return Ld;
}

Color shade_ptexp(const Ray &r, int max_bounce, unsigned short *xi) {
	return shade_ptexp_recursive(r, 1, max_bounce, xi);
}


int main(int argc, char *argv[]) {
	if (argc < 3) {
		fprintf(stderr, "Uso: %s <modo> [args...]\n", argv[0]);
		fprintf(stderr, "  modo issa/isarea/isbrdf <spp> <escena>\n");
		fprintf(stderr, "  modo ptexp <spp> <escena> [B]\n");
		return 1;
	}

	std::string mode(argv[1]);
	int spp = 0;
	std::string scene_name;
	int max_bounce = 10;

	if (mode == "ptexp") {
		if (argc < 4) {
			fprintf(stderr, "Uso: %s ptexp <spp> <escena> [B]\n", argv[0]);
			return 1;
		}
		spp = atoi(argv[2]);
		scene_name = argv[3];
		if (argc >= 5) {
			max_bounce = atoi(argv[4]);
		}
	} else if (mode == "issa" || mode == "isarea" || mode == "isbrdf") {
		if (argc < 4) {
			fprintf(stderr, "Uso: %s %s <spp> <escena>\n", argv[0], mode.c_str());
			return 1;
		}
		spp = atoi(argv[2]);
		scene_name = argv[3];
	} else {
		fprintf(stderr, "Modo desconocido: %s\n", mode.c_str());
		return 1;
	}

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

			unsigned short xi[3];
			xi[0] = (idx >> 16) & 0xFFFF;
			xi[1] = (idx >> 0) & 0xFFFF;
			xi[2] = 1;

			for (int s = 0; s < spp; s++) {
				Color sample;
				if (mode == "issa") {
					sample = shade_issa(primaryRay, xi);
				} else if (mode == "isarea") {
					sample = shade_isarea(primaryRay, xi);
				} else if (mode == "isbrdf") {
					sample = shade_isbrdf(primaryRay, xi);
				} else if (mode == "ptexp") {
					sample = shade_ptexp(primaryRay, max_bounce, xi);
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
