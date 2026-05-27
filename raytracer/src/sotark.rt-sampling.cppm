export module sotark.rt:sampling;

import sotark.common;

export namespace sotark::rt {

// Rejection sampling: throw points in [-1,1]^3 until one falls in unit sphere.
// ~2 attempts on average (4/3·π / 8). Faster than the trig approach in practice.
inline Vec3 rng_in_unit_sphere(Pcg32& r) noexcept {
    for (;;) {
        const Vec3 p{
            r.float01() * 2.0f - 1.0f,
            r.float01() * 2.0f - 1.0f,
            r.float01() * 2.0f - 1.0f,
        };
        if (length_sq(p) < 1.0f) return p;
    }
}

// Uniformly-distributed unit vector on the sphere.
inline Vec3 rng_unit_vec3(Pcg32& r) noexcept {
    return normalize(rng_in_unit_sphere(r));
}

// Unit vector in the hemisphere oriented by `normal`.
inline Vec3 rng_hemisphere(Pcg32& r, Vec3 normal) noexcept {
    const Vec3 v = rng_unit_vec3(r);
    return dot(v, normal) > 0.0f ? v : -v;
}

// Random point in unit disk in XY plane (z=0). Used by camera defocus.
inline Vec3 rng_in_unit_disk(Pcg32& r) noexcept {
    for (;;) {
        const f32 x = r.float01() * 2.0f - 1.0f;
        const f32 y = r.float01() * 2.0f - 1.0f;
        if (x*x + y*y < 1.0f) return Vec3{x, y, 0.0f};
    }
}

}  // namespace sotark::rt
