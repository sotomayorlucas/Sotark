#include "rt/bvh.h"
#include "common/log.h"

#include <stdlib.h>

/* qsort no toma un parámetro extra, así que pasamos el eje vía global.
 * Estamos single-threaded, sin contención. */
static int g_sort_axis = 0;

static int compare_by_centroid(const void *pa, const void *pb) {
    const hittable_t *a = *(const hittable_t * const *)pa;
    const hittable_t *b = *(const hittable_t * const *)pb;
    f32 ca = 0.0f, cb = 0.0f;
    switch (g_sort_axis) {
        case 0:  ca = a->bbox.min.x + a->bbox.max.x;
                 cb = b->bbox.min.x + b->bbox.max.x; break;
        case 1:  ca = a->bbox.min.y + a->bbox.max.y;
                 cb = b->bbox.min.y + b->bbox.max.y; break;
        default: ca = a->bbox.min.z + a->bbox.max.z;
                 cb = b->bbox.min.z + b->bbox.max.z; break;
    }
    return (ca < cb) ? -1 : (ca > cb ? 1 : 0);
}

static int longest_axis(aabb_t box) {
    const f32 dx = box.max.x - box.min.x;
    const f32 dy = box.max.y - box.min.y;
    const f32 dz = box.max.z - box.min.z;
    if (dx >= dy && dx >= dz) return 0;
    if (dy >= dz)              return 1;
    return 2;
}

hittable_t *bvh_build(hittable_t **items, int n,
                       hittable_t *arena, int arena_cap, int *n_arena) {
    if (n <= 0) PANIC("bvh_build: empty items");
    if (n == 1) return items[0];

    /* Combined bbox para elegir eje de partición. */
    aabb_t combined = items[0]->bbox;
    for (int i = 1; i < n; ++i) combined = aabb_union(combined, items[i]->bbox);

    g_sort_axis = longest_axis(combined);
    qsort(items, (size_t)n, sizeof(items[0]), compare_by_centroid);

    const int mid = n / 2;
    hittable_t *left  = bvh_build(items,       mid,     arena, arena_cap, n_arena);
    hittable_t *right = bvh_build(items + mid, n - mid, arena, arena_cap, n_arena);

    if (*n_arena >= arena_cap) PANIC("bvh_build: arena exhausted (need >%d nodes)", arena_cap);
    hittable_t *node = &arena[(*n_arena)++];
    *node = (hittable_t){
        .kind  = HIT_BVH,
        .bbox  = aabb_union(left->bbox, right->bbox),
        .u.bvh = { left, right },
    };
    return node;
}
