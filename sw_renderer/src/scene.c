#include "scene.h"

#include <string.h>

void scene_init(scene_t *s) {
    s->n_faces = 0;
    for (int i = 0; i < SCENE_MAX_FACES; ++i) {
        s->lightmaps[i] = (lightmap_t){
            .texels = s->lm_storage[i],
            .w = SCENE_LM_DIM,
            .h = SCENE_LM_DIM,
        };
    }
}

int scene_add_face(scene_t *s, bsp_face_t face) {
    if (s->n_faces >= SCENE_MAX_FACES) return -1;
    s->faces[s->n_faces] = face;
    return s->n_faces++;
}

void scene_remove_face(scene_t *s, int idx) {
    if (idx < 0 || idx >= s->n_faces) return;
    const int last = s->n_faces - 1;
    if (idx != last) {
        /* Compact: move la última face al hueco. Las texels de la lightmap
         * van con su slot fijo, así que copiamos data del último slot al
         * slot `idx`. */
        s->faces[idx] = s->faces[last];
        memcpy(s->lm_storage[idx], s->lm_storage[last],
               SCENE_LM_DIM * SCENE_LM_DIM * sizeof(u32));
        /* Las uv ranges del lightmap también se copian. */
        s->lightmaps[idx].u_min = s->lightmaps[last].u_min;
        s->lightmaps[idx].u_max = s->lightmaps[last].u_max;
        s->lightmaps[idx].v_min = s->lightmaps[last].v_min;
        s->lightmaps[idx].v_max = s->lightmaps[last].v_max;
    }
    s->n_faces--;
}

/* ─── Primitive builders ────────────────────────────────────────────── */

int scene_add_floor(scene_t *s, vec3_t c, f32 size, tex_sample_fn tex) {
    const f32 h = size * 0.5f;
    return scene_add_face(s, (bsp_face_t){
        .verts = {
            { c.x - h, c.y, c.z + h },
            { c.x + h, c.y, c.z + h },
            { c.x + h, c.y, c.z - h },
            { c.x - h, c.y, c.z - h },
        },
        .uvs   = {
            { -h,  h }, {  h,  h }, {  h, -h }, { -h, -h },
        },
        .tex = tex,
    });
}

int scene_add_cube(scene_t *s, vec3_t c, f32 size, tex_sample_fn tex) {
    const f32 h = size * 0.5f;
    const vec3_t mn = { c.x - h, c.y - h, c.z - h };
    const vec3_t mx = { c.x + h, c.y + h, c.z + h };
    const int first = s->n_faces;

    /* Top (+y) */
    scene_add_face(s, (bsp_face_t){
        .verts = { {mn.x, mx.y, mx.z}, {mx.x, mx.y, mx.z},
                   {mx.x, mx.y, mn.z}, {mn.x, mx.y, mn.z} },
        .uvs   = { {mn.x, mx.z}, {mx.x, mx.z}, {mx.x, mn.z}, {mn.x, mn.z} },
        .tex   = tex,
    });
    /* Bottom (-y) */
    scene_add_face(s, (bsp_face_t){
        .verts = { {mn.x, mn.y, mn.z}, {mx.x, mn.y, mn.z},
                   {mx.x, mn.y, mx.z}, {mn.x, mn.y, mx.z} },
        .uvs   = { {mn.x, mn.z}, {mx.x, mn.z}, {mx.x, mx.z}, {mn.x, mx.z} },
        .tex   = tex,
    });
    /* North (+z) */
    scene_add_face(s, (bsp_face_t){
        .verts = { {mn.x, mn.y, mx.z}, {mx.x, mn.y, mx.z},
                   {mx.x, mx.y, mx.z}, {mn.x, mx.y, mx.z} },
        .uvs   = { {mn.x, mn.y}, {mx.x, mn.y}, {mx.x, mx.y}, {mn.x, mx.y} },
        .tex   = tex,
    });
    /* South (-z) */
    scene_add_face(s, (bsp_face_t){
        .verts = { {mx.x, mn.y, mn.z}, {mn.x, mn.y, mn.z},
                   {mn.x, mx.y, mn.z}, {mx.x, mx.y, mn.z} },
        .uvs   = { {mx.x, mn.y}, {mn.x, mn.y}, {mn.x, mx.y}, {mx.x, mx.y} },
        .tex   = tex,
    });
    /* East (+x) */
    scene_add_face(s, (bsp_face_t){
        .verts = { {mx.x, mn.y, mx.z}, {mx.x, mn.y, mn.z},
                   {mx.x, mx.y, mn.z}, {mx.x, mx.y, mx.z} },
        .uvs   = { {mx.z, mn.y}, {mn.z, mn.y}, {mn.z, mx.y}, {mx.z, mx.y} },
        .tex   = tex,
    });
    /* West (-x) */
    scene_add_face(s, (bsp_face_t){
        .verts = { {mn.x, mn.y, mn.z}, {mn.x, mn.y, mx.z},
                   {mn.x, mx.y, mx.z}, {mn.x, mx.y, mn.z} },
        .uvs   = { {mn.z, mn.y}, {mx.z, mn.y}, {mx.z, mx.y}, {mn.z, mx.y} },
        .tex   = tex,
    });
    return first;
}

