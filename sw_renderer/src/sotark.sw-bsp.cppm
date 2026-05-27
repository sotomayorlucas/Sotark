module;

#include <span>
#include <functional>
#include <array>

export module sotark.sw:bsp;

import sotark.common;
import :texture;

export namespace sotark::sw {

struct BspFace {
    std::array<Vec3, 4> verts{};
    std::array<Vec2, 4> uvs{};
    TexSampleFn         tex{nullptr};
};

struct BspNode {
    Plane plane{};
    int   front{0};     // + = interior idx; - = -(leaf_id + 1)
    int   back{0};
};

struct BspLeaf {
    std::span<const int> face_indices;
};

struct Bsp {
    std::span<const BspFace> faces;
    std::span<const BspNode> nodes;
    std::span<const BspLeaf> leaves;
};

// std::function callback (could also be a concept-based template; std::function
// is fine here, the callback runs once per face which is rare).
using BspFaceCb = std::function<void(const BspFace&)>;

inline void bsp_walk(const Bsp& bsp, int idx, Vec3 camera, const BspFaceCb& cb) {
    if (idx < 0) {
        const int leaf_id = -idx - 1;
        for (int fi : bsp.leaves[leaf_id].face_indices) cb(bsp.faces[fi]);
        return;
    }
    const BspNode& node = bsp.nodes[idx];
    const PlaneSide side = classify(node.plane, camera);
    if (static_cast<int>(side) >= 0) {
        bsp_walk(bsp, node.front, camera, cb);
        bsp_walk(bsp, node.back,  camera, cb);
    } else {
        bsp_walk(bsp, node.back,  camera, cb);
        bsp_walk(bsp, node.front, camera, cb);
    }
}

}  // namespace sotark::sw
