#ifndef SOTARK_SW_SCENE_H
#define SOTARK_SW_SCENE_H

#include "common/types.h"
#include "common/vec.h"
#include "bsp.h"
#include "lightmap.h"
#include "texture.h"

/*
 * Scene mutable para el editor. Substituye las arrays `static const`
 * de la versión Quake-only por storage runtime con add/remove/edit.
 *
 * Layout: arrays paralelos faces[]/lightmaps[]/lm_storage[]. La compaction
 * en remove es swap-with-last (no preserva orden — está OK porque el
 * orden se recalcula cada frame por distancia a cámara).
 */

#define SCENE_MAX_FACES 64
#define SCENE_LM_DIM    32

typedef struct {
    bsp_face_t faces      [SCENE_MAX_FACES];
    lightmap_t lightmaps  [SCENE_MAX_FACES];
    u32        lm_storage [SCENE_MAX_FACES][SCENE_LM_DIM * SCENE_LM_DIM];
    int        n_faces;
} scene_t;

/* Init un scene vacío (sin faces). Setea lightmap pointers a su lm_storage. */
void scene_init(scene_t *s);

/* Construye una scene default: floor 20x20 + 1 cubo en el centro. */
void scene_load_default(scene_t *s);

/* Agrega una face raw. Devuelve idx (≥0), o -1 si está full. */
int scene_add_face(scene_t *s, bsp_face_t face);

/* Quita la face de índice `idx`. Mueve la última al hueco (no preserva
 * orden — el sort por distancia se hace cada frame igual). */
void scene_remove_face(scene_t *s, int idx);

/* Primitive builders. Devuelven el idx de la PRIMER face creada
 * (el primitive abarca varias faces consecutivas). */
int  scene_add_floor(scene_t *s, vec3_t center, f32 size, tex_sample_fn tex);
int  scene_add_cube (scene_t *s, vec3_t center, f32 size, tex_sample_fn tex);
int  scene_add_pillar(scene_t *s, vec3_t base_center, f32 width, f32 height,
                       tex_sample_fn tex);

/* Bake lightmaps para todas las faces. Llamar tras agregar/quitar/mover. */
void scene_bake_lightmaps(scene_t *s, vec3_t light_pos,
                          f32 intensity, f32 ambient);

#endif