int scene_add_pillar(scene_t *s, vec3_t base_c, f32 w, f32 height, tex_sample_fn tex) {
    /* Pilar = box rectangular sin bottom (asume sobre piso). */
    const f32 hw = w * 0.5f;
    const vec3_t mn = { base_c.x - hw, base_c.y,          base_c.z - hw };
    const vec3_t mx = { base_c.x + hw, base_c.y + height, base_c.z + hw };
    const int first = s->n_faces;

    /* Top */
    scene_add_face(s, (bsp_face_t){
        .verts = { {mn.x, mx.y, mx.z}, {mx.x, mx.y, mx.z},
                   {mx.x, mx.y, mn.z}, {mn.x, mx.y, mn.z} },
        .uvs   = { {mn.x, mx.z}, {mx.x, mx.z}, {mx.x, mn.z}, {mn.x, mn.z} },
        .tex   = tex,
    });
    /* North */
    scene_add_face(s, (bsp_face_t){
        .verts = { {mn.x, mn.y, mx.z}, {mx.x, mn.y, mx.z},
                   {mx.x, mx.y, mx.z}, {mn.x, mx.y, mx.z} },
        .uvs   = { {mn.x, mn.y}, {mx.x, mn.y}, {mx.x, mx.y}, {mn.x, mx.y} },
        .tex   = tex,
    });
    /* South */
    scene_add_face(s, (bsp_face_t){
        .verts = { {mx.x, mn.y, mn.z}, {mn.x, mn.y, mn.z},
                   {mn.x, mx.y, mn.z}, {mx.x, mx.y, mn.z} },
        .uvs   = { {mx.x, mn.y}, {mn.x, mn.y}, {mn.x, mx.y}, {mx.x, mx.y} },
        .tex   = tex,
    });
    /* East */
    scene_add_face(s, (bsp_face_t){
        .verts = { {mx.x, mn.y, mx.z}, {mx.x, mn.y, mn.z},
                   {mx.x, mx.y, mn.z}, {mx.x, mx.y, mx.z} },
        .uvs   = { {mx.z, mn.y}, {mn.z, mn.y}, {mn.z, mx.y}, {mx.z, mx.y} },
        .tex   = tex,
    });
    /* West */
    scene_add_face(s, (bsp_face_t){
        .verts = { {mn.x, mn.y, mn.z}, {mn.x, mn.y, mx.z},
                   {mn.x, mx.y, mx.z}, {mn.x, mx.y, mn.z} },
        .uvs   = { {mn.z, mn.y}, {mx.z, mn.y}, {mx.z, mx.y}, {mn.z, mx.y} },
        .tex   = tex,
    });
    return first;
}

void scene_load_default(scene_t *s) {
    scene_init(s);
    /* Floor amplio. */
    scene_add_floor(s, (vec3_t){0, 0, 0}, 24.0f, tex_floor);
    /* Pillar central. */
    scene_add_pillar(s, (vec3_t){0, 0, 0}, 0.8f, 3.5f, tex_brick);
    /* Cubos decorativos. */
    scene_add_cube(s, (vec3_t){3.0f, 0.5f, 1.5f}, 1.0f, tex_brick);
    scene_add_cube(s, (vec3_t){-2.5f, 0.4f, -1.8f}, 0.8f, tex_checker);
}

void scene_bake_lightmaps(scene_t *s, vec3_t light_pos, f32 intensity, f32 ambient) {
    for (int i = 0; i < s->n_faces; ++i) {
        lightmap_bake(&s->lightmaps[i], &s->faces[i],
                       s->faces, s->n_faces, i,
                       light_pos, intensity, ambient);
    }
}
