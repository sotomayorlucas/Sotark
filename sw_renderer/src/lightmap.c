#include "lightmap.h"

#include <math.h>

u32 lightmap_sample_bilinear(const lightmap_t *lm, f32 u, f32 v) {
    /* Normalize (u, v) a coords del texel grid. */
    f32 fu = (u - lm->u_min) / (lm->u_max - lm->u_min) * (f32)(lm->w - 1);
    f32 fv = (v - lm->v_min) / (lm->v_max - lm->v_min) * (f32)(lm->h - 1);
    /* Clamp dentro del rango muestreable. */
    if (fu < 0.0f) fu = 0.0f;
    if (fv < 0.0f) fv = 0.0f;
    const f32 max_u = (f32)(lm->w - 1) - 1e-4f;
    const f32 max_v = (f32)(lm->h - 1) - 1e-4f;
    if (fu > max_u) fu = max_u;
    if (fv > max_v) fv = max_v;

    const int iu = (int)floorf(fu);
    const int iv = (int)floorf(fv);
    const f32 fx = fu - (f32)iu;
    const f32 fy = fv - (f32)iv;

    const u32 c00 = lm->texels[iv      * lm->w + iu    ];
    const u32 c10 = lm->texels[iv      * lm->w + iu + 1];
    const u32 c01 = lm->texels[(iv + 1)* lm->w + iu    ];
    const u32 c11 = lm->texels[(iv + 1)* lm->w + iu + 1];

    const f32 w00 = (1.0f - fx) * (1.0f - fy);
    const f32 w10 = fx          * (1.0f - fy);
    const f32 w01 = (1.0f - fx) * fy;
    const f32 w11 = fx          * fy;

#define MIX(shift) (u32)((f32)((c00 >> (shift)) & 0xFFu) * w00 + \
                          (f32)((c10 >> (shift)) & 0xFFu) * w10 + \
                          (f32)((c01 >> (shift)) & 0xFFu) * w01 + \
                          (f32)((c11 >> (shift)) & 0xFFu) * w11)
    const u32 r = MIX(16);
    const u32 g = MIX( 8);
    const u32 b = MIX( 0);
#undef MIX
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

/* Bilinear interp world pos: P(u, v) en [0, 1]² → punto en el quad. */
static vec3_t quad_interp(const vec3_t verts[4], f32 u, f32 v) {
    const vec3_t bottom = v3_add(v3_scale(verts[0], 1.0f - u),
                                  v3_scale(verts[1], u));
    const vec3_t top    = v3_add(v3_scale(verts[3], 1.0f - u),
                                  v3_scale(verts[2], u));
    return v3_add(v3_scale(bottom, 1.0f - v), v3_scale(top, v));
}

/* Test ray-vs-quad. true si el rayo (origin + t·dir) cruza la face dentro
 * de t ∈ (0.001, t_max). */
static bool quad_blocks(const bsp_face_t *face,
                         vec3_t origin, vec3_t dir, f32 t_max) {
    const vec3_t e1 = v3_sub(face->verts[1], face->verts[0]);
    const vec3_t e2 = v3_sub(face->verts[3], face->verts[0]);
    const vec3_t n  = v3_cross(e1, e2);
    const f32    denom = v3_dot(n, dir);
    if (fabsf(denom) < 1e-8f) return false;
    const f32 D = v3_dot(n, face->verts[0]);
    const f32 t = (D - v3_dot(n, origin)) / denom;
    if (t <= 0.001f || t >= t_max) return false;

    const vec3_t P = v3_add(origin, v3_scale(dir, t));
    const vec3_t local = v3_sub(P, face->verts[0]);
    const f32 a = v3_dot(local, e1) / v3_dot(e1, e1);
    const f32 b = v3_dot(local, e2) / v3_dot(e2, e2);
    return (a >= 0.0f && a <= 1.0f && b >= 0.0f && b <= 1.0f);
}

void lightmap_bake(lightmap_t *lm,
                   const bsp_face_t *face,
                   const bsp_face_t *all_faces, int n_faces, int skip_idx,
                   vec3_t light_pos, f32 light_intensity, f32 ambient) {
    /* Normal del quad (no necesita ser unitaria para dot test, pero lo
     * normalizamos para el cosine). */
    const vec3_t e1 = v3_sub(face->verts[1], face->verts[0]);
    const vec3_t e2 = v3_sub(face->verts[3], face->verts[0]);
    const vec3_t normal = v3_normalize(v3_cross(e1, e2));

    /* Compute uv range del face. */
    f32 u_min = face->uvs[0].x, u_max = face->uvs[0].x;
    f32 v_min = face->uvs[0].y, v_max = face->uvs[0].y;
    for (int i = 1; i < 4; ++i) {
        if (face->uvs[i].x < u_min) u_min = face->uvs[i].x;
        if (face->uvs[i].y < v_min) v_min = face->uvs[i].y;
        if (face->uvs[i].x > u_max) u_max = face->uvs[i].x;
        if (face->uvs[i].y > v_max) v_max = face->uvs[i].y;
    }
    lm->u_min = u_min; lm->u_max = u_max;
    lm->v_min = v_min; lm->v_max = v_max;

    for (int j = 0; j < lm->h; ++j) {
        for (int i = 0; i < lm->w; ++i) {
            /* Posición parametrica (0,0)..(1,1) y world pos correspondiente. */
            const f32 pu = ((f32)i + 0.5f) / (f32)lm->w;
            const f32 pv = ((f32)j + 0.5f) / (f32)lm->h;
            const vec3_t P = quad_interp(face->verts, pu, pv);

            /* Offset levemente hacia el normal para evitar self-shadow. */
            const vec3_t P_off = v3_add(P, v3_scale(normal, 0.001f));
            const vec3_t to_L  = v3_sub(light_pos, P_off);
            const f32    dist_sq = v3_length_sq(to_L);
            const f32    dist    = sqrtf(dist_sq);
            const vec3_t L_dir   = v3_scale(to_L, 1.0f / dist);
            const f32    cos_theta = v3_dot(normal, L_dir);

            f32 lit = ambient;
            if (cos_theta > 0.0f) {
                /* Shadow test contra todas las otras faces (direction NO
                 * normalizada → t = 1 corresponde al light). */
                bool blocked = false;
                for (int k = 0; k < n_faces; ++k) {
                    if (k == skip_idx) continue;
                    if (quad_blocks(&all_faces[k], P_off, to_L, 1.0f)) {
                        blocked = true;
                        break;
                    }
                }
                if (!blocked) {
                    lit += light_intensity * cos_theta / dist_sq;
                }
            }
            if (lit > 1.0f) lit = 1.0f;
            if (lit < 0.0f) lit = 0.0f;
            const u8 b = (u8)(lit * 255.0f);
            lm->texels[j * lm->w + i] =
                0xFF000000u | ((u32)b << 16) | ((u32)b << 8) | (u32)b;
        }
    }
}
