#ifndef SOTARK_SW_FRUSTUM_H
#define SOTARK_SW_FRUSTUM_H

#include "common/types.h"
#include "common/vec.h"
#include "common/mat.h"
#include "common/aabb.h"

/*
 * 6 planos del frustum en world space, extraídos de la matriz P*V
 * (proyección × view, SIN model). Cada plano (a, b, c, d) tiene la
 * convención: ax + by + cz + d >= 0 = dentro del frustum.
 *
 * Orden: left, right, bottom, top, near, far.
 */
typedef struct {
    f32 planes[6][4];
} frustum_t;

/* Extrae los 6 planos de la matriz pv (proj * view en world coords). */
void frustum_from_pv(frustum_t *f, mat4_t pv);

/* Devuelve true si la AABB está COMPLETAMENTE fuera del frustum y podemos
 * descartarla (test de "positive vertex" para cada plano). Si devuelve
 * false, la AABB puede estar dentro o cruzar — la dibujamos. */
bool frustum_cull_aabb(const frustum_t *f, aabb_t box);

#endif
