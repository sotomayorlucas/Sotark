module;

#include <cmath>
#include <algorithm>
#include <array>

export module sotark.sw:rast_scan;

import sotark.common;
import :framebuffer;
import :draw_tri;
import :span_buffer;
import :texture;
import :lightmap;

export namespace sotark::sw {

// Vertex in clip space (post-MVP, pre-perspective divide).
struct ClipVert {
    f32 x{}, y{}, z{}, w{};
    f32 u{}, v{};
};

// Sutherland-Hodgman clip vs plane w = near_w. Output: 0, 1, or 2 triangles.
// `out` must have capacity for 6 vertices.
inline int clip_triangle_near(ClipVert a, ClipVert b, ClipVert c,
                              f32 near_w, std::array<ClipVert, 6>& out) noexcept {
    auto lerp_cv = [](ClipVert a, ClipVert b, f32 t) {
        return ClipVert{
            .x = a.x + (b.x - a.x) * t,
            .y = a.y + (b.y - a.y) * t,
            .z = a.z + (b.z - a.z) * t,
            .w = a.w + (b.w - a.w) * t,
            .u = a.u + (b.u - a.u) * t,
            .v = a.v + (b.v - a.v) * t,
        };
    };
    const std::array<ClipVert, 3> verts{a, b, c};
    const std::array<int, 3> ins{
        a.w >= near_w ? 1 : 0,
        b.w >= near_w ? 1 : 0,
        c.w >= near_w ? 1 : 0,
    };
    const int n_in = ins[0] + ins[1] + ins[2];
    if (n_in == 0) return 0;
    if (n_in == 3) { out[0] = a; out[1] = b; out[2] = c; return 1; }

    std::array<ClipVert, 4> poly{};
    int n = 0;
    for (int i = 0; i < 3; ++i) {
        const int j = (i + 1) % 3;
        if (ins[i]) poly[n++] = verts[i];
        if (ins[i] != ins[j]) {
            const f32 t = (near_w - verts[i].w) / (verts[j].w - verts[i].w);
            poly[n] = lerp_cv(verts[i], verts[j], t);
            poly[n].w = near_w;
            ++n;
        }
    }
    int n_tris = 0;
    for (int i = 1; i < n - 1; ++i) {
        out[n_tris*3 + 0] = poly[0];
        out[n_tris*3 + 1] = poly[i];
        out[n_tris*3 + 2] = poly[i + 1];
        ++n_tris;
    }
    return n_tris;
}

namespace detail {

struct GradCtx {
    f32 inv_w_dx{}, inv_w_dy{}, inv_w_origin{};
    f32 u_w_dx{},   u_w_dy{},   u_w_origin{};
    f32 v_w_dx{},   v_w_dy{},   v_w_origin{};
    TexSampleFn      tex{nullptr};
    const Lightmap*  lm{nullptr};
    Framebuf*        fb{nullptr};
};

constexpr u32 modulate_argb(u32 base, u32 mod) noexcept {
    const u32 br = (base >> 16) & 0xFFu, bg = (base >> 8) & 0xFFu, bb = base & 0xFFu;
    const u32 mr = (mod  >> 16) & 0xFFu, mg = (mod  >> 8) & 0xFFu, mb = mod  & 0xFFu;
    const u32 rr = (br * mr) >> 8;
    const u32 rg = (bg * mg) >> 8;
    const u32 rb = (bb * mb) >> 8;
    return 0xFF000000u | (rr << 16) | (rg << 8) | rb;
}

// M9: subspan FDIV (Abrash's classic). Divides only every 16 pixels; affine
// inside subspan. 1/16 the FDIVs per pixel for invisible quality cost.
inline constexpr int SUBSPAN = 16;

inline void draw_span_perspective(GradCtx& c, int y, int x0, int x1) {
    const f32 xs = static_cast<f32>(x0) + 0.5f;
    const f32 ys = static_cast<f32>(y)  + 0.5f;

    f32 inv_w_acc = c.inv_w_origin + xs * c.inv_w_dx + ys * c.inv_w_dy;
    f32 u_w_acc   = c.u_w_origin   + xs * c.u_w_dx   + ys * c.u_w_dy;
    f32 v_w_acc   = c.v_w_origin   + xs * c.v_w_dx   + ys * c.v_w_dy;

    f32 tu_left = u_w_acc / inv_w_acc;
    f32 tv_left = v_w_acc / inv_w_acc;

    u32* p = c.fb->color + static_cast<usize>(y) * c.fb->pitch_pixels + x0;

    int x = x0;
    while (x < x1) {
        int x_next = std::min(x + SUBSPAN, x1);
        const int len = x_next - x;
        const f32 len_f = static_cast<f32>(len);

        inv_w_acc += c.inv_w_dx * len_f;
        u_w_acc   += c.u_w_dx   * len_f;
        v_w_acc   += c.v_w_dx   * len_f;

        const f32 w_right  = 1.0f / inv_w_acc;
        const f32 tu_right = u_w_acc * w_right;
        const f32 tv_right = v_w_acc * w_right;

        const f32 inv_len = 1.0f / len_f;
        const f32 du = (tu_right - tu_left) * inv_len;
        const f32 dv = (tv_right - tv_left) * inv_len;

        f32 tu = tu_left;
        f32 tv = tv_left;

        if (c.lm) {
            for (int i = 0; i < len; ++i) {
                const u32 base = c.tex(tu, tv);
                const u32 mod  = c.lm->sample_bilinear(tu, tv);
                *p++ = modulate_argb(base, mod);
                tu += du; tv += dv;
            }
        } else {
            for (int i = 0; i < len; ++i) {
                *p++ = c.tex(tu, tv);
                tu += du; tv += dv;
            }
        }
        tu_left = tu_right;
        tv_left = tv_right;
        x = x_next;
    }
}

}  // namespace detail

// Scanline rasterizer with perspective + subspan FDIV + span buffer + lightmap.
// y_clip_min/max: rasterize only scanlines in [min, max). For M10 horizontal
// stripe multithreading. Single-threaded: pass 0 and fb.h.
inline void rast_scan_textured(Framebuf& fb, SpanBuffer& sb,
                               ScreenVert v0, ScreenVert v1, ScreenVert v2,
                               TexSampleFn tex,
                               const Lightmap* lm,
                               int y_clip_min, int y_clip_max) {
    const f32 signed_area = (v1.x - v0.x) * (v2.y - v0.y)
                          - (v1.y - v0.y) * (v2.x - v0.x);
    if (signed_area > 0.0f) return;   // backface (Y-flipped screen convention)

    // Sort by Y: v0=top, v1=mid, v2=bot.
    if (v0.y > v1.y) std::swap(v0, v1);
    if (v1.y > v2.y) std::swap(v1, v2);
    if (v0.y > v1.y) std::swap(v0, v1);

    const f32 dx10 = v1.x - v0.x, dy10 = v1.y - v0.y;
    const f32 dx20 = v2.x - v0.x, dy20 = v2.y - v0.y;
    const f32 det  = dx10 * dy20 - dy10 * dx20;
    if (std::abs(det) < 1e-6f) return;
    const f32 inv_det = 1.0f / det;

    const f32 iw0 = v0.inv_w, iw1 = v1.inv_w, iw2 = v2.inv_w;
    const f32 uw0 = v0.u * iw0, uw1 = v1.u * iw1, uw2 = v2.u * iw2;
    const f32 vw0 = v0.v * iw0, vw1 = v1.v * iw1, vw2 = v2.v * iw2;

    detail::GradCtx c{};
    c.fb = &fb; c.tex = tex; c.lm = lm;

    auto compute_grad = [&](f32 a0, f32 a1, f32 a2,
                             f32& dx, f32& dy, f32& org) {
        const f32 d10 = a1 - a0;
        const f32 d20 = a2 - a0;
        dx  = (d10 * dy20 - d20 * dy10) * inv_det;
        dy  = (d20 * dx10 - d10 * dx20) * inv_det;
        org = a0 - dx * v0.x - dy * v0.y;
    };
    compute_grad(iw0, iw1, iw2, c.inv_w_dx, c.inv_w_dy, c.inv_w_origin);
    compute_grad(uw0, uw1, uw2, c.u_w_dx,   c.u_w_dy,   c.u_w_origin);
    compute_grad(vw0, vw1, vw2, c.v_w_dx,   c.v_w_dy,   c.v_w_origin);

    const int y_top = static_cast<int>(std::ceil(v0.y));
    const int y_bot = static_cast<int>(std::ceil(v2.y));
    const int y_mid = static_cast<int>(std::ceil(v1.y));

    const f32 dy_long   = v2.y - v0.y;
    const f32 dxdy_long  = (dy_long  > 1e-6f) ? (v2.x - v0.x) / dy_long  : 0.0f;
    const f32 dy_upper  = v1.y - v0.y;
    const f32 dxdy_upper = (dy_upper > 1e-6f) ? (v1.x - v0.x) / dy_upper : 0.0f;
    const f32 dy_lower  = v2.y - v1.y;
    const f32 dxdy_lower = (dy_lower > 1e-6f) ? (v2.x - v1.x) / dy_lower : 0.0f;

    int y_start = std::max({y_top, y_clip_min, 0});
    int y_end   = std::min({y_bot, y_clip_max, fb.h});

    for (int y = y_start; y < y_end; ++y) {
        const f32 yc     = static_cast<f32>(y) + 0.5f;
        const f32 x_long = v0.x + (yc - v0.y) * dxdy_long;

        f32 x_short;
        if (y < y_mid && dy_upper > 1e-6f)       x_short = v0.x + (yc - v0.y) * dxdy_upper;
        else if (dy_lower > 1e-6f)               x_short = v1.x + (yc - v1.y) * dxdy_lower;
        else continue;

        const f32 xl_f = std::min(x_long, x_short);
        const f32 xr_f = std::max(x_long, x_short);
        const int xl   = static_cast<int>(std::ceil(xl_f));
        const int xr   = static_cast<int>(std::ceil(xr_f));
        if (xl >= xr) continue;

        sb.emit(y, xl, xr, [&c](int yy, int x0_, int x1_) {
            detail::draw_span_perspective(c, yy, x0_, x1_);
        });
    }
}

}  // namespace sotark::sw
