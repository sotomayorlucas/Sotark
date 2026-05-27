#include "doctest.h"

import sotark.common;

using namespace sotark;

TEST_CASE("Plane from_point_normal") {
    const Plane p = Plane::from_point_normal(Vec3{0, 5, 0}, Vec3{0, 1, 0});
    CHECK(p.normal == Vec3{0, 1, 0});
    CHECK(p.d == doctest::Approx(-5.0f));
}

TEST_CASE("Plane distance") {
    const Plane p = Plane::from_point_normal(Vec3{0, 0, 0}, Vec3{0, 1, 0});
    CHECK(p.distance(Vec3{1, 3, 5})  == doctest::Approx(3.0f));
    CHECK(p.distance(Vec3{1, -2, 5}) == doctest::Approx(-2.0f));
    CHECK(p.distance(Vec3{0, 0, 0})  == doctest::Approx(0.0f));
}

TEST_CASE("Plane classify") {
    const Plane p = Plane::from_point_normal(Vec3{0, 0, 0}, Vec3{0, 1, 0});
    CHECK(classify(p, Vec3{1, 1, 1})    == PlaneSide::Front);
    CHECK(classify(p, Vec3{1, -1, 1})   == PlaneSide::Behind);
    CHECK(classify(p, Vec3{1,  0, 1})   == PlaneSide::On);
}
