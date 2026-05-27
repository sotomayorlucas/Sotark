module;

#include <array>

export module sotark.sw:frustum;

import sotark.common;

export namespace sotark::sw {

// 6 planes in world space, extracted from P*V (proj * view) matrix.
// Convention: a*x + b*y + c*z + d >= 0 = inside.
// Order: left, right, bottom, top, near, far.
struct Frustum {
    std::array<std::array<f32, 4>, 6> planes{};
};

inline Frustum frustum_from_pv(Mat4 m) noexcept {
    Frustum f{};
    for (int j = 0; j < 4; ++j) {
        const f32 r0 = m.m[j*4 + 0];
        const f32 r1 = m.m[j*4 + 1];
        const f32 r2 = m.m[j*4 + 2];
        const f32 r3 = m.m[j*4 + 3];
        f.planes[0][j] = r3 + r0;   // left
        f.planes[1][j] = r3 - r0;   // right
        f.planes[2][j] = r3 + r1;   // bottom
        f.planes[3][j] = r3 - r1;   // top
        f.planes[4][j] = r3 + r2;   // near
        f.planes[5][j] = r3 - r2;   // far
    }
    return f;
}

// True if the AABB is COMPLETELY outside the frustum (so it can be culled).
// "Positive vertex" test per plane.
inline bool frustum_cull_aabb(const Frustum& f, Aabb box) noexcept {
    for (const auto& pl : f.planes) {
        const f32 a = pl[0], b = pl[1], c = pl[2], d = pl[3];
        const f32 px = (a > 0.0f) ? box.max.x : box.min.x;
        const f32 py = (b > 0.0f) ? box.max.y : box.min.y;
        const f32 pz = (c > 0.0f) ? box.max.z : box.min.z;
        if (a*px + b*py + c*pz + d < 0.0f) return true;
    }
    return false;
}

}  // namespace sotark::sw
