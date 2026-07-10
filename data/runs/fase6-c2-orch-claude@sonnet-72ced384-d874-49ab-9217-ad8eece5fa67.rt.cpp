// rt: un lanzador de rayos minimalista
 // g++ -O3 -fopenmp rt.cpp -o rt
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <omp.h>
#include <string.h>

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
	Vector operator%(const Vector&b) const {return Vector(y * b.z - z * b.y, z * b.x - x * b.z, x * b.y - y * b.x);}
	
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
	Color ke;	// emisión

	Sphere(double r_, Point p_, Color c_, Color ke_ = Color()): r(r_), p(p_), c(c_), ke(ke_) {}
  
	// PROYECTO 1
	// determina si el rayo intersecta a esta esfera
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

// Fuente puntual para fase 02: representada como esfera de radio 0 (nunca se intersecta)
// Decisión de diseño: usar esfera con r=0 y ke=I para mantener arquitectura modular
struct PointLight {
	Point pos;      // Posición: (0, 24.3, 0)
	Color intensity; // Intensidad radiante: (4000, 4000, 4000)
};

// Índice de la esfera emisora (para skip en modo point light)
const int EMITTER_SPHERE_INDEX = 7;

// Esferas escena (mantiene la esfera emisora de fase 1 para compatibilidad MC)
Sphere spheres[] = {
	//Escena: radio, posicion, albedo, emisión
	Sphere(1e5,  Point(-1e5 - 49, 0, 0),   Color(.75, .25, .25), Color()), // pared izq
	Sphere(1e5,  Point(1e5 + 49, 0, 0),    Color(.25, .25, .75), Color()), // pared der
	Sphere(1e5,  Point(0, 0, -1e5 - 81.6), Color(.25, .75, .25), Color()), // pared detras
	Sphere(1e5,  Point(0, -1e5 - 40.8, 0), Color(.25, .75, .75), Color()), // suelo
	Sphere(1e5,  Point(0, 1e5 + 40.8, 0),  Color(.75, .75, .25), Color()), // techo
	Sphere(16.5, Point(-23, -24.3, -34.6), Color(.2, .3, .4), Color()),     // esfera abajo-izq
	Sphere(16.5, Point(23, -24.3, -3.6),   Color(.4, .3, .2), Color()),     // esfera abajo-der
	Sphere(10.5, Point(0, 24.3, 0),        Color(1, 1, 1), Color(10, 10, 10)) // esfera arriba (emisor, fase 1)
};

// Fuente puntual global para fase 02
PointLight point_light = {
	Point(0, 24.3, 0),
	Color(4000, 4000, 4000)
};

// Flag para usar iluminación puntual (mode point) o MC (modo fase 1)
bool use_point_light_mode = false;

// Parámetros del medio participante (fase 06: ray marching con single scattering)
bool use_fog_mode = false;
double sigma_a = 0.0;  // coeficiente de absorción
double sigma_s = 0.0;  // coeficiente de scattering
double sigma_t = 0.0;  // extinción total: σt = σa + σs

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

// PROYECTO 1 (actualizado para fase 02)
// calcular la intersección del rayo r con todas las esferas
// regresar true si hubo una intersección, falso de otro modo
// almacenar en t la distancia sobre el rayo en que sucede la interseccion
// almacenar en id el indice de spheres[] de la esfera cuya interseccion es mas cercana
// En modo point light (fase 02), ignora la esfera emisora (índice 7)
inline bool intersect(const Ray &r, double &t, int &id) {
	t = 1e20;
	bool found = false;
	for (int i = 0; i < 8; i++) {
		// En modo point light, saltar la esfera emisora
		if (use_point_light_mode && i == EMITTER_SPHERE_INDEX) continue;

		double hit = spheres[i].intersect(r);
		if (hit > 0.0 && hit < t) {
			t = hit;
			id = i;
			found = true;
		}
	}
	return found;
}

// Variables globales para el muestreador y spp
const char *sampler_name = "uniformsphere";
int spp = 32;

// Función auxiliar: retorna coseno del ángulo polar usando el muestreador especificado
// y el pdf correspondiente, dados dos uniformes (xi1, xi2) en [0, 1)
void sample_direction(const char *sampler, double xi1, double xi2,
                      Vector &local_dir, double &pdf) {
	double costheta, sintheta, phi;
	phi = 2.0 * M_PI * xi2;

	if (strcmp(sampler, "uniformsphere") == 0) {
		costheta = 1.0 - 2.0 * xi1;
		pdf = 1.0 / (4.0 * M_PI);
	} else if (strcmp(sampler, "uniformhemi") == 0) {
		costheta = xi1;
		pdf = 1.0 / (2.0 * M_PI);
	} else { // cosinehemi
		costheta = sqrt(1.0 - xi1);
		pdf = costheta / M_PI;
	}

	sintheta = sqrt(1.0 - costheta * costheta);
	local_dir = Vector(sintheta * cos(phi), sintheta * sin(phi), costheta);
}

