#ifndef SOTARK_COMMON_PLANE_H
#define SOTARK_COMMON_PLANE_H

#include "common/types.h"
#include "common/vec.h"

/* Plano en 3D: n · p + d = 0 (forma "hessian normal").
 * normal debe ser unitario para que `d` sea la distancia signed al origen. */
typedef struct {
    vec3_t normal;
    f32    d;
} plane_t;

/* Construye un plano que pasa por `point` con normal `n` (asumimos n unitario). */
static inline plane_t plane_from_point_normal(vec3_t point, vec3_t n) {
    return (plane_t){ n, -v3_dot(n, point) };
}

/* Distancia signed del punto al plano. + = lado del normal, - = el otro. */
static inline f32 plane_distance(plane_t pl, vec3_t pt) {
    return v3_dot(pl.normal, pt) + pl.d;
}

/* +1 = front (lado del normal), -1 = back, 0 = sobre el plano (epsilon). */
static inline int plane_classify(plane_t pl, vec3_t pt) {
    f32 d = plane_distance(pl, pt);
    if (d >  1e-4f) return  1;
    if (d < -1e-4f) return -1;
    return 0;
}

#endif
