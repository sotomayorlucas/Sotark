#include "framebuffer.h"

void framebuf_clear_color(framebuf_t *fb, u32 color) {
    for (int y = 0; y < fb->h; ++y) {
        u32 *row = fb->color + y * fb->pitch_pixels;
        for (int x = 0; x < fb->w; ++x) row[x] = color;
    }
}

void framebuf_clear_depth(framebuf_t *fb, f32 depth) {
    for (int y = 0; y < fb->h; ++y) {
        f32 *row = fb->depth + y * fb->pitch_pixels;
        for (int x = 0; x < fb->w; ++x) row[x] = depth;
    }
}
