#include "doctest.h"

import sotark.common;

using namespace sotark;

TEST_CASE("Mat4 identity * vec") {
    const Mat4 I = Mat4::identity();
    const Vec4 v{1, 2, 3, 1};
    const Vec4 r = I * v;
    CHECK(r == v);
}

TEST_CASE("Mat4 translate") {
    const Mat4 T = Mat4::translate(Vec3{10, 20, 30});
    const Vec4 r = T * Vec4{1, 2, 3, 1};
    CHECK(r == Vec4{11, 22, 33, 1});
}

TEST_CASE("Mat4 scale") {
    const Mat4 S = Mat4::scale(Vec3{2, 3, 4});
    const Vec4 r = S * Vec4{1, 1, 1, 1};
    CHECK(r == Vec4{2, 3, 4, 1});
}

TEST_CASE("Mat4 mul: A * I == A") {
    const Mat4 T = Mat4::translate(Vec3{1, 2, 3});
    const Mat4 I = Mat4::identity();
    const Mat4 R = T * I;
    for (int i = 0; i < 16; ++i) {
        CHECK(R.m[i] == doctest::Approx(T.m[i]));
    }
}

TEST_CASE("Mat4 look_at: eye → target gives -Z forward") {
    const Mat4 V = Mat4::look_at(Vec3{0, 0, 5}, Vec3{0, 0, 0}, Vec3{0, 1, 0});
    // Origin transformed: (0, 0, 0, 1) → (0, 0, -5, 1) (camera-space z negative
    // = in front of camera, since right-handed -Z is forward).
    const Vec4 r = V * Vec4{0, 0, 0, 1};
    CHECK(r.x == doctest::Approx(0.0f).epsilon(1e-5f));
    CHECK(r.y == doctest::Approx(0.0f).epsilon(1e-5f));
    CHECK(r.z == doctest::Approx(-5.0f).epsilon(1e-5f));
}
