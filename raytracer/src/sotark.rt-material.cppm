module;

#include <variant>
#include <optional>
#include <cmath>

export module sotark.rt:material;

import sotark.common;
import :ray;
import :hit;
import :sampling;

export namespace sotark::rt {

struct Lambertian { Vec3 albedo{}; };
struct Metal      { Vec3 albedo{}; f32 fuzz{0.0f}; };
struct Dielectric { f32  ior{1.5f}; };
struct Emissive   { Vec3 emit{}; };
struct TexturedLambertian {
    const Image* tex{nullptr};
    f32 u_scale{1.0f}, v_scale{1.0f};
};

using MaterialKind = std::variant<Lambertian, Metal, Dielectric, Emissive, TexturedLambertian>;

struct Material {
    MaterialKind kind;
};

struct ScatterResult {
    Ray  scattered;
    Vec3 attenuation;
};

namespace detail {

inline std::optional<ScatterResult>
scatter_lambertian(const Lambertian& l, const HitRecord& rec, Pcg32& rng) noexcept {
    Vec3 dir = rec.normal + rng_unit_vec3(rng);
    if (near_zero(dir)) dir = rec.normal;
    return ScatterResult{ Ray{rec.p, dir}, l.albedo };
}

inline std::optional<ScatterResult>
scatter_textured(const TexturedLambertian& tl, const HitRecord& rec, Pcg32& rng) noexcept {
    Vec3 dir = rec.normal + rng_unit_vec3(rng);
    if (near_zero(dir)) dir = rec.normal;
    const Vec3 albedo = sample_bilinear(*tl.tex,
                                         rec.u * tl.u_scale,
                                         rec.v * tl.v_scale);
    return ScatterResult{ Ray{rec.p, dir}, albedo };
}

inline std::optional<ScatterResult>
scatter_metal(const Metal& m, Ray in, const HitRecord& rec, Pcg32& rng) noexcept {
    const Vec3 unit       = normalize(in.dir);
    const Vec3 reflected  = reflect(unit, rec.normal);
    const Vec3 scatter    = reflected + rng_in_unit_sphere(rng) * m.fuzz;
    if (dot(scatter, rec.normal) <= 0.0f) return std::nullopt;   // absorbed
    return ScatterResult{ Ray{rec.p, scatter}, m.albedo };
}

// Schlick Fresnel approximation. r0 = ((1-η)/(1+η))²; F(θ) ≈ r0 + (1-r0)·(1-cosθ)⁵.
constexpr f32 schlick_reflectance(f32 cosine, f32 ref_idx) noexcept {
    f32 r0 = (1.0f - ref_idx) / (1.0f + ref_idx);
    r0 = r0 * r0;
    const f32 oc = 1.0f - cosine;
    return r0 + (1.0f - r0) * oc * oc * oc * oc * oc;
}

inline std::optional<ScatterResult>
scatter_dielectric(const Dielectric& d, Ray in, const HitRecord& rec, Pcg32& rng) noexcept {
    const Vec3 unit = normalize(in.dir);
    const f32  ri   = rec.front_face ? (1.0f / d.ior) : d.ior;
    f32  cos_t      = dot(-unit, rec.normal);
    if (cos_t > 1.0f) cos_t = 1.0f;
    const f32 sin_t = std::sqrt(1.0f - cos_t*cos_t);

    const bool tir     = (ri * sin_t) > 1.0f;
    const bool do_refl = tir || (schlick_reflectance(cos_t, ri) > rng.float01());
    const Vec3 dir     = do_refl ? reflect(unit, rec.normal)
                                  : refract(unit, rec.normal, ri);
    return ScatterResult{ Ray{rec.p, dir}, Vec3{1.0f, 1.0f, 1.0f} };
}

}  // namespace detail

inline std::optional<ScatterResult>
scatter(const Material& m, Ray in, const HitRecord& rec, Pcg32& rng) {
    return std::visit([&](const auto& mk) -> std::optional<ScatterResult> {
        using T = std::decay_t<decltype(mk)>;
        if constexpr (std::is_same_v<T, Lambertian>)         return detail::scatter_lambertian(mk, rec, rng);
        else if constexpr (std::is_same_v<T, Metal>)         return detail::scatter_metal(mk, in, rec, rng);
        else if constexpr (std::is_same_v<T, Dielectric>)    return detail::scatter_dielectric(mk, in, rec, rng);
        else if constexpr (std::is_same_v<T, Emissive>)      return std::nullopt;
        else if constexpr (std::is_same_v<T, TexturedLambertian>) return detail::scatter_textured(mk, rec, rng);
    }, m.kind);
}

constexpr Vec3 emitted(const Material& m) noexcept {
    return std::visit([](const auto& mk) -> Vec3 {
        using T = std::decay_t<decltype(mk)>;
        if constexpr (std::is_same_v<T, Emissive>) return mk.emit;
        else return Vec3{};
    }, m.kind);
}

constexpr bool is_emissive(const Material& m) noexcept {
    return std::holds_alternative<Emissive>(m.kind);
}

constexpr bool is_lambertian(const Material& m) noexcept {
    return std::holds_alternative<Lambertian>(m.kind);
}

}  // namespace sotark::rt
