module;

#include <span>
#include <vector>
#include <algorithm>
#include <ranges>

export module sotark.rt:bvh;

import sotark.common;
import :hittable;

export namespace sotark::rt {

namespace detail {

constexpr int longest_axis(Aabb box) noexcept {
    const f32 dx = box.max.x - box.min.x;
    const f32 dy = box.max.y - box.min.y;
    const f32 dz = box.max.z - box.min.z;
    if (dx >= dy && dx >= dz) return 0;
    if (dy >= dz)              return 1;
    return 2;
}

constexpr f32 centroid_on_axis(const Hittable& h, int axis) noexcept {
    switch (axis) {
        case 0:  return h.bbox.min.x + h.bbox.max.x;
        case 1:  return h.bbox.min.y + h.bbox.max.y;
        default: return h.bbox.min.z + h.bbox.max.z;
    }
}

}  // namespace detail

// Median-split BVH. Reorders `items` in-place; appends interior nodes to
// `arena` and returns the root.
//
// `arena` must have spare capacity. With n input items, the recursion creates
// up to n-1 interior nodes — reserving n is safe. We use a vector reference
// rather than a span so the caller doesn't need to pre-size.
inline const Hittable*
bvh_build(std::span<const Hittable*> items, std::vector<Hittable>& arena) {
    sotark::check(!items.empty(), "bvh_build: empty items");
    if (items.size() == 1) return items[0];

    // Combined bbox to pick partition axis.
    Aabb combined = items[0]->bbox;
    for (const Hittable* h : items.subspan(1)) {
        combined = union_of(combined, h->bbox);
    }
    const int axis = detail::longest_axis(combined);

    std::ranges::sort(items, [axis](const Hittable* a, const Hittable* b) noexcept {
        return detail::centroid_on_axis(*a, axis) < detail::centroid_on_axis(*b, axis);
    });

    const auto mid = items.size() / 2;
    const Hittable* left  = bvh_build(items.first(mid),       arena);
    const Hittable* right = bvh_build(items.subspan(mid),     arena);

    arena.push_back(Hittable{
        BvhNode{left, right},
        union_of(left->bbox, right->bbox),
    });
    return &arena.back();
}

}  // namespace sotark::rt
