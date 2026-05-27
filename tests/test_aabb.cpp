#include "doctest.h"
#include <array>

import sotark.common;

using namespace sotark;

TEST_CASE("Aabb from_min_max") {
    const Aabb b{Vec3{-1, -2, -3}, Vec3{1, 2, 3}};
    CHECK(b.min == Vec3{-1, -2, -3});
    CHECK(b.max == Vec3{1, 2, 3});
}

TEST_CASE("Aabb translate") {
    const Aabb b{Vec3{0, 0, 0}, Vec3{1, 1, 1}};
    const Aabb t = b.translated(Vec3{10, 20, 30});
    CHECK(t.min == Vec3{10, 20, 30});
    CHECK(t.max == Vec3{11, 21, 31});
}

TEST_CASE("Aabb from_points") {
    const std::array<Vec3, 4> pts{ Vec3{-1, 0, 5}, Vec3{2, -3, 1}, Vec3{0, 4, -2}, Vec3{3, 1, 0} };
    const Aabb b = Aabb::from_points(pts);
    CHECK(b.min == Vec3{-1, -3, -2});
    CHECK(b.max == Vec3{3, 4, 5});
}

TEST_CASE("Aabb union_of") {
    const Aabb a{Vec3{0, 0, 0}, Vec3{1, 1, 1}};
    const Aabb b{Vec3{-2, 3, 0}, Vec3{0, 5, 2}};
    const Aabb u = union_of(a, b);
    CHECK(u.min == Vec3{-2, 0, 0});
    CHECK(u.max == Vec3{1, 5, 2});
}
