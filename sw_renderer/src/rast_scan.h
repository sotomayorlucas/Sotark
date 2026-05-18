#ifndef SOTARK_SW_RAST_SCAN_H
#define SOTARK_SW_RAST_SCAN_H

#include "framebuffer.h"
#include "draw_tri.h"      /* for screen_vert_t */
#include "span_buffer.h"
#include "texture.h"
#include "lightmap.h"

/*
 * Vertex en clip space (post-MVP, pre-perspective divide). Lo necesitamos
 * para hacer near-plane clipping ANTES del divide — si un vértice tiene
 * clip.w < near, su proyección sale inválida (signo flip, screen coords
 * mirroring) y el rasterizer dibuja garbage.
 */
typedef struct {
    f32 x, y, z, w;
    f32 u, v;
} clip_vert_t;

/*
 * Sutherland-Hodgman clipping del triángulo contra el plano w = near_w
 * (= cualquier punto con clip.w < near_w queda fuera). Pueden salir 0, 1
 * o 2 triángulos resultantes. `out` debe tener capacidad de 6 vertices
 * (2 tris × 3 verts).
 *
 * Devuelve la cantidad de triángulos en out (0/1/2).
 */
int clip_triangle_near(clip_vert_t a, clip_vert_t b, clip_vert_t c,
                       f32 near_w, clip_vert_t out[6]);

/*
 * Rasterizer scanline + perspective-correct + span buffer.
 *
 * Diferencias con el rasterizer edge-function de M4:
 *   - Itera scanlines top-to-bottom (no bbox + per-pixel inside test).
 *   - Calcula x_left, x_right por scanline interpolando los edges.
 *   - Para cada scanline emite UN span al span buffer; el span buffer
 *     se encarga de clipear contra lo ya cubierto.
 *   - Backface cull por signed area (skip si back-facing).
 *
 * Los gradientes perspective (∂(1/w)/∂x, ∂(u/w)/∂y, etc.) se computan UNA
 * vez por triángulo y se evalúan en el origen del span — cada pixel solo
 * suma deltas.
 */
/*
 * y_clip_min/max permiten clippear el rasterizer a un rango de scanlines —
 * usado por M10 para que cada thread renderice solo su stripe horizontal
 * sin tocar pixels de otros threads (sin necesidad de locks).
 * Para single-threaded, pasar 0 y fb->h.
 */
void rast_scan_textured(framebuf_t *fb, span_buffer_t *sb,
                        screen_vert_t v0, screen_vert_t v1, screen_vert_t v2,
                        tex_sample_fn tex,
                        const lightmap_t *lm,    /* NULL = full bright */
                        int y_clip_min, int y_clip_max);

#endif
