module;

#include <cmath>
#include <cstdlib>

export module sotark.common:vec;

import :types;

export namespace sotark {

struct Vec2 {
    f32 x{}, y{};

    constexpr Vec2 operator+(Vec2 b) const noexcept { return {x + b.x, y + b.y}; }
    constexpr Vec2 operator-(Vec2 b) const noexcept { return {x - b.x, y - b.y}; }
    constexpr Vec2 operator*(f32 s)  const noexcept { return {x * s, y * s}; }
    constexpr Vec2 operator-()       const noexcept { return {-x, -y}; }
    // GCC 16 ICEs on `friend = default` when this module is imported from
    // a non-module unit. Manual implementation works around it.
    constexpr bool operator==(const Vec2& o) const noexcept { return x == o.x && y == o.y; }
};

struct Vec3 {
    f32 x{}, y{}, z{};

    constexpr Vec3 operator+(Vec3 b) const noexcept { return {x + b.x, y + b.y, z + b.z}; }
    constexpr Vec3 operator-(Vec3 b) const noexcept { return {x - b.x, y - b.y, z - b.z}; }
    constexpr Vec3 operator*(Vec3 b) const noexcept { return {x * b.x, y * b.y, z * b.z}; }  // component-wise
    constexpr Vec3 operator*(f32 s)  const noexcept { return {x * s, y * s, z * s}; }
    constexpr Vec3 operator/(f32 s)  const noexcept { return {x / s, y / s, z / s}; }
    constexpr Vec3 operator-()       const noexcept { return {-x, -y, -z}; }
    constexpr Vec3& operator+=(Vec3 b) noexcept { x += b.x; y += b.y; z += b.z; return *this; }
    constexpr Vec3& operator-=(Vec3 b) noexcept { x -= b.x; y -= b.y; z -= b.z; return *this; }
    constexpr Vec3& operator*=(f32 s)  noexcept { x *= s;   y *= s;   z *= s;   return *this; }
    constexpr bool operator==(const Vec3& o) const noexcept { return x == o.x && y == o.y && z == o.z; }
};

struct Vec4 {
    f32 x{}, y{}, z{}, w{};
    constexpr bool operator==(const Vec4& o) const noexcept {
        return x == o.x && y == o.y && z == o.z && w == o.w;
    }
};

// ─── free functions ─────────────────────────────────────────────────
constexpr f32 dot(Vec3 a, Vec3 b) noexcept {
    return a.x*b.x + a.y*b.y + a.z*b.z;
}
constexpr Vec3 cross(Vec3 a, Vec3 b) noexcept {
    return { a.y*b.z - a.z*b.y,
             a.z*b.x - a.x*b.z,
             a.x*b.y - a.y*b.x };
}
constexpr f32 length_sq(Vec3 a) noexcept { return dot(a, a); }

inline f32 length(Vec3 a) noexcept { return std::sqrt(length_sq(a)); }

inline Vec3 normalize(Vec3 a) noexcept {
    return a * (1.0f / length(a));
}

constexpr Vec3 lerp(Vec3 a, Vec3 b, f32 t) noexcept {
    return a * (1.0f - t) + b * t;
}

// Mirror reflection: v - 2(v·n)n. v any length; n must be unit.
constexpr Vec3 reflect(Vec3 v, Vec3 n) noexcept {
    return v - n * (2.0f * dot(v, n));
}

// Snell refraction. uv must be unit. eta_ratio = n_in / n_out.
// Caller must have verified non-TIR before calling.
inline Vec3 refract(Vec3 uv, Vec3 n, f32 eta_ratio) noexcept {
    f32 cos_theta = dot(-uv, n);
    if (cos_theta > 1.0f) cos_theta = 1.0f;
    const Vec3 r_perp        = (uv + n * cos_theta) * eta_ratio;
    f32        parallel_lsq  = 1.0f - length_sq(r_perp);
    if (parallel_lsq < 0.0f) parallel_lsq = 0.0f;
    const Vec3 r_parallel    = n * (-std::sqrt(parallel_lsq));
    return r_perp + r_parallel;
}

// All components below eps (degenerate Lambertian scatter guard).
constexpr bool near_zero(Vec3 v) noexcept {
    constexpr f32 eps = 1e-8f;
    auto abs_f = [](f32 x) constexpr { return x < 0 ? -x : x; };
    return abs_f(v.x) < eps && abs_f(v.y) < eps && abs_f(v.z) < eps;
}

}  // namespace sotark
