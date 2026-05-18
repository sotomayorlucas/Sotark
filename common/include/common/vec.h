#ifndef SOTARK_COMMON_VEC_H
#define SOTARK_COMMON_VEC_H

#include "common/types.h"
#include <math.h>

typedef struct { f32 x, y; }       vec2_t;
typedef struct { f32 x, y, z; }    vec3_t;
typedef struct { f32 x, y, z, w; } vec4_t;

static inline vec3_t vec3(f32 x, f32 y, f32 z)    { return (vec3_t){x, y, z}; }
static inline vec3_t v3_add(vec3_t a, vec3_t b)   { return vec3(a.x+b.x, a.y+b.y, a.z+b.z); }
static inline vec3_t v3_sub(vec3_t a, vec3_t b)   { return vec3(a.x-b.x, a.y-b.y, a.z-b.z); }
static inline vec3_t v3_mul(vec3_t a, vec3_t b)   { return vec3(a.x*b.x, a.y*b.y, a.z*b.z); }
static inline vec3_t v3_scale(vec3_t a, f32 s)    { return vec3(a.x*s, a.y*s, a.z*s); }
static inline vec3_t v3_neg(vec3_t a)             { return vec3(-a.x, -a.y, -a.z); }
static inline f32    v3_dot(vec3_t a, vec3_t b)   { return a.x*b.x + a.y*b.y + a.z*b.z; }
static inline vec3_t v3_cross(vec3_t a, vec3_t b) { return vec3(a.y*b.z - a.z*b.y,
                                                                a.z*b.x - a.x*b.z,
                                                                a.x*b.y - a.y*b.x); }
static inline f32    v3_length_sq(vec3_t a)       { return v3_dot(a, a); }
static inline f32    v3_length(vec3_t a)          { return sqrtf(v3_length_sq(a)); }

static inline vec3_t v3_normalize(vec3_t a) {
    f32 inv = 1.0f / v3_length(a);
    return v3_scale(a, inv);
}

static inline vec3_t v3_lerp(vec3_t a, vec3_t b, f32 t) {
    return v3_add(v3_scale(a, 1.0f - t), v3_scale(b, t));
}

/* Mirror reflection: v - 2(v·n)n. v can be any length; n must be unit. */
static inline vec3_t v3_reflect(vec3_t v, vec3_t n) {
    return v3_sub(v, v3_scale(n, 2.0f * v3_dot(v, n)));
}

/* Snell refraction. uv must be unit length. eta_ratio = n_in / n_out.
 * Assumes the caller has already verified the ray actually refracts
 * (not total internal reflection). */
static inline vec3_t v3_refract(vec3_t uv, vec3_t n, f32 eta_ratio) {
    f32 cos_theta = v3_dot(v3_neg(uv), n);
    if (cos_theta > 1.0f) cos_theta = 1.0f;
    vec3_t r_perp        = v3_scale(v3_add(uv, v3_scale(n, cos_theta)), eta_ratio);
    f32    parallel_lsq  = 1.0f - v3_length_sq(r_perp);
    if (parallel_lsq < 0.0f) parallel_lsq = 0.0f;
    vec3_t r_parallel    = v3_scale(n, -sqrtf(parallel_lsq));
    return v3_add(r_perp, r_parallel);
}

/* True if all components have magnitude under eps — used to catch the
 * degenerate Lambertian scatter where rand_unit cancels the surface normal. */
static inline bool v3_near_zero(vec3_t v) {
    const f32 eps = 1e-8f;
    return (fabsf(v.x) < eps) && (fabsf(v.y) < eps) && (fabsf(v.z) < eps);
}

#endif
