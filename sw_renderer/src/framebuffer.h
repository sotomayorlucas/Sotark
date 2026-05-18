#ifndef SOTARK_SW_FRAMEBUFFER_H
#define SOTARK_SW_FRAMEBUFFER_H

#include "common/types.h"

typedef struct {
    u32 *color;          /* ARGB8888 */
    f32 *depth;          /* NDC z (or any monotonic depth metric); lower = closer */
    int  w, h;
    int  pitch_pixels;   /* stride in u32 units; applies to both color and depth */
} framebuf_t;

/* Bounds-checked pixel plot. Out-of-range = no-op. Does NOT touch the depth buffer. */
static inline void framebuf_plot(framebuf_t *fb, int x, int y, u32 color) {
    if ((unsigned)x < (unsigned)fb->w && (unsigned)y < (unsigned)fb->h) {
        fb->color[y * fb->pitch_pixels + x] = color;
    }
}

void framebuf_clear_color(framebuf_t *fb, u32 color);
void framebuf_clear_depth(framebuf_t *fb, f32 depth);

#endif
