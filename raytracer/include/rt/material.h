#ifndef SOTARK_RT_MATERIAL_H
#define SOTARK_RT_MATERIAL_H

#include "common/types.h"
#include "common/vec.h"
#include "common/rng.h"
#include "common/image.h"
#include "rt/ray.h"
#include "rt/hit.h"

/*
 * Tagged-union de materiales (mismo patrón que hittable_t).
 * Shirley §9-11 BRDFs básicos:
 *   - Lambertian:  scatter difuso isotrópico, atenuación = albedo.
 *   - Metal:       reflect perfecto + fuzz opcional (random offset).
 *   - Dielectric:  refract con Schlick approximation para Fresnel;
 *                   reflect bajo total internal reflection.
 */
typedef enum {
    MAT_LAMBERTIAN,
    MAT_METAL,
    MAT_DIELECTRIC,
    MAT_EMISSIVE,           /* M6: superficies que emiten luz y no dispersan */
    MAT_TEXTURED_LAMBERTIAN,/* M11: lambertian con albedo desde image texture */
} mat_kind_t;

typedef struct material {
    mat_kind_t kind;
    union {
        struct { vec3_t albedo; }            lambertian;
        struct { vec3_t albedo; f32 fuzz; }  metal;       /* fuzz in [0, 1] */
        struct { f32 ior; }                  dielectric;  /* índice de refracción */
        struct { vec3_t emit; }              emissive;    /* emission color, puede >1 */
        struct {
            const image_t *tex;
            f32 u_scale, v_scale;            /* multiplicadores para tile texturas */
        } textured_lambertian;
    } u;
} material_t;

/* True si el rayo se dispersa; out parameters quedan inicializados.
 * False si el rayo se absorbe completamente (e.g. emissive: no scatter, solo
 * aporta luz vía material_emitted). */
bool material_scatter(const material_t  *m,
                      ray_t              ray_in,
                      const hit_record_t *rec,
                      vec3_t             *attenuation_out,
                      ray_t              *scattered_out,
                      pcg32_t            *rng);

/* Luz emitida por el material (independiente del scatter). Para no-emissive
 * devuelve (0,0,0). Permite escenas iluminadas por superficies emisivas
 * — necesario para Cornell box donde la única luz viene del techo. */
vec3_t material_emitted(const material_t *m);

#endif