// Construye una base ortonormal (s, t, n) donde n es la normal
// s = t × n, t es perpendicular a n
void build_orthonormal_basis(const Vector &n, Vector &s, Vector &t) {
	if (fabs(n.x) > fabs(n.y)) {
		// |n.x| > |n.y|: t = (n.z, 0, -n.x) / sqrt(n.x^2 + n.z^2)
		double norm = sqrt(n.x * n.x + n.z * n.z);
		t = Vector(n.z, 0.0, -n.x) * (1.0 / norm);
	} else {
		// else: t = (0, n.z, -n.y) / sqrt(n.y^2 + n.z^2)
		double norm = sqrt(n.y * n.y + n.z * n.z);
		t = Vector(0.0, n.z, -n.y) * (1.0 / norm);
	}
	s = t % n; // s = t × n
}

// Calcula el valor de color para el rayo dado (una muestra) — fase 01 (Monte Carlo)
Color shade(const Ray &r, unsigned short *rand_state) {
	double t;
	int id = 0;
	// determinar que esfera (id) y a que distancia (t) el rayo intersecta
	if (!intersect(r, t, id))
		return Color(); // el rayo no intersecto objeto, return Color() == negro

	const Sphere &obj = spheres[id];

	// determinar coordenadas del punto de interseccion
	Point x = r.o + r.d * t;

	// determinar la dirección normal en el punto de interseccion
	Vector n = (x - obj.p);
	n.normalize();

	// Si el objeto es un emisor (ke ≠ 0), devolver su emisión
	if (obj.ke.x > 0 || obj.ke.y > 0 || obj.ke.z > 0) {
		return obj.ke;
	}

	// Iluminación directa por Monte Carlo
	// Generar dirección uniforme
	double xi1 = erand48(rand_state);
	double xi2 = erand48(rand_state);

	Vector local_dir;
	double pdf;
	sample_direction(sampler_name, xi1, xi2, local_dir, pdf);

	// Convertir de local a global usando base ortonormal
	Vector s, t_basis;
	build_orthonormal_basis(n, s, t_basis);

	Vector wi = s * local_dir.x + t_basis * local_dir.y + n * local_dir.z;

	// Lanzar rayo de sombra desde x
	Ray shadow_ray(x + n * 1e-4, wi); // epsilon = 1e-4
	double t_shadow;
	int id_shadow;
	Color L = Color(); // acumulador

	if (intersect(shadow_ray, t_shadow, id_shadow)) {
		const Sphere &shadow_obj = spheres[id_shadow];
		// Si toca un emisor, usar su emisión
		if (shadow_obj.ke.x > 0 || shadow_obj.ke.y > 0 || shadow_obj.ke.z > 0) {
			// Le = shadow_obj.ke
			// fr = obj.c / π
			// cos(θ) = n · wi
			double costheta = n.dot(wi);
			if (costheta > 0) {
				Color fr = obj.c * (1.0 / M_PI);
				L = shadow_obj.ke.mult(fr) * (costheta / pdf);
			}
		}
	}

	return L;
}

// Calcula iluminación directa con fuente puntual determinista (fase 02)
// Entrada: rayo desde la cámara
// Salida: color con iluminación directa (hard shadows) de la fuente puntual
Color shade_point_light(const Ray &r) {
	double t;
	int id = 0;
	// Intersectar con la escena
	if (!intersect(r, t, id))
		return Color(); // no hay intersección

	const Sphere &obj = spheres[id];

	// Punto de intersección
	Point x = r.o + r.d * t;

	// Normal en el punto (hacia afuera de la esfera)
	Vector n = (x - obj.p);
	n.normalize();

	// Vector desde x hacia la fuente puntual
	Vector w = point_light.pos - x;
	double r_dist = sqrt(w.dot(w)); // distancia a la fuente
	w = w * (1.0 / r_dist); // normalizar

	// Coseno del ángulo entre normal y dirección a la fuente
	double costheta = n.dot(w);

	// Si el punto está "de espaldas" a la fuente, contribución = 0
	if (costheta <= 0.0)
		return Color();

	// Lanzar rayo de sombra desde x + n*epsilon en dirección w
	const double epsilon = 1e-4;
	Ray shadow_ray(x + n * epsilon, w);

	double t_shadow;
	int id_shadow;

	// Verificar si hay un objeto bloqueando
	if (intersect(shadow_ray, t_shadow, id_shadow)) {
		// Un objeto intersecta el rayo de sombra antes de llegar a la fuente
		if (t_shadow < r_dist - epsilon) {
			// Sombra dura
			return Color();
		}
	}

	// Visibilidad directa: aplicar fórmula L = (albedo/π) ⊙ I · cos(θ) / r²
	Color fr = obj.c * (1.0 / M_PI);
	Color L = fr.mult(point_light.intensity) * (costheta / (r_dist * r_dist));

	return L;
}

