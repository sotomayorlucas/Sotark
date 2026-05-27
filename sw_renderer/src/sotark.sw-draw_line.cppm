module;

#include <cstdlib>

export module sotark.sw:draw_line;

import sotark.common;
import :framebuffer;

export namespace sotark::sw {

// Integer Bresenham. Endpoints may lie outside fb; clipping is per-pixel via
// Framebuf::plot bounds check.
inline void draw_line(Framebuf& fb, int x0, int y0, int x1, int y1, u32 color) noexcept {
    const int dx =  std::abs(x1 - x0);
    const int dy = -std::abs(y1 - y0);
    const int sx = x0 < x1 ? 1 : -1;
    const int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        fb.plot(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        const int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

}  // namespace sotark::sw
