export module sotark.rt:hit;

import sotark.common;

export namespace sotark::rt {

struct HitRecord {
    Vec3 p{};            // point of intersection
    Vec3 normal{};       // always points AGAINST the incident ray
    f32  t{0.0f};
    bool front_face{false};
    u32  mat{0};
    f32  u{0.0f}, v{0.0f};   // texture coords ∈ [0, 1] for sphere/quad; 0 for tri

    // Orient `normal` against incident ray. `outward_normal` must be unit-length.
    constexpr void set_face(Vec3 ray_dir, Vec3 outward_normal) noexcept {
        front_face = dot(ray_dir, outward_normal) < 0.0f;
        normal     = front_face ? outward_normal : -outward_normal;
    }
};

}  // namespace sotark::rt
