module;

#include <variant>
#include <optional>
#include <span>
#include <cmath>
#include <array>

export module sotark.rt:hittable;

import sotark.common;
import :ray;
import :hit;

export namespace sotark::rt {

struct Hittable;  // forward decl — Bvh & List hold pointers to it

struct Sphere {
    Vec3 center{};
    f32  radius{0.0f};
    u32  mat{0};
};

struct Quad {
    Vec3 Q{};          // origin
    Vec3 u{}, v{};     // adjacent edges
    Vec3 normal{};     // unit normal
    f32  D{0.0f};      // n·p = D
    Vec3 w{};          // (u×v) / |u×v|²
    u32  mat{0};
};

struct Tri {
    Vec3 v0{}, v1{}, v2{};
    u32  mat{0};
};

struct List {
    std::span<const Hittable* const> items;
};

struct BvhNode {
    const Hittable* left{nullptr};
    const Hittable* right{nullptr};
};

using HittableShape = std::variant<Sphere, Quad, Tri, List, BvhNode>;

struct Hittable {
    HittableShape shape;
    Aabb          bbox;
};

// ─── constructors that auto-fill the bbox ───────────────────────────
constexpr Hittable make_sphere(Vec3 center, f32 radius, u32 mat) noexcept {
    return Hittable{
        .shape = Sphere{center, radius, mat},
        .bbox  = Aabb{ Vec3{center.x - radius, center.y - radius, center.z - radius},
                       Vec3{center.x + radius, center.y + radius, center.z + radius} },
    };
}

inline Hittable make_quad(Vec3 Q, Vec3 u_edge, Vec3 v_edge, u32 mat) noexcept {
    const Vec3 n     = cross(u_edge, v_edge);
    const f32  n_lsq = length_sq(n);
    const Vec3 norm  = n * (1.0f / std::sqrt(n_lsq));
    const f32  D     = dot(norm, Q);
    const Vec3 w     = n * (1.0f / n_lsq);

    const std::array<Vec3, 4> corners{ Q, Q + u_edge, Q + v_edge, Q + u_edge + v_edge };
    Aabb box = Aabb::from_points(corners);
    constexpr f32 eps = 1e-4f;
    if (box.max.x - box.min.x < eps) { box.min.x -= eps; box.max.x += eps; }
    if (box.max.y - box.min.y < eps) { box.min.y -= eps; box.max.y += eps; }
    if (box.max.z - box.min.z < eps) { box.min.z -= eps; box.max.z += eps; }

    return Hittable{ Quad{Q, u_edge, v_edge, norm, D, w, mat}, box };
}

inline Hittable make_triangle(Vec3 v0, Vec3 v1, Vec3 v2, u32 mat) noexcept {
    const std::array<Vec3, 3> corners{ v0, v1, v2 };
    Aabb box = Aabb::from_points(corners);
    constexpr f32 eps = 1e-4f;
    if (box.max.x - box.min.x < eps) { box.min.x -= eps; box.max.x += eps; }
    if (box.max.y - box.min.y < eps) { box.min.y -= eps; box.max.y += eps; }
    if (box.max.z - box.min.z < eps) { box.min.z -= eps; box.max.z += eps; }
    return Hittable{ Tri{v0, v1, v2, mat}, box };
}

// ─── ray-vs-AABB slab test ──────────────────────────────────────────
constexpr bool aabb_hit(Aabb box, Ray r, f32 t_min, f32 t_max) noexcept {
    {
        const f32 inv = 1.0f / r.dir.x;
        f32 t0 = (box.min.x - r.origin.x) * inv;
        f32 t1 = (box.max.x - r.origin.x) * inv;
        if (t0 > t1) { const f32 t = t0; t0 = t1; t1 = t; }
        if (t0 > t_min) t_min = t0;
        if (t1 < t_max) t_max = t1;
        if (t_max <= t_min) return false;
    }
    {
        const f32 inv = 1.0f / r.dir.y;
        f32 t0 = (box.min.y - r.origin.y) * inv;
        f32 t1 = (box.max.y - r.origin.y) * inv;
        if (t0 > t1) { const f32 t = t0; t0 = t1; t1 = t; }
        if (t0 > t_min) t_min = t0;
        if (t1 < t_max) t_max = t1;
        if (t_max <= t_min) return false;
    }
    {
        const f32 inv = 1.0f / r.dir.z;
        f32 t0 = (box.min.z - r.origin.z) * inv;
        f32 t1 = (box.max.z - r.origin.z) * inv;
        if (t0 > t1) { const f32 t = t0; t0 = t1; t1 = t; }
        if (t0 > t_min) t_min = t0;
        if (t1 < t_max) t_max = t1;
        if (t_max <= t_min) return false;
    }
    return true;
}

// ─── intersection routines (per shape) ──────────────────────────────
std::optional<HitRecord> hit(const Hittable& h, Ray r, f32 t_min, f32 t_max);

namespace detail {

inline std::optional<HitRecord> hit_sphere(const Sphere& s, Ray r, f32 t_min, f32 t_max) noexcept {
    const Vec3 oc   = r.origin - s.center;
    const f32  a    = dot(r.dir, r.dir);
    const f32  hf   = dot(oc, r.dir);
    const f32  c    = dot(oc, oc) - s.radius * s.radius;
    const f32  disc = hf*hf - a*c;
    if (disc < 0.0f) return std::nullopt;

    const f32 sqrt_disc = std::sqrt(disc);
    f32 root            = (-hf - sqrt_disc) / a;
    if (root <= t_min || root >= t_max) {
        root = (-hf + sqrt_disc) / a;
        if (root <= t_min || root >= t_max) return std::nullopt;
    }

    HitRecord rec{};
    rec.t   = root;
    rec.p   = r.at(root);
    rec.mat = s.mat;
    const Vec3 outward = (rec.p - s.center) * (1.0f / s.radius);
    rec.set_face(r.dir, outward);
    // Spherical UV: theta ∈ [0,π] from +y to -y; phi ∈ [0,2π] from -x.
    const f32 theta = std::acos(-outward.y);
    const f32 phi   = std::atan2(-outward.z, outward.x) + pi_f;
    rec.u = phi   / two_pi_f;
    rec.v = theta / pi_f;
    return rec;
}

// Möller-Trumbore triangle intersection.
inline std::optional<HitRecord> hit_tri(const Tri& t, Ray r, f32 t_min, f32 t_max) noexcept {
    const Vec3 e1 = t.v1 - t.v0;
    const Vec3 e2 = t.v2 - t.v0;
    const Vec3 pvec = cross(r.dir, e2);
    const f32  det  = dot(e1, pvec);
    if (std::abs(det) < 1e-8f) return std::nullopt;
    const f32 inv_det = 1.0f / det;

    const Vec3 tvec = r.origin - t.v0;
    const f32  u    = inv_det * dot(tvec, pvec);
    if (u < 0.0f || u > 1.0f) return std::nullopt;

    const Vec3 qvec = cross(tvec, e1);
    const f32  v    = inv_det * dot(r.dir, qvec);
    if (v < 0.0f || (u + v) > 1.0f) return std::nullopt;

    const f32 tt = inv_det * dot(e2, qvec);
    if (tt <= t_min || tt >= t_max) return std::nullopt;

    HitRecord rec{};
    rec.t   = tt;
    rec.p   = r.at(tt);
    rec.mat = t.mat;
    rec.set_face(r.dir, normalize(cross(e1, e2)));
    return rec;
}

inline std::optional<HitRecord> hit_quad(const Quad& q, Ray r, f32 t_min, f32 t_max) noexcept {
    const f32 denom = dot(q.normal, r.dir);
    if (std::abs(denom) < 1e-8f) return std::nullopt;
    const f32 tt = (q.D - dot(q.normal, r.origin)) / denom;
    if (tt <= t_min || tt >= t_max) return std::nullopt;

    const Vec3 P       = r.at(tt);
    const Vec3 hit_vec = P - q.Q;
    const f32  alpha   = dot(q.w, cross(hit_vec, q.v));
    const f32  beta    = dot(q.w, cross(q.u, hit_vec));
    if (alpha < 0.0f || alpha > 1.0f || beta < 0.0f || beta > 1.0f) return std::nullopt;

    HitRecord rec{};
    rec.t   = tt;
    rec.p   = P;
    rec.mat = q.mat;
    rec.u   = alpha;
    rec.v   = beta;
    rec.set_face(r.dir, q.normal);
    return rec;
}

inline std::optional<HitRecord> hit_list(const List& l, Ray r, f32 t_min, f32 t_max) noexcept {
    std::optional<HitRecord> best;
    f32 closest = t_max;
    for (const Hittable* item : l.items) {
        if (auto rec = hit(*item, r, t_min, closest); rec) {
            closest = rec->t;
            best    = rec;
        }
    }
    return best;
}

inline std::optional<HitRecord> hit_bvh(const BvhNode& n, Aabb bbox, Ray r, f32 t_min, f32 t_max) noexcept {
    if (!aabb_hit(bbox, r, t_min, t_max)) return std::nullopt;

    auto left_rec = hit(*n.left, r, t_min, t_max);
    const f32 closest = left_rec ? left_rec->t : t_max;
    auto right_rec = hit(*n.right, r, t_min, closest);
    return right_rec ? right_rec : left_rec;
}

}  // namespace detail

// Outer dispatcher — std::visit over the variant.
inline std::optional<HitRecord> hit(const Hittable& h, Ray r, f32 t_min, f32 t_max) {
    return std::visit([&](const auto& shape) -> std::optional<HitRecord> {
        using T = std::decay_t<decltype(shape)>;
        if constexpr (std::is_same_v<T, Sphere>)       return detail::hit_sphere(shape, r, t_min, t_max);
        else if constexpr (std::is_same_v<T, Tri>)     return detail::hit_tri(shape, r, t_min, t_max);
        else if constexpr (std::is_same_v<T, Quad>)    return detail::hit_quad(shape, r, t_min, t_max);
        else if constexpr (std::is_same_v<T, List>)    return detail::hit_list(shape, r, t_min, t_max);
        else if constexpr (std::is_same_v<T, BvhNode>) return detail::hit_bvh(shape, h.bbox, r, t_min, t_max);
    }, h.shape);
}

}  // namespace sotark::rt
