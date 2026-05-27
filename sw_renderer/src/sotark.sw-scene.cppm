module;

#include <vector>
#include <span>
#include <array>

export module sotark.sw:scene;

import sotark.common;
import :bsp;
import :lightmap;
import :texture;

export namespace sotark::sw {

inline constexpr int SCENE_MAX_FACES = 64;
inline constexpr int SCENE_LM_DIM    = 32;

// Mutable scene for the editor. Vectors with reserved capacity match the
// SCENE_MAX_FACES limit of the original C99.
class Scene {
public:
    Scene() {
        faces_.reserve(SCENE_MAX_FACES);
        lightmaps_.reserve(SCENE_MAX_FACES);
    }

    int  n_faces() const noexcept { return static_cast<int>(faces_.size()); }
    bool full()    const noexcept { return n_faces() >= SCENE_MAX_FACES; }

    std::span<const BspFace>  faces()      const noexcept { return faces_; }
    std::span<BspFace>        faces()            noexcept { return faces_; }
    std::span<const Lightmap> lightmaps()  const noexcept { return lightmaps_; }
    std::span<Lightmap>       lightmaps()        noexcept { return lightmaps_; }

    int add_face(BspFace f) {
        if (full()) return -1;
        faces_.push_back(std::move(f));
        lightmaps_.emplace_back(SCENE_LM_DIM, SCENE_LM_DIM);
        return static_cast<int>(faces_.size()) - 1;
    }

    void remove_face(int idx) {
        if (idx < 0 || idx >= n_faces()) return;
        const int last = n_faces() - 1;
        if (idx != last) {
            faces_[idx]     = std::move(faces_[last]);
            lightmaps_[idx] = std::move(lightmaps_[last]);
        }
        faces_.pop_back();
        lightmaps_.pop_back();
    }

    int add_floor(Vec3 center, f32 size, TexSampleFn tex);
    int add_cube (Vec3 center, f32 size, TexSampleFn tex);
    int add_pillar(Vec3 base_center, f32 width, f32 height, TexSampleFn tex);

