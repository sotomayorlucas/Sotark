export module sotark.common:plane;

import :types;
import :vec;

export namespace sotark {

// Plane in 3D: n · p + d = 0 ("Hessian normal" form).
// `normal` must be unit so that `d` = signed distance to origin.
struct Plane {
    Vec3 normal{};
    f32  d{};

    static constexpr Plane from_point_normal(Vec3 point, Vec3 n) noexcept {
        return { n, -dot(n, point) };
    }

    constexpr f32 distance(Vec3 pt) const noexcept {
        return dot(normal, pt) + d;
    }
};

enum class PlaneSide { Behind = -1, On = 0, Front = 1 };

constexpr PlaneSide classify(Plane pl, Vec3 pt) noexcept {
    const f32 d = pl.distance(pt);
    if (d >  1e-4f) return PlaneSide::Front;
    if (d < -1e-4f) return PlaneSide::Behind;
    return PlaneSide::On;
}

}  // namespace sotark
