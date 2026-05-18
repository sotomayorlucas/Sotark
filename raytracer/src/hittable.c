#include "rt/hittable.h"
#include "rt/bvh.h"

#include <math.h>

static bool hit_sphere(const hittable_t *h, ray_t r, f32 t_min, f32 t_max, hit_record_t *rec) {
    vec3_t oc   = v3_sub(r.origin, h->u.sphere.center);
    f32    a    = v3_dot(r.dir, r.dir);
    f32    hf   = v3_dot(oc, r.dir);
    f32    c    = v3_dot(oc, oc) - h->u.sphere.radius * h->u.sphere.radius;
    f32    disc = hf * hf - a * c;
    if (disc < 0.0f) return false;

    f32 sqrt_disc = sqrtf(disc);
    f32 root      = (-hf - sqrt_disc) / a;
    if (root <= t_min || root >= t_max) {
        root = (-hf + sqrt_disc) / a;
        if (root <= t_min || root >= t_max) return false;
    }

    rec->t   = root;
    rec->p   = ray_at(r, root);
    rec->mat = h->u.sphere.mat;
    vec3_t outward = v3_scale(v3_sub(rec->p, h->u.sphere.center), 1.0f / h->u.sphere.radius);
    hit_record_set_face(rec, r.dir, outward);
    /* UV mapping esférico (Shirley book 2 §6):
     *   theta ∈ [0, π]  desde +y al -y
     *   phi   ∈ [0, 2π] alrededor del eje y, midiendo desde -x */
    const f32 theta = acosf(-outward.y);
    const f32 phi   = atan2f(-outward.z, outward.x) + 3.14159265358979f;
    rec->u = phi   / (2.0f * 3.14159265358979f);
    rec->v = theta / 3.14159265358979f;
    return true;
}

static bool hit_list(const hittable_t *h, ray_t r, f32 t_min, f32 t_max, hit_record_t *rec) {
    hit_record_t temp;
    bool         any     = false;
    f32          closest = t_max;
    for (int i = 0; i < h->u.list.count; ++i) {
        const hittable_t *item = h->u.list.items[i];
        if (hit_hittable(item, r, t_min, closest, &temp)) {
            any     = true;
            closest = temp.t;
            *rec    = temp;
        }
    }
    return any;
}

/* Möller-Trumbore triangle intersection. Una de las técnicas más usadas para
 * meshes — sin precomputar normales, sin storage extra por triangle. */
static bool hit_triangle(const hittable_t *h, ray_t r, f32 t_min, f32 t_max, hit_record_t *rec) {
    const vec3_t v0 = h->u.tri.v0;
    const vec3_t v1 = h->u.tri.v1;
    const vec3_t v2 = h->u.tri.v2;
    const vec3_t e1 = v3_sub(v1, v0);
    const vec3_t e2 = v3_sub(v2, v0);
    const vec3_t pvec = v3_cross(r.dir, e2);
    const f32    det  = v3_dot(e1, pvec);
    if (fabsf(det) < 1e-8f) return false;
    const f32    inv_det = 1.0f / det;

    const vec3_t tvec = v3_sub(r.origin, v0);
    const f32    u    = inv_det * v3_dot(tvec, pvec);
    if (u < 0.0f || u > 1.0f) return false;

    const vec3_t qvec = v3_cross(tvec, e1);
    const f32    v    = inv_det * v3_dot(r.dir, qvec);
    if (v < 0.0f || (u + v) > 1.0f) return false;

    const f32 t = inv_det * v3_dot(e2, qvec);
    if (t <= t_min || t >= t_max) return false;

    rec->t   = t;
    rec->p   = ray_at(r, t);
    rec->mat = h->u.tri.mat;
    const vec3_t outward = v3_normalize(v3_cross(e1, e2));
    hit_record_set_face(rec, r.dir, outward);
    return true;
}

static bool hit_quad(const hittable_t *h, ray_t r, f32 t_min, f32 t_max, hit_record_t *rec) {
    /* 1) Intersección rayo-plano. */
    const f32 denom = v3_dot(h->u.quad.normal, r.dir);
    if (fabsf(denom) < 1e-8f) return false;   /* casi paralelo al plano */
    const f32 t = (h->u.quad.D - v3_dot(h->u.quad.normal, r.origin)) / denom;
    if (t <= t_min || t >= t_max) return false;

    /* 2) ¿El punto cae dentro del paralelogramo?
     * P - Q = α*u + β*v, se despeja con productos cruz:
     *   α = w · (hit_vec × v)
     *   β = w · (u × hit_vec)
     * donde w = n / |n|² (precomputado). */
    const vec3_t P       = ray_at(r, t);
    const vec3_t hit_vec = v3_sub(P, h->u.quad.Q);
    const f32    alpha   = v3_dot(h->u.quad.w, v3_cross(hit_vec, h->u.quad.v));
    const f32    beta    = v3_dot(h->u.quad.w, v3_cross(h->u.quad.u, hit_vec));
    if (alpha < 0.0f || alpha > 1.0f || beta < 0.0f || beta > 1.0f) return false;

    rec->t   = t;
    rec->p   = P;
    rec->mat = h->u.quad.mat;
    rec->u   = alpha;
    rec->v   = beta;
    hit_record_set_face(rec, r.dir, h->u.quad.normal);
    return true;
}

static bool hit_bvh(const hittable_t *h, ray_t r, f32 t_min, f32 t_max, hit_record_t *rec) {
    /* Early-out: si el rayo no cruza la bbox del nodo, ningún hijo intersecta. */
    if (!aabb_hit(h->bbox, r, t_min, t_max)) return false;

    hit_record_t left_rec;
    const bool hit_left = hit_hittable(h->u.bvh.left, r, t_min, t_max, &left_rec);

    /* El right child solo necesita encontrar hits MÁS cercanos que el left. */
    const f32 closest = hit_left ? left_rec.t : t_max;
    hit_record_t right_rec;
    const bool hit_right = hit_hittable(h->u.bvh.right, r, t_min, closest, &right_rec);

    if (hit_right) { *rec = right_rec; return true; }
    if (hit_left)  { *rec = left_rec;  return true; }
    return false;
}

bool hit_hittable(const hittable_t *h, ray_t r, f32 t_min, f32 t_max, hit_record_t *rec) {
    switch (h->kind) {
        case HIT_SPHERE: return hit_sphere  (h, r, t_min, t_max, rec);
        case HIT_LIST:   return hit_list    (h, r, t_min, t_max, rec);
        case HIT_BVH:    return hit_bvh     (h, r, t_min, t_max, rec);
        case HIT_QUAD:   return hit_quad    (h, r, t_min, t_max, rec);
        case HIT_TRI:    return hit_triangle(h, r, t_min, t_max, rec);
    }
    return false;
}