    void load_default();
    void bake_lightmaps(Vec3 light_pos, f32 intensity, f32 ambient);

private:
    std::vector<BspFace>  faces_;
    std::vector<Lightmap> lightmaps_;
};

inline int Scene::add_floor(Vec3 c, f32 size, TexSampleFn tex) {
    const f32 h = size * 0.5f;
    return add_face(BspFace{
        {{ { c.x - h, c.y, c.z + h },
           { c.x + h, c.y, c.z + h },
           { c.x + h, c.y, c.z - h },
           { c.x - h, c.y, c.z - h } }},
        {{ {-h,  h}, { h,  h}, { h, -h}, {-h, -h} }},
        tex,
    });
}

inline int Scene::add_cube(Vec3 c, f32 size, TexSampleFn tex) {
    const f32 h = size * 0.5f;
    const Vec3 mn{ c.x - h, c.y - h, c.z - h };
    const Vec3 mx{ c.x + h, c.y + h, c.z + h };
    const int first = n_faces();
    // Top, Bottom, North, South, East, West
    add_face({{{ {mn.x, mx.y, mx.z}, {mx.x, mx.y, mx.z},
                  {mx.x, mx.y, mn.z}, {mn.x, mx.y, mn.z} }},
               {{ {mn.x, mx.z}, {mx.x, mx.z}, {mx.x, mn.z}, {mn.x, mn.z} }}, tex});
    add_face({{{ {mn.x, mn.y, mn.z}, {mx.x, mn.y, mn.z},
                  {mx.x, mn.y, mx.z}, {mn.x, mn.y, mx.z} }},
               {{ {mn.x, mn.z}, {mx.x, mn.z}, {mx.x, mx.z}, {mn.x, mx.z} }}, tex});
    add_face({{{ {mn.x, mn.y, mx.z}, {mx.x, mn.y, mx.z},
                  {mx.x, mx.y, mx.z}, {mn.x, mx.y, mx.z} }},
               {{ {mn.x, mn.y}, {mx.x, mn.y}, {mx.x, mx.y}, {mn.x, mx.y} }}, tex});
    add_face({{{ {mx.x, mn.y, mn.z}, {mn.x, mn.y, mn.z},
                  {mn.x, mx.y, mn.z}, {mx.x, mx.y, mn.z} }},
               {{ {mx.x, mn.y}, {mn.x, mn.y}, {mn.x, mx.y}, {mx.x, mx.y} }}, tex});
    add_face({{{ {mx.x, mn.y, mx.z}, {mx.x, mn.y, mn.z},
                  {mx.x, mx.y, mn.z}, {mx.x, mx.y, mx.z} }},
               {{ {mx.z, mn.y}, {mn.z, mn.y}, {mn.z, mx.y}, {mx.z, mx.y} }}, tex});
    add_face({{{ {mn.x, mn.y, mn.z}, {mn.x, mn.y, mx.z},
                  {mn.x, mx.y, mx.z}, {mn.x, mx.y, mn.z} }},
               {{ {mn.z, mn.y}, {mx.z, mn.y}, {mx.z, mx.y}, {mn.z, mx.y} }}, tex});
    return first;
}

inline int Scene::add_pillar(Vec3 base_c, f32 w, f32 height, TexSampleFn tex) {
    const f32 hw = w * 0.5f;
    const Vec3 mn{ base_c.x - hw, base_c.y,          base_c.z - hw };
    const Vec3 mx{ base_c.x + hw, base_c.y + height, base_c.z + hw };
    const int first = n_faces();
    // Top, North, South, East, West (no bottom — pillar sits on floor)
    add_face({{{ {mn.x, mx.y, mx.z}, {mx.x, mx.y, mx.z},
                  {mx.x, mx.y, mn.z}, {mn.x, mx.y, mn.z} }},
               {{ {mn.x, mx.z}, {mx.x, mx.z}, {mx.x, mn.z}, {mn.x, mn.z} }}, tex});
    add_face({{{ {mn.x, mn.y, mx.z}, {mx.x, mn.y, mx.z},
                  {mx.x, mx.y, mx.z}, {mn.x, mx.y, mx.z} }},
               {{ {mn.x, mn.y}, {mx.x, mn.y}, {mx.x, mx.y}, {mn.x, mx.y} }}, tex});
    add_face({{{ {mx.x, mn.y, mn.z}, {mn.x, mn.y, mn.z},
                  {mn.x, mx.y, mn.z}, {mx.x, mx.y, mn.z} }},
               {{ {mx.x, mn.y}, {mn.x, mn.y}, {mn.x, mx.y}, {mx.x, mx.y} }}, tex});
    add_face({{{ {mx.x, mn.y, mx.z}, {mx.x, mn.y, mn.z},
                  {mx.x, mx.y, mn.z}, {mx.x, mx.y, mx.z} }},
               {{ {mx.z, mn.y}, {mn.z, mn.y}, {mn.z, mx.y}, {mx.z, mx.y} }}, tex});
    add_face({{{ {mn.x, mn.y, mn.z}, {mn.x, mn.y, mx.z},
                  {mn.x, mx.y, mx.z}, {mn.x, mx.y, mn.z} }},
               {{ {mn.z, mn.y}, {mx.z, mn.y}, {mx.z, mx.y}, {mn.z, mx.y} }}, tex});
    return first;
}

inline void Scene::load_default() {
    faces_.clear();
    lightmaps_.clear();
    add_floor({0, 0, 0}, 24.0f, tex_floor);
    add_pillar({0, 0, 0}, 0.8f, 3.5f, tex_brick);
    add_cube({3.0f, 0.5f, 1.5f}, 1.0f, tex_brick);
    add_cube({-2.5f, 0.4f, -1.8f}, 0.8f, tex_checker);
}

inline void Scene::bake_lightmaps(Vec3 light_pos, f32 intensity, f32 ambient) {
    const int n = n_faces();
    for (int i = 0; i < n; ++i) {
        lightmap_bake(lightmaps_[i], faces_[i],
                      std::span<const BspFace>{faces_}, i,
                      light_pos, intensity, ambient);
    }
}

}  // namespace sotark::sw
