module;

#include <cmath>
#include <functional>

export module sotark.sw:texture;

import sotark.common;

export namespace sotark::sw {

// Pure (u, v) → ARGB sampler.
using TexSampleFn = u32 (*)(f32, f32);

inline u32 tex_checker(f32 u, f32 v) noexcept {
    const int cu = static_cast<int>(std::floor(u));
    const int cv = static_cast<int>(std::floor(v));
    return ((cu ^ cv) & 1) ? 0xFFE0E0E0u : 0xFF303030u;
}

inline u32 tex_brick(f32 u, f32 v) noexcept {
    const int row   = static_cast<int>(std::floor(v));
    const f32 u_off = (row & 1) ? 1.0f : 0.0f;
    const f32 uu    = u + u_off;
    const f32 fu    = uu - std::floor(uu * 0.5f) * 2.0f;
    const f32 fv    = v - std::floor(v);
    if (fu < 0.08f || fu > 1.92f || fv < 0.08f || fv > 0.92f) return 0xFF555555u;
    const int col = static_cast<int>(std::floor(uu * 0.5f));
    const u32 n   = static_cast<u32>((col * 73856093) ^ (row * 19349663));
    const u8 r = 140u + static_cast<u8>((n >> 16) & 31);
    const u8 g =  70u + static_cast<u8>((n >>  8) & 15);
    const u8 b =  50u + static_cast<u8>( n        & 15);
    return 0xFF000000u | (static_cast<u32>(r) << 16) | (static_cast<u32>(g) << 8) | b;
}

inline u32 tex_floor(f32 u, f32 v) noexcept {
    const f32 fu = u * 2.0f;
    const f32 fv = v * 2.0f;
    const int cu = static_cast<int>(std::floor(fu));
    const int cv = static_cast<int>(std::floor(fv));
    const f32 lu = fu - std::floor(fu);
    const f32 lv = fv - std::floor(fv);
    if (lu < 0.04f || lu > 0.96f || lv < 0.04f || lv > 0.96f) return 0xFF202020u;
    return ((cu ^ cv) & 1) ? 0xFFB0B0B0u : 0xFF909090u;
}

inline u32 tex_ceiling(f32 u, f32 v) noexcept {
    const u32 n = static_cast<u32>(static_cast<int>(std::floor(u * 4.0f)) * 73856093) ^
                  static_cast<u32>(static_cast<int>(std::floor(v * 4.0f)) * 19349663);
    const u8 r = 200u + static_cast<u8>((n >> 16) & 15);
    const u8 g = 195u + static_cast<u8>((n >>  8) & 15);
    const u8 b = 180u + static_cast<u8>( n        & 15);
    return 0xFF000000u | (static_cast<u32>(r) << 16) | (static_cast<u32>(g) << 8) | b;
}

}  // namespace sotark::sw
