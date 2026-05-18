#ifndef SOTARK_SW_DRAW_LINE_H
#define SOTARK_SW_DRAW_LINE_H

#include "framebuffer.h"

/* Integer Bresenham. Endpoints may lie outside the framebuffer; the line is
 * clipped per-pixel by framebuf_plot's bounds check. */
void draw_line(framebuf_t *fb, int x0, int y0, int x1, int y1, u32 color);

#endif
