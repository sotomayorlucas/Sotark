#include "rt/material.h"
#include "rt/sampling.h"

#include <math.h>

static bool scatter_lambertian(const material_t *m, ray_t in, const hit_record_t *rec,
                                vec3_t *att, ray_t *out, pcg32_t *rng) {
    (void)in;
    /* normal + random_unit es la versión "true Lambertian" de Shirley §9.5:
     * la distribución resultante es cos(θ) en lugar de uniforme, que es la
     * BRDF físicamente correcta para una superficie difusa ideal. */
    vec3_t dir = v3_add(rec->normal, rng_unit_vec3(rng));
    if (v3_near_zero(dir)) dir = rec->normal;          /* degenerate guard */
    *att = m->u.lambertian.albedo;
    *out = (ray_t){ rec->p, dir };
    return true;
}

static bool scatter_textured_lambertian(const material_t *m, ray_t in, const hit_record_t *rec,
                                          vec3_t *att, ray_t *out, pcg32_t *rng) {
    (void)in;
    vec3_t dir = v3_add(rec->normal, rng_unit_vec3(rng));
    if (v3_near_zero(dir)) dir = rec->normal;
    *att = image_sample_bilinear(m->u.textured_lambertian.tex,
                                  rec->u * m->u.textured_lambertian.u_scale,
                                  rec->v * m->u.textured_lambertian.v_scale);
    *out = (ray_t){ rec->p, dir };
    return true;
}

static bool scatter_metal(const material_t *m, ray_t in, const hit_record_t *rec,
                           vec3_t *att, ray_t *out, pcg32_t *rng) {
    vec3_t unit       = v3_normalize(in.dir);
    vec3_t reflected  = v3_reflect(unit, rec->normal);
    /* Fuzz: ofrece especular borroso, agregando un offset aleatorio en una
     * mini-esfera. fuzz = 0 es espejo perfecto. */
    vec3_t scatter_dir = v3_add(reflected,
                                 v3_scale(rng_in_unit_sphere(rng), m->u.metal.fuzz));
    *att = m->u.metal.albedo;
    *out = (ray_t){ rec->p, scatter_dir };
    /* Si el fuzz mete el rayo bajo la superficie, lo absorbemos. */
    return v3_dot(scatter_dir, rec->normal) > 0.0f;
}

/* Aproximación de Schlick para Fresnel, cap. 11 de Shirley.
 * r0 = ((1-η)/(1+η))^2 ; F(θ) ≈ r0 + (1-r0)·(1-cosθ)^5 */
static f32 schlick_reflectance(f32 cosine, f32 ref_idx) {
    f32 r0 = (1.0f - ref_idx) / (1.0f + ref_idx);
    r0 = r0 * r0;
    return r0 + (1.0f - r0) * powf(1.0f - cosine, 5.0f);
}

static bool scatter_dielectric(const material_t *m, ray_t in, const hit_record_t *rec,
                                vec3_t *att, ray_t *out, pcg32_t *rng) {
    *att = (vec3_t){ 1.0f, 1.0f, 1.0f };

    /* Si entramos al material: η = 1/ior (saliendo del aire).
     * Si salimos:                η = ior. */
    f32    ri      = rec->front_face ? (1.0f / m->u.dielectric.ior)
                                     :  m->u.dielectric.ior;
    vec3_t unit    = v3_normalize(in.dir);
    f32    cos_t   = v3_dot(v3_neg(unit), rec->normal);
    if (cos_t > 1.0f) cos_t = 1.0f;
    f32    sin_t   = sqrtf(1.0f - cos_t * cos_t);

    /* Total internal reflection cuando sin_t' = ri·sin_t > 1. */
    bool   tir     = (ri * sin_t) > 1.0f;
    bool   reflect = tir || (schlick_reflectance(cos_t, ri) > pcg32_float01(rng));

    vec3_t dir = reflect ? v3_reflect(unit, rec->normal)
                         : v3_refract(unit, rec->normal, ri);
    *out = (ray_t){ rec->p, dir };
    return true;
}

bool material_scatter(const material_t *m, ray_t in, const hit_record_t *rec,
                       vec3_t *att, ray_t *out, pcg32_t *rng) {
    switch (m->kind) {
        case MAT_LAMBERTIAN:          return scatter_lambertian          (m, in, rec, att, out, rng);
        case MAT_METAL:               return scatter_metal               (m, in, rec, att, out, rng);
        case MAT_DIELECTRIC:          return scatter_dielectric          (m, in, rec, att, out, rng);
        case MAT_EMISSIVE:            return false;   /* emissive no dispersa */
        case MAT_TEXTURED_LAMBERTIAN: return scatter_textured_lambertian(m, in, rec, att, out, rng);
    }
    return false;
}

vec3_t material_emitted(const material_t *m) {
    if (m->kind == MAT_EMISSIVE) return m->u.emissive.emit;
    return (vec3_t){ 0.0f, 0.0f, 0.0f };
}
