#ifndef SOTARK_RT_HIT_H
#define SOTARK_RT_HIT_H

#include "common/types.h"
#include "common/vec.h"

typedef struct {
    vec3_t p;          /* point of intersection                                */
    vec3_t normal;     /* always points AGAINST the incident ray               */
    f32    t;
    bool   front_face; /* true if the ray hit the outside of the surface       */
    u32    mat;        /* index into the materials array                       */
    f32    u, v;       /* texture coords (∈ [0, 1] for sphere/quad; 0 for tri) */
} hit_record_t;

/* Decide which side of the surface the ray hit and orient the recorded normal
 * accordingly. `outward_normal` is the geometric outward normal of the shape;
 * it must be unit-length. */
static inline void hit_record_set_face(hit_record_t *rec,
                                       vec3_t ray_dir,
                                       vec3_t outward_normal) {
    rec->front_face = v3_dot(ray_dir, outward_normal) < 0.0f;
    rec->normal     = rec->front_face ? outward_normal : v3_neg(outward_normal);
}

#endif