// Calcula iluminación con medio participante homogéneo: ray marching + single scattering (fase 06)
// Entrada: rayo desde la cámara, sigma_a, sigma_s
// Salida: color con transmitancia atenuada del hit + in-scattering del medio
Color shade_fog(const Ray &r) {
	// Caso especial: si σt = 0, el medio es transparente y no hay scattering.
	// Debe reproducir EXACTAMENTE el modo point (bit a bit).
	if (sigma_t == 0.0) {
		return shade_point_light(r);
	}

	double t;
	int id = 0;
	// Intersectar con la escena
	if (!intersect(r, t, id))
		return Color(); // no hay intersección

	const Sphere &obj = spheres[id];

	// Punto de intersección
	Point x = r.o + r.d * t;

	// Normal en el punto (hacia afuera de la esfera)
	Vector n = (x - obj.p);
	n.normalize();

	// Vector desde x hacia la fuente puntual
	Vector w = point_light.pos - x;
	double d_sombra = sqrt(w.dot(w)); // distancia a la fuente
	w = w * (1.0 / d_sombra); // normalizar

	// Coseno del ángulo entre normal y dirección a la fuente
	double costheta = n.dot(w);

	// Radiancia de superficie (sin atenuación aún)
	Color L_surface = Color();

	if (costheta > 0.0) {
		// Lanzar rayo de sombra desde x + n*epsilon en dirección w
		const double epsilon = 1e-4;
		Ray shadow_ray(x + n * epsilon, w);

		double t_shadow;
		int id_shadow;

		// Verificar si hay un objeto bloqueando la fuente puntual
		bool visible = true;
		if (intersect(shadow_ray, t_shadow, id_shadow)) {
			// Un objeto intersecta el rayo de sombra antes de llegar a la fuente
			if (t_shadow < d_sombra - epsilon) {
				visible = false;
			}
		}

		if (visible) {
			// Radiancia de superficie (con transmitancia del rayo de sombra)
			// fr = albedo / π
			Color fr = obj.c * (1.0 / M_PI);
			// L_surface = fr ⊙ I · cos(θ) / r²
			L_surface = fr.mult(point_light.intensity)
						* (costheta / (d_sombra * d_sombra));
		}
	}

	// Transmitancia desde la cámara hasta el hit Y desde el hit a la fuente
	double transmittance = exp(-sigma_t * t);
	double transmittance_shadow = exp(-sigma_t * d_sombra);
	Color L_attenuated = L_surface * transmittance * transmittance_shadow;

	// Ray marching: single scattering
	// Para cada punto de marcha a pasos t = Δ/2, 3Δ/2, ... < s
	const double delta = 2.0; // paso de marcha en unidades de escena
	Color L_scattering = Color();

	if (sigma_s > 0.0) {
		// Marchar sobre el rayo primario
		for (double t_march = delta / 2.0; t_march < t; t_march += delta) {
			// Punto de marcha sobre el rayo primario
			Point x_march = r.o + r.d * t_march;

			// Transmitancia desde la cámara hasta este punto
			double trans_to_march = exp(-sigma_t * t_march);

			// Vector desde el punto de marcha hacia la fuente
			Vector w_march = point_light.pos - x_march;
			double d_t = sqrt(w_march.dot(w_march)); // distancia a la fuente
			w_march = w_march * (1.0 / d_t); // normalizar

			// Verificar visibilidad del punto de marcha hacia la fuente
			const double epsilon = 1e-4;
			Ray shadow_ray_march(x_march + w_march * epsilon, w_march);

			double t_shadow_march;
			int id_shadow_march;

			bool visible_march = true;
			if (intersect(shadow_ray_march, t_shadow_march, id_shadow_march)) {
				if (t_shadow_march < d_t - epsilon) {
					visible_march = false;
				}
			}

			if (visible_march) {
				// Transmitancia del rayo de sombra (desde punto de marcha hacia la fuente)
				double trans_shadow_march = exp(-sigma_t * d_t);

				// Acumular in-scattering
				// L += exp(-σt·t) · σs · (1/(4π)) · I · exp(-σt·d_t) · Δ / d_t²
				// La función de fase isotrópica es 1/(4π); comentar dónde va Henyey-Greenstein
				double phase_func = 1.0 / (4.0 * M_PI);
				double contrib_scalar = trans_to_march * sigma_s * phase_func * trans_shadow_march * delta / (d_t * d_t);

				// TODO: Aquí entraría la función de fase Henyey-Greenstein
				// en lugar de la isotrópica, si quisiéramos modelar anisotropía en forward scattering.
				// El parámetro g ∈ [-1, 1] controlaría la tendencia del scattering hacia/contra
				// la dirección del rayo incidente.

				L_scattering = L_scattering + point_light.intensity * contrib_scalar;
			}
		}
	}

	// Radiancia final: L = Tr(s)·Lo_atenuada + L_medio
	Color L_final = L_attenuated + L_scattering;

	return L_final;
}


