#ifndef SOTARK_SW_LIGHTMAP_H
#define SOTARK_SW_LIGHTMAP_H

#include "common/types.h"
#include "common/vec.h"
#include "bsp.h"

/*
 * Lightmap estático per-surface, estilo Quake.
 *
 * Cada face de la escena tiene un lightmap chico (e.g., 32×32) que guarda
 * la irradiancia precomputada. Durante render, sampleamos el lightmap con
 * bilinear filtering y lo multiplicamos con la base texture — el resultado
 * son sombras y falloff "gratis", computados una vez al cargar la escena.
 *
 * El lightmap se parametriza por la misma (u, v) de la base texture: cada
 * lightmap recuerda el RANGO de UV que cubre (uv_min, uv_max) y mappea
 * proporcionalmente a sus texels.
 *
 * Para baker: por cada texel, computamos su posición en world, casteamos
 * shadow ray a cada light, sumamos contribuciones lambertian. Solo direct
 * lighting + ambient (sin radiosity bounces — eso queda para "the rest of
 * your life" en raytracer-land).
 */
typedef struct {
    u32 *texels;        /* ARGB modulator: blanco = full lit, oscuro = sombra */
    int  w, h;
    f32  u_min, u_max;  /* rango de UV que el lightmap cubre */
    f32  v_min, v_max;
} lightmap_t;

/* Sample bilineal (u, v) en world UV coords. Devuelve u32 ARGB modulator
 * (alpha siempre 0xFF). */
u32 lightmap_sample_bilinear(const lightmap_t *lm, f32 u, f32 v);

/* Bake un lightmap para una face específica. Por cada texel:
 *   - posición world via bilinear interp de los 4 verts del quad
 *   - shadow ray test a la luz contra TODAS las otras faces
 *   - irradiance = ambient + light_intensity · cos(N, L) / dist² (si clear)
 *
 * skip_idx evita auto-intersección. Para correr una sola vez al init. */
void lightmap_bake(lightmap_t *lm,
                   const bsp_face_t *face,
                   const bsp_face_t *all_faces, int n_faces, int skip_idx,
                   vec3_t light_pos, f32 light_intensity, f32 ambient);

#endif
