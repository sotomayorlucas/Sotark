module;

#include <numbers>
#include <cmath>

export module sotark.common:util;

import :types;

export namespace sotark {

inline constexpr f32 pi_f       = std::numbers::pi_v<f32>;
inline constexpr f32 two_pi_f   = 2.0f * pi_f;
inline constexpr f32 inv_pi_f   = std::numbers::inv_pi_v<f32>;
inline constexpr f32 sqrt2_f    = std::numbers::sqrt2_v<f32>;

constexpr f32 deg_to_rad(f32 d) noexcept { return d * (pi_f / 180.0f); }
constexpr f32 rad_to_deg(f32 r) noexcept { return r * (180.0f / pi_f); }

constexpr f32 lerp_f(f32 a, f32 b, f32 t) noexcept { return std::lerp(a, b, t); }

}  // namespace sotark
