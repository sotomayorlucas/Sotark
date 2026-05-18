#ifndef SOTARK_RT_BVH_H
#define SOTARK_RT_BVH_H

#include "common/types.h"
#include "common/vec.h"
#include "common/aabb.h"
#include "rt/ray.h"
#include "rt/hittable.h"

/*
 * Ray-vs-AABB slab test. Para cada eje, calcula los valores de t en los
 * dos slabs del cajón y los intersecta. Si los intervalos se solapan, el
 * rayo cruza la caja. Manejo correcto del signo de dir (un dir negativo
 * pone el slab "min" en el lado far).
 *
 * Devuelve true si hay intersección dentro de (t_min, t_max).
 */
static inline bool aabb_hit(aabb_t box, ray_t r, f32 t_min, f32 t_max) {
    {
        const f32 inv = 1.0f / r.dir.x;
        f32 t0 = (box.min.x - r.origin.x) * inv;
        f32 t1 = (box.max.x - r.origin.x) * inv;
        if (t0 > t1) { const f32 t = t0; t0 = t1; t1 = t; }
        if (t0 > t_min) t_min = t0;
        if (t1 < t_max) t_max = t1;
        if (t_max <= t_min) return false;
    }
    {
        const f32 inv = 1.0f / r.dir.y;
        f32 t0 = (box.min.y - r.origin.y) * inv;
        f32 t1 = (box.max.y - r.origin.y) * inv;
        if (t0 > t1) { const f32 t = t0; t0 = t1; t1 = t; }
        if (t0 > t_min) t_min = t0;
        if (t1 < t_max) t_max = t1;
        if (t_max <= t_min) return false;
    }
    {
        const f32 inv = 1.0f / r.dir.z;
        f32 t0 = (box.min.z - r.origin.z) * inv;
        f32 t1 = (box.max.z - r.origin.z) * inv;
        if (t0 > t1) { const f32 t = t0; t0 = t1; t1 = t; }
        if (t0 > t_min) t_min = t0;
        if (t1 < t_max) t_max = t1;
        if (t_max <= t_min) return false;
    }
    return true;
}

/*
 * Construye una BVH median-split recursiva.
 *   - Reordena `items` in-place (sort por centroide en el eje más largo
 *     de la bbox combinada de cada nivel).
 *   - Aloca nodos interiores desde `arena` (hasta `arena_cap`).
 *   - Devuelve el nodo raíz (puede ser un primitive si n == 1).
 *
 * Pre: cada item en `items` tiene `bbox` populado.
 *
 * Una BVH balanceada con n hojas tiene n-1 nodos interiores; en la
 * práctica con median-split el factor real es similar — reservar ≥ n
 * nodos en el arena es seguro.
 */
hittable_t *bvh_build(hittable_t **items, int n,
                      hittable_t *arena, int arena_cap, int *n_arena);

#endif
