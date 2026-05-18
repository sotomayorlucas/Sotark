#include "draw_tri.h"

#include <math.h>

static inline f32 edge_fn(f32 ax, f32 ay, f32 bx, f32 by, f32 px, f32 py) {
    return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}

void draw_triangle_solid(framebuf_t *fb,
                         screen_vert_t v0, screen_vert_t v1, screen_vert_t v2,
                         u32 color) {
    const f32 area = edge_fn(v0.x, v0.y, v1.x, v1.y, v2.x, v2.y);
    if (fabsf(area) < 1e-6f) return;

    int x_min = (int)floorf(fminf(fminf(v0.x, v1.x), v2.x));
    int y_min = (int)floorf(fminf(fminf(v0.y, v1.y), v2.y));
    int x_max = (int)ceilf (fmaxf(fmaxf(v0.x, v1.x), v2.x));
    int y_max = (int)ceilf (fmaxf(fmaxf(v0.y, v1.y), v2.y));
    if (x_min < 0)      x_min = 0;
    if (y_min < 0)      y_min = 0;
    if (x_max > fb->w)  x_max = fb->w;
    if (y_max > fb->h)  y_max = fb->h;

    const f32 inv_area = 1.0f / area;
    const bool ccw     = area > 0.0f;

    for (int y = y_min; y < y_max; ++y) {
        for (int x = x_min; x < x_max; ++x) {
            const f32 px = (f32)x + 0.5f;
            const f32 py = (f32)y + 0.5f;
            const f32 w0 = edge_fn(v1.x, v1.y, v2.x, v2.y, px, py);
            const f32 w1 = edge_fn(v2.x, v2.y, v0.x, v0.y, px, py);
            const f32 w2 = edge_fn(v0.x, v0.y, v1.x, v1.y, px, py);
            const bool inside = ccw
                ? (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f)
                : (w0 <= 0.0f && w1 <= 0.0f && w2 <= 0.0f);
            if (!inside) continue;

            const f32 b0 = w0 * inv_area;
            const f32 b1 = w1 * inv_area;
            const f32 b2 = w2 * inv_area;
            const f32 z  = b0 * v0.z + b1 * v1.z + b2 * v2.z;

            const int idx = y * fb->pitch_pixels + x;
            if (z < fb->depth[idx]) {
                fb->depth[idx] = z;
                fb->color[idx] = color;
            }
        }
    }
}

void draw_triangle_textured_perspective(framebuf_t *fb,
                                        screen_vert_t v0, screen_vert_t v1, screen_vert_t v2,
                                        tex_sample_fn sample) {
    const f32 area = edge_fn(v0.x, v0.y, v1.x, v1.y, v2.x, v2.y);
    if (fabsf(area) < 1e-6f) return;

    int x_min = (int)floorf(fminf(fminf(v0.x, v1.x), v2.x));
    int y_min = (int)floorf(fminf(fminf(v0.y, v1.y), v2.y));
    int x_max = (int)ceilf (fmaxf(fmaxf(v0.x, v1.x), v2.x));
    int y_max = (int)ceilf (fmaxf(fmaxf(v0.y, v1.y), v2.y));
    if (x_min < 0)      x_min = 0;
    if (y_min < 0)      y_min = 0;
    if (x_max > fb->w)  x_max = fb->w;
    if (y_max > fb->h)  y_max = fb->h;

    const f32 inv_area = 1.0f / area;
    const bool ccw     = area > 0.0f;

    /* Pre-multiplicamos en vértices: u*inv_w y v*inv_w son las cantidades
     * que viajan linealmente en screen space. */
    const f32 uw0 = v0.u * v0.inv_w, uw1 = v1.u * v1.inv_w, uw2 = v2.u * v2.inv_w;
    const f32 vw0 = v0.v * v0.inv_w, vw1 = v1.v * v1.inv_w, vw2 = v2.v * v2.inv_w;

    for (int y = y_min; y < y_max; ++y) {
        for (int x = x_min; x < x_max; ++x) {
            const f32 px = (f32)x + 0.5f;
            const f32 py = (f32)y + 0.5f;
            const f32 w0 = edge_fn(v1.x, v1.y, v2.x, v2.y, px, py);
            const f32 w1 = edge_fn(v2.x, v2.y, v0.x, v0.y, px, py);
            const f32 w2 = edge_fn(v0.x, v0.y, v1.x, v1.y, px, py);
            const bool inside = ccw
                ? (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f)
                : (w0 <= 0.0f && w1 <= 0.0f && w2 <= 0.0f);
            if (!inside) continue;

            const f32 b0 = w0 * inv_area;
            const f32 b1 = w1 * inv_area;
            const f32 b2 = w2 * inv_area;
            const f32 z  = b0 * v0.z + b1 * v1.z + b2 * v2.z;

            const int idx = y * fb->pitch_pixels + x;
            if (z >= fb->depth[idx]) continue;

            /* Interpolamos lineal 1/w, u/w, v/w y dividimos para recuperar u, v. */
            const f32 invw_p = b0 * v0.inv_w + b1 * v1.inv_w + b2 * v2.inv_w;
            const f32 uw     = b0 * uw0      + b1 * uw1      + b2 * uw2;
            const f32 vw     = b0 * vw0      + b1 * vw1      + b2 * vw2;
            const f32 w_pix  = 1.0f / invw_p;
            const f32 tu     = uw * w_pix;
            const f32 tv     = vw * w_pix;

            fb->depth[idx] = z;
            fb->color[idx] = sample(tu, tv);
        }
    }
}

