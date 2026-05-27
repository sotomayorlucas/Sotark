#include "doctest.h"

import sotark.common;

using namespace sotark;

TEST_CASE("Vec3 basic ops") {
    const Vec3 a{1, 2, 3};
    const Vec3 b{4, 5, 6};

    CHECK((a + b) == Vec3{5, 7, 9});
    CHECK((a - b) == Vec3{-3, -3, -3});
    CHECK((a * 2.0f) == Vec3{2, 4, 6});
    CHECK((-a) == Vec3{-1, -2, -3});
    CHECK(dot(a, b) == doctest::Approx(32.0f));
    CHECK(cross(a, b) == Vec3{-3, 6, -3});
    CHECK(length_sq(Vec3{3, 4, 0}) == doctest::Approx(25.0f));
    CHECK(length(Vec3{3, 4, 0})    == doctest::Approx(5.0f));
}

TEST_CASE("Vec3 normalize") {
    const Vec3 n = normalize(Vec3{3, 4, 0});
    CHECK(length(n) == doctest::Approx(1.0f).epsilon(1e-6f));
    CHECK(n.x == doctest::Approx(0.6f));
    CHECK(n.y == doctest::Approx(0.8f));
    CHECK(n.z == doctest::Approx(0.0f));
}

TEST_CASE("Vec3 lerp / reflect") {
    CHECK(lerp(Vec3{0, 0, 0}, Vec3{10, 10, 10}, 0.5f) == Vec3{5, 5, 5});

    // Reflect a downward ray off horizontal floor (normal +y) → +y component.
    const Vec3 r = reflect(Vec3{0, -1, 0}, Vec3{0, 1, 0});
    CHECK(r == Vec3{0, 1, 0});
}

TEST_CASE("Vec3 near_zero") {
    CHECK(near_zero(Vec3{0, 0, 0}));
    CHECK(near_zero(Vec3{1e-9f, -1e-9f, 0}));
    CHECK_FALSE(near_zero(Vec3{1e-4f, 0, 0}));
}

TEST_CASE("Vec3 constexpr") {
    constexpr Vec3 a{1, 2, 3};
    constexpr Vec3 b{4, 5, 6};
    static_assert((a + b) == Vec3{5, 7, 9});
    static_assert(dot(a, b) == 32.0f);
}
