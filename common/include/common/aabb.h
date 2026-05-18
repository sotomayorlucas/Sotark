#ifndef SOTARK_COMMON_AABB_H
#define SOTARK_COMMON_AABB_H

#include "common/types.h"
#include "common/vec.h"

#include <math.h>

/* Axis-aligned bounding box. Convención inclusiva: un punto p está dentro
 * si min <= p <= max componente a componente. */
typedef struct {
    vec3_t min, max;
} aabb_t;

static inline aabb_t aabb_from_min_max(vec3_t mn, vec3_t mx) {
    return (aabb_t){ mn, mx };
}

static inline aabb_t aabb_translate(aabb_t b, vec3_t t) {
    return (aabb_t){ v3_add(b.min, t), v3_add(b.max, t) };
}

/* Bounding box que envuelve un array de puntos. */
static inline aabb_t aabb_from_points(const vec3_t *pts, int n) {
    aabb_t b = { pts[0], pts[0] };
    for (int i = 1; i < n; ++i) {
        b.min.x = fminf(b.min.x, pts[i].x);
        b.min.y = fminf(b.min.y, pts[i].y);
        b.min.z = fminf(b.min.z, pts[i].z);
        b.max.x = fmaxf(b.max.x, pts[i].x);
        b.max.y = fmaxf(b.max.y, pts[i].y);
        b.max.z = fmaxf(b.max.z, pts[i].z);
    }
    return b;
}

/* Union de dos AABBs (smallest box que contiene a ambos). */
static inline aabb_t aabb_union(aabb_t a, aabb_t b) {
    return (aabb_t){
        .min = { fminf(a.min.x, b.min.x), fminf(a.min.y, b.min.y), fminf(a.min.z, b.min.z) },
        .max = { fmaxf(a.max.x, b.max.x), fmaxf(a.max.y, b.max.y), fmaxf(a.max.z, b.max.z) },
    };
}

#endif
