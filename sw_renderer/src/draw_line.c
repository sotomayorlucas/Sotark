#include "draw_line.h"

static int iabs(int x) { return x < 0 ? -x : x; }

void draw_line(framebuf_t *fb, int x0, int y0, int x1, int y1, u32 color) {
    int dx =  iabs(x1 - x0);
    int dy = -iabs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        framebuf_plot(fb, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}
