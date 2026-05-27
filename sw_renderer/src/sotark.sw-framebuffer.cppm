module;

#include <mdspan>
#include <algorithm>
#include <span>

export module sotark.sw:framebuffer;

import sotark.common;

export namespace sotark::sw {

// Framebuffer with separate color (ARGB8888) and depth planes.
// Color/depth views are exposed as std::mdspan for 2D-indexed access.
struct Framebuf {
    u32* color{nullptr};
    f32* depth{nullptr};
    int  w{0};
    int  h{0};
    int  pitch_pixels{0};        // stride in u32 units (color & depth share it)

    constexpr void plot(int x, int y, u32 c) noexcept {
        if (static_cast<unsigned>(x) < static_cast<unsigned>(w) &&
            static_cast<unsigned>(y) < static_cast<unsigned>(h)) {
            color[y * pitch_pixels + x] = c;
        }
    }

    using ColorView = std::mdspan<u32, std::dextents<usize, 2>, std::layout_stride>;
    using DepthView = std::mdspan<f32, std::dextents<usize, 2>, std::layout_stride>;

    ColorView color_view() noexcept {
        return ColorView{
            color,
            std::layout_stride::mapping<std::dextents<usize, 2>>{
                std::dextents<usize, 2>{static_cast<usize>(h), static_cast<usize>(w)},
                std::array<usize, 2>{static_cast<usize>(pitch_pixels), 1u}}};
    }
    DepthView depth_view() noexcept {
        return DepthView{
            depth,
            std::layout_stride::mapping<std::dextents<usize, 2>>{
                std::dextents<usize, 2>{static_cast<usize>(h), static_cast<usize>(w)},
                std::array<usize, 2>{static_cast<usize>(pitch_pixels), 1u}}};
    }
};

inline void clear_color(Framebuf& fb, u32 c) noexcept {
    for (int y = 0; y < fb.h; ++y) {
        u32* row = fb.color + y * fb.pitch_pixels;
        std::fill_n(row, fb.w, c);
    }
}

inline void clear_depth(Framebuf& fb, f32 d) noexcept {
    for (int y = 0; y < fb.h; ++y) {
        f32* row = fb.depth + y * fb.pitch_pixels;
        std::fill_n(row, fb.w, d);
    }
}

}  // namespace sotark::sw