void draw_triangle_textured_affine(framebuf_t *fb,
                                   screen_vert_t v0, screen_vert_t v1, screen_vert_t v2,
                                   tex_sample_fn sample) {
    const f32 area = edge_fn(v0.x, v0.y, v1.x, v1.y, v2.x, v2.y);
    if (fabsf(area) < 1e-6f) return;

    int x_min = (int)floorf(fminf(fminf(v0.x, v1.x), v2.x));
    int y_min = (int)floorf(fminf(fminf(v0.y, v1.y), v2.y));
    int x_max = (int)ceilf (fmaxf(fmaxf(v0.x, v1.x), v2.x));
    int y_max = (int)ceilf (fmaxf(fmaxf(v0.y, v1.y), v2.y));
    if (x_min < 0)      x_min = 0;
    if (y_min < 0)      y_min = 0;
    if (x_max > fb->w)  x_max = fb->w;
    if (y_max > fb->h)  y_max = fb->h;

    const f32 inv_area = 1.0f / area;
    const bool ccw     = area > 0.0f;

    for (int y = y_min; y < y_max; ++y) {
        for (int x = x_min; x < x_max; ++x) {
            const f32 px = (f32)x + 0.5f;
            const f32 py = (f32)y + 0.5f;
            const f32 w0 = edge_fn(v1.x, v1.y, v2.x, v2.y, px, py);
            const f32 w1 = edge_fn(v2.x, v2.y, v0.x, v0.y, px, py);
            const f32 w2 = edge_fn(v0.x, v0.y, v1.x, v1.y, px, py);
            const bool inside = ccw
                ? (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f)
                : (w0 <= 0.0f && w1 <= 0.0f && w2 <= 0.0f);
            if (!inside) continue;

            const f32 b0 = w0 * inv_area;
            const f32 b1 = w1 * inv_area;
            const f32 b2 = w2 * inv_area;
            const f32 z  = b0 * v0.z + b1 * v1.z + b2 * v2.z;
            /* AFFINE: u, v interpolated linearly in screen space (no /z). */
            const f32 tu = b0 * v0.u + b1 * v1.u + b2 * v2.u;
            const f32 tv = b0 * v0.v + b1 * v1.v + b2 * v2.v;

            const int idx = y * fb->pitch_pixels + x;
            if (z < fb->depth[idx]) {
                fb->depth[idx] = z;
                fb->color[idx] = sample(tu, tv);
            }
        }
    }
}
