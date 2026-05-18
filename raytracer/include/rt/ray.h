#ifndef SOTARK_RT_RAY_H
#define SOTARK_RT_RAY_H

#include "common/types.h"
#include "common/vec.h"

typedef struct {
    vec3_t origin;
    vec3_t dir;
} ray_t;

/* P(t) = O + t*D */
static inline vec3_t ray_at(ray_t r, f32 t) {
    return v3_add(r.origin, v3_scale(r.dir, t));
}

#endif
