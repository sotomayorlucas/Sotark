#ifndef SOTARK_SW_DRAW_TRI_H
#define SOTARK_SW_DRAW_TRI_H

#include "framebuffer.h"
#include "texture.h"

/* Vertex en screen space:
 *   x, y    — pixel coords (float, pre-rounding)
 *   z       — NDC depth en [-1, 1] (lower = closer)
 *   u, v    — coords de textura del vértice
 *   inv_w   — 1 / clip.w, requerido por la variante perspective
 */
typedef struct {
    f32 x, y, z;
    f32 u, v;
    f32 inv_w;
} screen_vert_t;

/* Rasterización scanline edge-function + barycentric + z-test, color sólido.
 * Ignora u, v, inv_w. */
void draw_triangle_solid(framebuf_t *fb,
                         screen_vert_t v0, screen_vert_t v1, screen_vert_t v2,
                         u32 color);

/* M3: AFFINE texture mapping. u, v se interpolan lineal en screen space
 * (sin dividir por z). Produce el característico "swimming" en superficies
 * oblícuas — feature didáctico, no bug. */
void draw_triangle_textured_affine(framebuf_t *fb,
                                   screen_vert_t v0, screen_vert_t v1, screen_vert_t v2,
                                   tex_sample_fn sample);

/* M4: PERSPECTIVE-CORRECT texture mapping.
 *
 * Idea (Quake/Abrash): después de proyectar perspectivamente, las cantidades
 * 1/w, u/w, v/w son LINEALES en screen space (porque la proyección es una
 * división por w; lo que es lineal antes de dividir queda lineal después de
 * multiplicar por 1/w).
 *
 * Entonces interpolamos linealmente esos tres usando barycentric, y al final
 * dividimos: u = (u/w) / (1/w). Esta variante hace la divide por pixel —
 * exacto y didácticamente claro. La optimización clásica de Abrash (dividir
 * cada 8 o 16 pixels e interpolar afín entre medio) queda para M9.
 *
 * Para esta función el caller DEBE poblar v0.inv_w / v1.inv_w / v2.inv_w. */
void draw_triangle_textured_perspective(framebuf_t *fb,
                                        screen_vert_t v0, screen_vert_t v1, screen_vert_t v2,
                                        tex_sample_fn sample);

#endif
