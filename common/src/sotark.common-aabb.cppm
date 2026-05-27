module;

#include <span>
#include <algorithm>

export module sotark.common:aabb;

import :types;
import :vec;

export namespace sotark {

// Inclusive convention: point p is inside if min <= p <= max componentwise.
struct Aabb {
    Vec3 min{}, max{};

    constexpr Aabb() = default;
    constexpr Aabb(Vec3 mn, Vec3 mx) noexcept : min(mn), max(mx) {}

    constexpr Aabb translated(Vec3 t) const noexcept { return {min + t, max + t}; }

    static constexpr Aabb from_points(std::span<const Vec3> pts) noexcept {
        Aabb b{pts.front(), pts.front()};
        for (const Vec3& p : pts.subspan(1)) {
            b.min.x = std::min(b.min.x, p.x);
            b.min.y = std::min(b.min.y, p.y);
            b.min.z = std::min(b.min.z, p.z);
            b.max.x = std::max(b.max.x, p.x);
            b.max.y = std::max(b.max.y, p.y);
            b.max.z = std::max(b.max.z, p.z);
        }
        return b;
    }
};

constexpr Aabb union_of(Aabb a, Aabb b) noexcept {
    return {
        Vec3{ std::min(a.min.x, b.min.x), std::min(a.min.y, b.min.y), std::min(a.min.z, b.min.z) },
        Vec3{ std::max(a.max.x, b.max.x), std::max(a.max.y, b.max.y), std::max(a.max.z, b.max.z) },
    };
}

}  // namespace sotark
