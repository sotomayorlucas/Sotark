module;

#include <array>
#include <cmath>
#include <algorithm>

export module sotark.common:color;

import :types;
import :vec;

export namespace sotark {

// RGB color in linear space, components in [0, 1] (may exceed for HDR).
using Rgb = Vec3;

constexpr u8 to_byte(f32 c) noexcept {
    const f32 clamped = std::clamp(c, 0.0f, 1.0f);
    return static_cast<u8>(255.999f * clamped);
}

// Linear -> 8-bit, no gamma. Used in Shirley §2-era code paths.
constexpr std::array<u8, 3> to_bytes(Rgb c) noexcept {
    return { to_byte(c.x), to_byte(c.y), to_byte(c.z) };
}

// sqrt gamma 2.0 — for post-antialias raytracer output. constexpr-friendly
// at runtime; cmath::sqrt is constexpr in C++26.
inline std::array<u8, 3> to_bytes_gamma2(Rgb c) noexcept {
    const f32 r = std::sqrt(std::clamp(c.x, 0.0f, 1.0f));
    const f32 g = std::sqrt(std::clamp(c.y, 0.0f, 1.0f));
    const f32 b = std::sqrt(std::clamp(c.z, 0.0f, 1.0f));
    return {
        static_cast<u8>(255.999f * r),
        static_cast<u8>(255.999f * g),
        static_cast<u8>(255.999f * b),
    };
}

}  // namespace sotark
