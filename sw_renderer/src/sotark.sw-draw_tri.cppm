module;

#include <cmath>
#include <algorithm>

export module sotark.sw:draw_tri;

import sotark.common;
import :framebuffer;
import :texture;

export namespace sotark::sw {

// Vertex in screen space:
//   x, y    — pixel coords (float, pre-rounding)
//   z       — NDC depth in [-1, 1] (lower = closer)
//   u, v    — texture coords
//   inv_w   — 1 / clip.w (required by perspective variant)
struct ScreenVert {
    f32 x{}, y{}, z{};
    f32 u{}, v{};
    f32 inv_w{};
};

constexpr f32 edge_fn(f32 ax, f32 ay, f32 bx, f32 by, f32 px, f32 py) noexcept {
    return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}

inline void draw_triangle_solid(Framebuf& fb,
                                ScreenVert v0, ScreenVert v1, ScreenVert v2,
                                u32 color) noexcept {
    const f32 area = edge_fn(v0.x, v0.y, v1.x, v1.y, v2.x, v2.y);
    if (std::abs(area) < 1e-6f) return;

    int x_min = static_cast<int>(std::floor(std::min({v0.x, v1.x, v2.x})));
    int y_min = static_cast<int>(std::floor(std::min({v0.y, v1.y, v2.y})));
    int x_max = static_cast<int>(std::ceil (std::max({v0.x, v1.x, v2.x})));
    int y_max = static_cast<int>(std::ceil (std::max({v0.y, v1.y, v2.y})));
    x_min = std::max(0, x_min); y_min = std::max(0, y_min);
    x_max = std::min(fb.w, x_max); y_max = std::min(fb.h, y_max);

    const f32  inv_area = 1.0f / area;
    const bool ccw      = area > 0.0f;

    for (int y = y_min; y < y_max; ++y) {
        for (int x = x_min; x < x_max; ++x) {
            const f32 px = static_cast<f32>(x) + 0.5f;
            const f32 py = static_cast<f32>(y) + 0.5f;
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

            const int idx = y * fb.pitch_pixels + x;
            if (z < fb.depth[idx]) {
                fb.depth[idx] = z;
                fb.color[idx] = color;
            }
        }
    }
}

inline void draw_triangle_textured_affine(Framebuf& fb,
                                          ScreenVert v0, ScreenVert v1, ScreenVert v2,
                                          TexSampleFn sample) noexcept {
    const f32 area = edge_fn(v0.x, v0.y, v1.x, v1.y, v2.x, v2.y);
    if (std::abs(area) < 1e-6f) return;

    int x_min = std::max(0, static_cast<int>(std::floor(std::min({v0.x, v1.x, v2.x}))));
    int y_min = std::max(0, static_cast<int>(std::floor(std::min({v0.y, v1.y, v2.y}))));
    int x_max = std::min(fb.w, static_cast<int>(std::ceil (std::max({v0.x, v1.x, v2.x}))));
    int y_max = std::min(fb.h, static_cast<int>(std::ceil (std::max({v0.y, v1.y, v2.y}))));

    const f32  inv_area = 1.0f / area;
    const bool ccw      = area > 0.0f;

    for (int y = y_min; y < y_max; ++y) {
        for (int x = x_min; x < x_max; ++x) {
            const f32 px = static_cast<f32>(x) + 0.5f;
            const f32 py = static_cast<f32>(y) + 0.5f;
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
            const f32 tu = b0 * v0.u + b1 * v1.u + b2 * v2.u;
            const f32 tv = b0 * v0.v + b1 * v1.v + b2 * v2.v;

            const int idx = y * fb.pitch_pixels + x;
            if (z < fb.depth[idx]) {
                fb.depth[idx] = z;
                fb.color[idx] = sample(tu, tv);
            }
        }
    }
}

inline void draw_triangle_textured_perspective(Framebuf& fb,
                                               ScreenVert v0, ScreenVert v1, ScreenVert v2,
                                               TexSampleFn sample) noexcept {
    const f32 area = edge_fn(v0.x, v0.y, v1.x, v1.y, v2.x, v2.y);
    if (std::abs(area) < 1e-6f) return;

    int x_min = std::max(0, static_cast<int>(std::floor(std::min({v0.x, v1.x, v2.x}))));
    int y_min = std::max(0, static_cast<int>(std::floor(std::min({v0.y, v1.y, v2.y}))));
    int x_max = std::min(fb.w, static_cast<int>(std::ceil (std::max({v0.x, v1.x, v2.x}))));
    int y_max = std::min(fb.h, static_cast<int>(std::ceil (std::max({v0.y, v1.y, v2.y}))));

    const f32  inv_area = 1.0f / area;
    const bool ccw      = area > 0.0f;

    const f32 uw0 = v0.u * v0.inv_w, uw1 = v1.u * v1.inv_w, uw2 = v2.u * v2.inv_w;
    const f32 vw0 = v0.v * v0.inv_w, vw1 = v1.v * v1.inv_w, vw2 = v2.v * v2.inv_w;

    for (int y = y_min; y < y_max; ++y) {
        for (int x = x_min; x < x_max; ++x) {
            const f32 px = static_cast<f32>(x) + 0.5f;
            const f32 py = static_cast<f32>(y) + 0.5f;
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

            const int idx = y * fb.pitch_pixels + x;
            if (z >= fb.depth[idx]) continue;

            const f32 invw_p = b0 * v0.inv_w + b1 * v1.inv_w + b2 * v2.inv_w;
            const f32 uw     = b0 * uw0      + b1 * uw1      + b2 * uw2;
            const f32 vw     = b0 * vw0      + b1 * vw1      + b2 * vw2;
            const f32 w_pix  = 1.0f / invw_p;
            const f32 tu     = uw * w_pix;
            const f32 tv     = vw * w_pix;

            fb.depth[idx] = z;
            fb.color[idx] = sample(tu, tv);
        }
    }
}

}  // namespace sotark::sw
