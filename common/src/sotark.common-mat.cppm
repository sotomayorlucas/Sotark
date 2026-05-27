module;

#include <array>
#include <cmath>

export module sotark.common:mat;

import :types;
import :vec;

export namespace sotark {

// Column-major mat4. Element (row r, col c) is m[c*4 + r].
// OpenGL/glm convention: v_clip = P * V * M * v_world.
struct Mat4 {
    std::array<f32, 16> m{};

    constexpr f32&       operator()(int r, int c)       noexcept { return m[c*4 + r]; }
    constexpr const f32& operator()(int r, int c) const noexcept { return m[c*4 + r]; }

    static constexpr Mat4 zero() noexcept { return Mat4{}; }

    static constexpr Mat4 identity() noexcept {
        Mat4 r{};
        r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
        return r;
    }

    static constexpr Mat4 translate(Vec3 t) noexcept {
        Mat4 r = identity();
        r.m[12] = t.x;
        r.m[13] = t.y;
        r.m[14] = t.z;
        return r;
    }

    static constexpr Mat4 scale(Vec3 s) noexcept {
        Mat4 r{};
        r.m[0]  = s.x;
        r.m[5]  = s.y;
        r.m[10] = s.z;
        r.m[15] = 1.0f;
        return r;
    }

    static Mat4 rotate_x(f32 a) noexcept {
        Mat4 r = identity();
        const f32 c = std::cos(a), s = std::sin(a);
        r.m[5] = c;   r.m[6]  = s;
        r.m[9] = -s;  r.m[10] = c;
        return r;
    }

    static Mat4 rotate_y(f32 a) noexcept {
        Mat4 r = identity();
        const f32 c = std::cos(a), s = std::sin(a);
        r.m[0] = c;   r.m[2]  = -s;
        r.m[8] = s;   r.m[10] = c;
        return r;
    }

    static Mat4 rotate_z(f32 a) noexcept {
        Mat4 r = identity();
        const f32 c = std::cos(a), s = std::sin(a);
        r.m[0] = c;   r.m[1] = s;
        r.m[4] = -s;  r.m[5] = c;
        return r;
    }

    // Right-handed perspective. Camera looks -Z. Maps -near→NDC z=-1, -far→+1.
    static Mat4 perspective(f32 fov_y_rad, f32 aspect, f32 near, f32 far) noexcept {
        Mat4 r{};
        const f32 fy = 1.0f / std::tan(fov_y_rad * 0.5f);
        r.m[0]  = fy / aspect;
        r.m[5]  = fy;
        r.m[10] = (far + near) / (near - far);
        r.m[11] = -1.0f;
        r.m[14] = (2.0f * far * near) / (near - far);
        return r;
    }

    static Mat4 look_at(Vec3 eye, Vec3 target, Vec3 up) noexcept {
        const Vec3 f = normalize(target - eye);
        const Vec3 s = normalize(cross(f, up));
        const Vec3 u = cross(s, f);
        Mat4 r = identity();
        r.m[0]  =  s.x;  r.m[4]  =  s.y;  r.m[8]   =  s.z;
        r.m[1]  =  u.x;  r.m[5]  =  u.y;  r.m[9]   =  u.z;
        r.m[2]  = -f.x;  r.m[6]  = -f.y;  r.m[10]  = -f.z;
        r.m[12] = -dot(s, eye);
        r.m[13] = -dot(u, eye);
        r.m[14] =  dot(f, eye);
        return r;
    }
};

// Mat4 * Mat4: r = a * b. Apply b first, then a.
constexpr Mat4 operator*(const Mat4& a, const Mat4& b) noexcept {
    Mat4 r{};
    for (int j = 0; j < 4; ++j) {
        for (int i = 0; i < 4; ++i) {
            f32 s = 0.0f;
            for (int k = 0; k < 4; ++k) {
                s += a.m[k*4 + i] * b.m[j*4 + k];
            }
            r.m[j*4 + i] = s;
        }
    }
    return r;
}

constexpr Vec4 operator*(const Mat4& a, Vec4 v) noexcept {
    return {
        a.m[0]*v.x + a.m[4]*v.y + a.m[8] *v.z + a.m[12]*v.w,
        a.m[1]*v.x + a.m[5]*v.y + a.m[9] *v.z + a.m[13]*v.w,
        a.m[2]*v.x + a.m[6]*v.y + a.m[10]*v.z + a.m[14]*v.w,
        a.m[3]*v.x + a.m[7]*v.y + a.m[11]*v.z + a.m[15]*v.w,
    };
}

}  // namespace sotark
