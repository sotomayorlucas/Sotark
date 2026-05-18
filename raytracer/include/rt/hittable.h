#ifndef SOTARK_RT_HITTABLE_H
#define SOTARK_RT_HITTABLE_H

#include "common/types.h"
#include "common/vec.h"
#include "common/aabb.h"
#include "rt/ray.h"
#include "rt/hit.h"

/* Tagged-union dispatch over hittable types.
 *   M2: SPHERE, LIST
 *   M5: + BVH (interior node con 2 hijos)
 *   Futuro: QUAD, TRI */
typedef enum {
    HIT_SPHERE,
    HIT_LIST,
    HIT_BVH,
    HIT_QUAD,    /* M6: paralelogramo definido por origen Q + 2 edges (u, v) */
    HIT_TRI,     /* M8: triángulo con 3 vértices, Möller-Trumbore intersection */
} hit_kind_t;

typedef struct hittable {
    hit_kind_t kind;
    aabb_t     bbox;       /* precomputed bounding box (HIT_LIST puede dejarlo cero) */
    union {
        struct {
            vec3_t center;
            f32    radius;
            u32    mat;
        } sphere;
        struct {
            struct hittable * const *items;
            int count;
        } list;
        struct {
            struct hittable *left;
            struct hittable *right;
        } bvh;
        struct {
            vec3_t Q;        /* origen del quad */
            vec3_t u, v;     /* dos edges adyacentes */
            vec3_t normal;   /* unit normal (= normalize(u × v)) */
            f32    D;        /* plane equation: normal·p = D */
            vec3_t w;        /* = (u × v) / |u × v|², usado para inside-test */
            u32    mat;
        } quad;
        struct {
            vec3_t v0, v1, v2;
            u32    mat;
        } tri;
    } u;
} hittable_t;

/* Constructor helper — popula la bbox automáticamente para spheres. */
static inline hittable_t hittable_sphere(vec3_t center, f32 radius, u32 mat) {
    return (hittable_t){
        .kind = HIT_SPHERE,
        .bbox = {
            .min = { center.x - radius, center.y - radius, center.z - radius },
            .max = { center.x + radius, center.y + radius, center.z + radius },
        },
        .u.sphere = { center, radius, mat },
    };
}

#include <math.h>

/* Constructor helper para quads. Computa normal, plane D, w, y bbox. La
 * bbox de un quad axis-aligned es degenerada (zero-thickness) en un eje,
 * lo que rompe el ray-vs-AABB. Padding mínimo para evitarlo. */
static inline hittable_t hittable_quad(vec3_t Q, vec3_t u_edge, vec3_t v_edge, u32 mat) {
    vec3_t n     = v3_cross(u_edge, v_edge);
    f32    n_lsq = v3_length_sq(n);
    vec3_t norm  = v3_scale(n, 1.0f / sqrtf(n_lsq));
    f32    D     = v3_dot(norm, Q);
    vec3_t w     = v3_scale(n, 1.0f / n_lsq);

    vec3_t corners[4] = {
        Q,
        v3_add(Q, u_edge),
        v3_add(Q, v_edge),
        v3_add(v3_add(Q, u_edge), v_edge),
    };
    aabb_t box = aabb_from_points(corners, 4);
    const f32 eps = 1e-4f;
    if (box.max.x - box.min.x < eps) { box.min.x -= eps; box.max.x += eps; }
    if (box.max.y - box.min.y < eps) { box.min.y -= eps; box.max.y += eps; }
    if (box.max.z - box.min.z < eps) { box.min.z -= eps; box.max.z += eps; }

    return (hittable_t){
        .kind = HIT_QUAD,
        .bbox = box,
        .u.quad = { Q, u_edge, v_edge, norm, D, w, mat },
    };
}

/* Constructor helper para triángulos. Pad mínimo en ejes degenerados. */
static inline hittable_t hittable_triangle(vec3_t v0, vec3_t v1, vec3_t v2, u32 mat) {
    vec3_t corners[3] = { v0, v1, v2 };
    aabb_t box = aabb_from_points(corners, 3);
    const f32 eps = 1e-4f;
    if (box.max.x - box.min.x < eps) { box.min.x -= eps; box.max.x += eps; }
    if (box.max.y - box.min.y < eps) { box.min.y -= eps; box.max.y += eps; }
    if (box.max.z - box.min.z < eps) { box.min.z -= eps; box.max.z += eps; }
    return (hittable_t){
        .kind = HIT_TRI,
        .bbox = box,
        .u.tri = { v0, v1, v2, mat },
    };
}

/* Returns true if the ray hits the hittable within (t_min, t_max). On hit,
 * rec is fully populated with the closest intersection. */
bool hit_hittable(const hittable_t *h, ray_t r, f32 t_min, f32 t_max, hit_record_t *rec);

#endif
