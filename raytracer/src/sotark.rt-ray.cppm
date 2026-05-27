export module sotark.rt:ray;

import sotark.common;

export namespace sotark::rt {

struct Ray {
    Vec3 origin{};
    Vec3 dir{};

    constexpr Vec3 at(f32 t) const noexcept { return origin + dir * t; }
};

}  // namespace sotark::rt