int main(int argc, char *argv[]) {
	int w = 1024, h = 768; // image resolution

	// Detectar modo: "point" para iluminación puntual, "fog" para ray marching, o MC (uniformsphere/uniformhemi/cosinehemi)
	bool use_point_light_render = false;
	const char *output_file = "image.ppm";

	// parsear argumentos de línea de comandos
	if (argc > 1) {
		if (strcmp(argv[1], "point") == 0) {
			use_point_light_render = true;
			use_point_light_mode = true; // Indica a intersect() que ignore la esfera emisora
			output_file = "image.ppm"; // output estándar para poder comparar con fog 0 0
		} else if (strcmp(argv[1], "fog") == 0) {
			// Modo ray marching con medio participante (fase 06)
			if (argc < 4) {
				fprintf(stderr, "Uso: ./rt fog <sigma_a> <sigma_s>\n");
				return 1;
			}
			use_fog_mode = true;
			use_point_light_mode = true; // También ignora la esfera emisora en intersect()
			sigma_a = atof(argv[2]);
			sigma_s = atof(argv[3]);
			sigma_t = sigma_a + sigma_s;
			output_file = "image.ppm"; // El mismo archivo base; check.sh lo renombra
		} else {
			sampler_name = argv[1];
		}
	}
	if (argc > 2 && !use_point_light_render && !use_fog_mode) {
		spp = atoi(argv[2]);
	}

	// fija la posicion de la camara y la dirección en que mira
	Ray camera( Point(0, 11.2, 214), Vector(0, -0.042612, -1).normalize() );

	// parametros de la camara
	Vector cx = Vector( w * 0.5095 / h, 0., 0.);
	Vector cy = (cx % camera.d).normalize() * 0.5095;

	// auxiliar para valor de pixel y matriz para almacenar la imagen
	Color *pixelColors = new Color[w * h];

	// usar openmp para paralelizar el ciclo: cada hilo computara un renglon
	#pragma omp parallel for schedule(dynamic)
	for(int y = 0; y < h; y++)
	{
		// recorre todos los pixeles de la imagen
		#pragma omp critical
		fprintf(stderr,"\r%5.2f%%",100.*y/(h-1));

		for(int x = 0; x < w; x++ ) {
			int idx = (h - y - 1) * w + x; // index en 1D para una imagen 2D
			Color accumulator = Color(); // acumulador para las N muestras

			// para el pixel actual, computar la dirección que un rayo debe tener
			Vector cameraRayDir = cx * ( double(x)/w - .5) + cy * ( double(y)/h - .5) + camera.d;
			cameraRayDir.normalize();

			if (use_point_light_render) {
				// Modo iluminación puntual: determinista, sin MC
				Color sample = shade_point_light( Ray(camera.o, cameraRayDir) );
				pixelColors[idx] = Color(clamp(sample.x), clamp(sample.y), clamp(sample.z));
			} else if (use_fog_mode) {
				// Modo ray marching con medio participante (fase 06)
				Color sample = shade_fog( Ray(camera.o, cameraRayDir) );
				pixelColors[idx] = Color(clamp(sample.x), clamp(sample.y), clamp(sample.z));
			} else {
				// Modo Monte Carlo (fase 01): con spp muestras
				// Inicializar estado RNG per-pixel de forma determinista
				unsigned short rand_state[3];
				unsigned int pixel_index = y * w + x;
				rand_state[0] = (pixel_index >> 16) & 0xFFFF;
				rand_state[1] = pixel_index & 0xFFFF;
				rand_state[2] = 5489 ^ pixel_index; // semilla fija + índice

				// Promediar spp muestras
				for (int s = 0; s < spp; s++) {
					Color sample = shade( Ray(camera.o, cameraRayDir), rand_state );
					accumulator = accumulator + sample;
				}

				// Promediar
				Color pixelValue = accumulator * (1.0 / spp);

				// limitar los tres valores de color del pixel a [0,1]
				pixelColors[idx] = Color(clamp(pixelValue.x), clamp(pixelValue.y), clamp(pixelValue.z));
			}
		}
	}

	fprintf(stderr,"\n");

	// Guardar imagen PPM
	FILE *f = fopen(output_file, "w");
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
