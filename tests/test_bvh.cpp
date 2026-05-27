#include "doctest.h"
#include <vector>
#include <variant>
#include <limits>

import sotark.common;
import sotark.rt;

using namespace sotark;
using namespace sotark::rt;

TEST_CASE("aabb_hit: ray hits centered box") {
    const Aabb box{Vec3{-1, -1, -1}, Vec3{1, 1, 1}};
    CHECK(aabb_hit(box, Ray{Vec3{0, 0, -5}, Vec3{0, 0, 1}}, 0.001f,
                    std::numeric_limits<f32>::infinity()));
    CHECK_FALSE(aabb_hit(box, Ray{Vec3{5, 5, -5}, Vec3{0, 0, 1}}, 0.001f,
                          std::numeric_limits<f32>::infinity()));
}

TEST_CASE("hit_sphere: ray hits origin sphere") {
    const Hittable s = make_sphere(Vec3{0, 0, 0}, 1.0f, 7);
    const auto rec = hit(s, Ray{Vec3{0, 0, -5}, Vec3{0, 0, 1}}, 0.001f,
                          std::numeric_limits<f32>::infinity());
    REQUIRE(rec);
    CHECK(rec->t   == doctest::Approx(4.0f));
    CHECK(rec->mat == 7u);
    CHECK(rec->front_face);
    CHECK(rec->normal == Vec3{0, 0, -1});
}

TEST_CASE("hit_sphere: ray misses") {
    const Hittable s = make_sphere(Vec3{0, 0, 0}, 1.0f, 0);
    const auto rec = hit(s, Ray{Vec3{5, 5, -5}, Vec3{0, 0, 1}}, 0.001f,
                          std::numeric_limits<f32>::infinity());
    CHECK_FALSE(rec.has_value());
}

TEST_CASE("bvh_build over 1 sphere returns it") {
    std::vector<Hittable> spheres;
    spheres.push_back(make_sphere(Vec3{0, 0, 0}, 1.0f, 0));
    std::vector<const Hittable*> items{ &spheres[0] };
    std::vector<Hittable> arena;
    arena.reserve(8);
    const Hittable* root = bvh_build(items, arena);
    REQUIRE(root != nullptr);
    const bool is_sphere = std::holds_alternative<Sphere>(root->shape);
    CHECK(is_sphere);
}

TEST_CASE("bvh_build over 3 spheres builds 2-level tree") {
    std::vector<Hittable> spheres;
    spheres.push_back(make_sphere(Vec3{-5, 0, 0}, 0.5f, 0));
    spheres.push_back(make_sphere(Vec3{ 0, 0, 0}, 0.5f, 1));
    spheres.push_back(make_sphere(Vec3{ 5, 0, 0}, 0.5f, 2));
    std::vector<const Hittable*> items{ &spheres[0], &spheres[1], &spheres[2] };
    std::vector<Hittable> arena;
    arena.reserve(8);
    const Hittable* root = bvh_build(items, arena);
    REQUIRE(root != nullptr);
    const bool is_bvh = std::holds_alternative<BvhNode>(root->shape);
    CHECK(is_bvh);
    // Ray straight through center -> should hit middle sphere first.
    const auto rec = hit(*root, Ray{Vec3{0, 0, -5}, Vec3{0, 0, 1}}, 0.001f,
                          std::numeric_limits<f32>::infinity());
    REQUIRE(rec);
    CHECK(rec->mat == 1u);
}
