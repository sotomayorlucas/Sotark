// Sotark raytracer — C++26 port of the M9-era pthread tile renderer.
//
// Architecture:
//   - Scene = static pool of Hittable + Material + (lights array, NEE driven).
//   - BVH built over `items` once, then traversed per ray.
//   - ray_color: NEE on Lambertian hits, recursive otherwise, with the
//     include_emission flag to prevent double-counting.
//   - Threading: std::jthread pool + std::atomic<int> tile counter.
//     Each tile is independent; per-tile Pcg32 seeded so output is
//     order-independent.

#include <atomic>
#include <thread>
#include <vector>
#include <array>
#include <string>
#include <string_view>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <print>
#include <span>
#include <algorithm>
#include <utility>
#include <variant>

import sotark.common;
import sotark.rt;

using namespace sotark;
using namespace sotark::rt;

namespace {

constexpr int MAX_HITTABLES = 2000;
constexpr int MAX_MATERIALS = 600;
constexpr int MAX_LIGHTS    = 16;

struct Sky {
    Rgb low{};
    Rgb high{};
};

struct Lights {
    std::span<const Hittable* const> items;
};

// ─── NEE: direct light contribution from a quad-shaped emitter ─────
Rgb direct_light_quad(const Hittable&  light,
                       const Material&  light_mat,
                       const Material&  surf_mat,
                       Vec3 hit_p, Vec3 hit_normal,
                       const Hittable&  world,
                       std::span<const Material> all_mats,
                       Pcg32& rng) noexcept {
    const Quad* lq = std::get_if<Quad>(&light.shape);
    if (!lq) return Rgb{};
    const Emissive* le = std::get_if<Emissive>(&light_mat.kind);
    if (!le) return Rgb{};
    const Lambertian* sl = std::get_if<Lambertian>(&surf_mat.kind);
    if (!sl) return Rgb{};

    const f32  ra = rng.float01();
    const f32  rb = rng.float01();
    const Vec3 P  = lq->Q + lq->u * ra + lq->v * rb;

    const Vec3 to_light = P - hit_p;
    const f32  dist_sq  = length_sq(to_light);
    if (dist_sq < 1e-6f) return Rgb{};
    const f32 dist = std::sqrt(dist_sq);

    const f32 cos_theta = dot(hit_normal, to_light) / dist;
    if (cos_theta <= 0.0f) return Rgb{};
    const f32 cos_alpha = -dot(lq->normal, to_light) / dist;
    if (cos_alpha <= 0.0f) return Rgb{};

    // Shadow test. Direction NOT normalized → t ≈ 1.0 corresponds to hit on
    // the light. Pass 1.001 since hit_quad uses strict-less.
    const Ray shadow{ hit_p, to_light };
    auto shadow_rec = hit(world, shadow, 0.001f, 1.001f);
    if (!shadow_rec || !is_emissive(all_mats[shadow_rec->mat])) return Rgb{};

    const f32 area   = length(cross(lq->u, lq->v));
    const Vec3 emit  = le->emit;
    const f32 factor = cos_theta * cos_alpha * area / (dist_sq * pi_f);
    return Vec3{ sl->albedo.x * emit.x, sl->albedo.y * emit.y, sl->albedo.z * emit.z } * factor;
}

Rgb ray_color(Ray r, int depth,
              const Hittable& world,
              std::span<const Material> mats,
              Lights lights, Sky sky,
              bool include_emission,
              Pcg32& rng) {
    if (depth <= 0) return Rgb{};

    auto rec = hit(world, r, 0.001f, std::numeric_limits<f32>::infinity());
    if (!rec) {
        const Vec3 unit = normalize(r.dir);
        const f32  a    = 0.5f * (unit.y + 1.0f);
        return lerp(sky.low, sky.high, a);
    }

    const Material& surf_mat = mats[rec->mat];
    const Rgb emitted_v = include_emission ? emitted(surf_mat) : Rgb{};

    if (is_emissive(surf_mat)) return emitted_v;

    Rgb direct{};
    if (!lights.items.empty() && is_lambertian(surf_mat)) {
        for (const Hittable* L : lights.items) {
            if (!std::holds_alternative<Quad>(L->shape)) continue;
            const Quad& lq = std::get<Quad>(L->shape);
            direct += direct_light_quad(*L, mats[lq.mat], surf_mat,
                                          rec->p, rec->normal,
                                          world, mats, rng);
        }
    }

    auto scat = scatter(surf_mat, r, *rec, rng);
    if (!scat) return emitted_v + direct;

    const bool nee_done = !lights.items.empty() && is_lambertian(surf_mat);
    const Rgb  indirect = ray_color(scat->scattered, depth - 1,
                                     world, mats, lights, sky,
                                     !nee_done, rng);
    return emitted_v + direct
         + Vec3{ scat->attenuation.x * indirect.x,
                 scat->attenuation.y * indirect.y,
                 scat->attenuation.z * indirect.z };
}

// ─── Scene builders (1-shot, mutate caller-provided pool/items/mats) ──
struct SceneBuilder {
    Hittable*  pool;
    const Hittable** items;
    Material*  mats;
    int        n_items{0};
    int        n_mats{0};
    int        cap_items;

    void add(Hittable h) {
        pool[n_items]  = h;
        items[n_items] = &pool[n_items];
        ++n_items;
    }
    u32 add_material(Material m) {
        mats[n_mats] = m;
        return static_cast<u32>(n_mats++);
    }
};

void add_box(SceneBuilder& sb, Vec3 mn, Vec3 mx, u32 mat) {
    const f32 dx = mx.x - mn.x, dy = mx.y - mn.y, dz = mx.z - mn.z;
    sb.add(make_quad({mn.x, mn.y, mx.z}, { dx, 0, 0}, {0, dy, 0}, mat));
    sb.add(make_quad({mx.x, mn.y, mn.z}, {-dx, 0, 0}, {0, dy, 0}, mat));
    sb.add(make_quad({mx.x, mn.y, mx.z}, {0, 0, -dz}, {0, dy, 0}, mat));
    sb.add(make_quad({mn.x, mn.y, mn.z}, {0, 0,  dz}, {0, dy, 0}, mat));
    sb.add(make_quad({mn.x, mx.y, mx.z}, { dx, 0, 0}, {0, 0, -dz}, mat));
    sb.add(make_quad({mn.x, mn.y, mn.z}, { dx, 0, 0}, {0, 0,  dz}, mat));
}

int build_cover(SceneBuilder& sb, Pcg32& rng) {
    sb.add_material({Lambertian{Vec3{0.5f, 0.5f, 0.5f}}});
    sb.add(make_sphere({0.0f, -1000.0f, 0.0f}, 1000.0f, 0));

    for (int a = -11; a < 11 && sb.n_items < sb.cap_items - 4; ++a) {
        for (int b = -11; b < 11 && sb.n_items < sb.cap_items - 4; ++b) {
            const f32  choose = rng.float01();
            const Vec3 center{
                static_cast<f32>(a) + 0.9f * rng.float01(),
                0.2f,
                static_cast<f32>(b) + 0.9f * rng.float01(),
            };
            if (length(center - Vec3{4.0f, 0.2f, 0.0f}) <= 0.9f) continue;

            u32 mat_idx;
            if (choose < 0.8f) {
                const Vec3 albedo{
                    rng.float01() * rng.float01(),
                    rng.float01() * rng.float01(),
                    rng.float01() * rng.float01(),
                };
                mat_idx = sb.add_material({Lambertian{albedo}});
            } else if (choose < 0.95f) {
                const Vec3 albedo{
                    0.5f + 0.5f * rng.float01(),
                    0.5f + 0.5f * rng.float01(),
                    0.5f + 0.5f * rng.float01(),
                };
                mat_idx = sb.add_material({Metal{albedo, 0.5f * rng.float01()}});
            } else {
                mat_idx = sb.add_material({Dielectric{1.5f}});
            }
            sb.add(make_sphere(center, 0.2f, mat_idx));
        }
    }

    sb.add(make_sphere({ 0.0f, 1.0f, 0.0f}, 1.0f, sb.add_material({Dielectric{1.5f}})));
    sb.add(make_sphere({-4.0f, 1.0f, 0.0f}, 1.0f, sb.add_material({Lambertian{Vec3{0.4f, 0.2f, 0.1f}}})));
    sb.add(make_sphere({ 4.0f, 1.0f, 0.0f}, 1.0f, sb.add_material({Metal{Vec3{0.7f, 0.6f, 0.5f}, 0.0f}})));
    return sb.n_items;
}

int build_cornell(SceneBuilder& sb) {
    const u32 RED   = sb.add_material({Lambertian{Vec3{0.65f, 0.05f, 0.05f}}});
    const u32 GREEN = sb.add_material({Lambertian{Vec3{0.12f, 0.45f, 0.15f}}});
    const u32 WHITE = sb.add_material({Lambertian{Vec3{0.73f, 0.73f, 0.73f}}});
    const u32 LIGHT = sb.add_material({Emissive  {Vec3{15.0f, 15.0f, 15.0f}}});

    sb.add(make_quad({555, 0, 0},     {0, 555, 0},   {0, 0, 555}, GREEN));
    sb.add(make_quad({0,   0, 0},     {0, 555, 0},   {0, 0, 555}, RED));
    sb.add(make_quad({343, 554, 332}, {-130, 0, 0},  {0, 0, -105}, LIGHT));
    sb.add(make_quad({0,   0,   0},   {555, 0, 0},   {0, 0, 555}, WHITE));
    sb.add(make_quad({555, 555, 0},   {-555, 0, 0},  {0, 0, 555}, WHITE));
    sb.add(make_quad({0, 0, 555},     {555, 0, 0},   {0, 555, 0}, WHITE));

    add_box(sb, {265, 0,  295}, {430, 330, 460}, WHITE);
    add_box(sb, {130, 0,   65}, {295, 165, 230}, WHITE);
    return sb.n_items;
}

// Recursive icosphere subdivision.
void ico_subdiv(SceneBuilder& sb, Vec3 a, Vec3 b, Vec3 c, int depth,
                Vec3 center, f32 radius, u32 mat) {
    if (depth == 0) {
        sb.add(make_triangle(center + a*radius, center + b*radius, center + c*radius, mat));
        return;
    }
    const Vec3 ab = normalize(a + b);
    const Vec3 bc = normalize(b + c);
    const Vec3 ca = normalize(c + a);
    ico_subdiv(sb, a,  ab, ca, depth - 1, center, radius, mat);
    ico_subdiv(sb, b,  bc, ab, depth - 1, center, radius, mat);
    ico_subdiv(sb, c,  ca, bc, depth - 1, center, radius, mat);
    ico_subdiv(sb, ab, bc, ca, depth - 1, center, radius, mat);
}

void build_icosphere(SceneBuilder& sb, Vec3 center, f32 radius, int depth, u32 mat) {
    const f32 phi = (1.0f + std::sqrt(5.0f)) * 0.5f;
    const f32 inv = 1.0f / std::sqrt(1.0f + phi * phi);
    const f32 a = inv, b = phi * inv;
    const std::array<Vec3, 12> V{{
        {-a,  b,  0}, { a,  b,  0}, {-a, -b,  0}, { a, -b,  0},
        { 0, -a,  b}, { 0,  a,  b}, { 0, -a, -b}, { 0,  a, -b},
        { b,  0, -a}, { b,  0,  a}, {-b,  0, -a}, {-b,  0,  a},
    }};
    static constexpr std::array<std::array<u8, 3>, 20> T{{
        {0,11,5}, {0,5,1},  {0,1,7},  {0,7,10}, {0,10,11},
        {1,5,9},  {5,11,4}, {11,10,2},{10,7,6}, {7,1,8},
        {3,9,4},  {3,4,2},  {3,2,6},  {3,6,8},  {3,8,9},
        {4,9,5},  {2,4,11}, {6,2,10}, {8,6,7},  {9,8,1},
    }};
    for (const auto& tri : T) {
        ico_subdiv(sb, V[tri[0]], V[tri[1]], V[tri[2]], depth, center, radius, mat);
    }
}

int build_mesh(SceneBuilder& sb) {
    sb.add(make_sphere({0.0f, -1000.0f, 0.0f}, 1000.0f,
                        sb.add_material({Lambertian{Vec3{0.5f, 0.5f, 0.5f}}})));
    const u32 mesh_mat = sb.add_material({Metal{Vec3{0.85f, 0.75f, 0.55f}, 0.05f}});
    build_icosphere(sb, {0, 1, 0}, 1.0f, 3, mesh_mat);
    return sb.n_items;
}

int build_obj(SceneBuilder& sb, std::string_view path) {
    sb.add(make_sphere({0.0f, -1000.0f, 0.0f}, 1000.0f,
                        sb.add_material({Lambertian{Vec3{0.5f, 0.5f, 0.5f}}})));
    const u32 mesh_mat = sb.add_material({Metal{Vec3{0.85f, 0.5f, 0.4f}, 0.02f}});

    auto mesh_res = obj_load(path);
    if (!mesh_res) {
        log_error("could not load mesh from '{}': {}", path, mesh_res.error());
        return 0;
    }
    const ObjMesh& mesh = *mesh_res;
    if (mesh.tris.empty() || mesh.verts.empty()) {
        log_error("mesh empty");
        return 0;
    }

    // Bbox, center+scale into [-1,1]³ with base at y=0.
    Vec3 mn = mesh.verts[0], mx = mesh.verts[0];
    for (Vec3 v : mesh.verts) {
        mn.x = std::min(mn.x, v.x); mx.x = std::max(mx.x, v.x);
        mn.y = std::min(mn.y, v.y); mx.y = std::max(mx.y, v.y);
        mn.z = std::min(mn.z, v.z); mx.z = std::max(mx.z, v.z);
    }
    const Vec3 c{ (mn.x+mx.x)*0.5f, (mn.y+mx.y)*0.5f, (mn.z+mx.z)*0.5f };
    f32 ext = std::max({mx.x - mn.x, mx.y - mn.y, mx.z - mn.z});
    if (ext < 1e-6f) ext = 1.0f;
    const f32 scale = 2.0f / ext;
    const Vec3 offset{0.0f, -(mn.y - c.y) * scale, 0.0f};

    for (const auto& tri : mesh.tris) {
        if (sb.n_items >= sb.cap_items) break;
        const Vec3 pa = (mesh.verts[tri[0]] - c) * scale + offset;
        const Vec3 pb = (mesh.verts[tri[1]] - c) * scale + offset;
        const Vec3 pc = (mesh.verts[tri[2]] - c) * scale + offset;
        sb.add(make_triangle(pa, pb, pc, mesh_mat));
    }
    log_info("OBJ scene: {} triangles loaded from '{}'", mesh.tris.size(), path);
    return sb.n_items;
}

int build_earth(SceneBuilder& sb, std::string_view path, Image& tex_out) {
    auto img = load_ppm(path);
    if (!img) {
        log_error("could not load earth texture '{}': {}", path, img.error());
        return 0;
    }
    tex_out = std::move(*img);
    log_info("loaded texture {}x{} from '{}'", tex_out.width(), tex_out.height(), path);

    sb.add(make_sphere({0.0f, -1000.0f, 0.0f}, 1000.0f,
                        sb.add_material({Lambertian{Vec3{0.5f, 0.5f, 0.5f}}})));
    sb.add(make_sphere({0.0f, 1.5f, 0.0f}, 1.5f,
                        sb.add_material({TexturedLambertian{&tex_out, 1.0f, 1.0f}})));
    sb.add(make_sphere({3.0f, 1.0f, -1.0f}, 1.0f,
                        sb.add_material({Metal{Vec3{0.85f, 0.75f, 0.55f}, 0.08f}})));
    sb.add(make_sphere({-3.0f, 1.0f, -0.5f}, 1.0f,
                        sb.add_material({Dielectric{1.5f}})));
    return sb.n_items;
}

// ─── tile-based threaded renderer ───────────────────────────────────
constexpr int TILE_SIZE = 32;

struct TileJob {
    int width, height;
    int spp, max_depth;
    u64 base_seed;
    const Camera*   cam;
    const Hittable* world;
    std::span<const Material> mats;
    Lights          lights;
    Sky             sky;
    std::span<Rgb>  pixels;
    int             tile_size;
    int             tiles_x;
};

void render_one_tile(const TileJob& tj, int tile_idx) {
    const int tx = tile_idx % tj.tiles_x;
    const int ty = tile_idx / tj.tiles_x;
    const int x0 = tx * tj.tile_size;
    const int y0 = ty * tj.tile_size;
    const int x1 = std::min(x0 + tj.tile_size, tj.width);
    const int y1 = std::min(y0 + tj.tile_size, tj.height);

    Pcg32 rng(tj.base_seed, static_cast<u64>(tile_idx) + 1ull);

    const f32 inv_spp = 1.0f / static_cast<f32>(tj.spp);

    for (int j = y0; j < y1; ++j) {
        for (int i = x0; i < x1; ++i) {
            Rgb accum{};
            for (int s = 0; s < tj.spp; ++s) {
                const Ray r = tj.cam->get_ray(i, j, rng);
                accum += ray_color(r, tj.max_depth,
                                    *tj.world, tj.mats, tj.lights,
                                    tj.sky, true, rng);
            }
            tj.pixels[j * tj.width + i] = accum * inv_spp;
        }
    }
}

void render_tiles(const TileJob& tj, int num_threads) {
    const int tiles_y = (tj.height + tj.tile_size - 1) / tj.tile_size;
    const int total   = tj.tiles_x * tiles_y;

    if (num_threads <= 1) {
        for (int t = 0; t < total; ++t) {
            render_one_tile(tj, t);
            if ((t & 7) == 0) std::print(stderr, "\rtiles remaining: {:4}  ", total - t);
        }
        return;
    }

    std::atomic<int>           next_tile{0};
    std::vector<std::jthread>  workers;
    workers.reserve(static_cast<usize>(num_threads));
    for (int i = 0; i < num_threads; ++i) {
        workers.emplace_back([&tj, &next_tile, total]() {
            for (;;) {
                const int t = next_tile.fetch_add(1, std::memory_order_relaxed);
                if (t >= total) return;
                render_one_tile(tj, t);
            }
        });
    }
    // jthread joins on destruction.
}

}  // namespace

int main(int argc, char** argv) {
    const std::string_view out_path = argc > 1 ? argv[1] : "out.ppm";
    const std::string_view scene_s  = argc > 2 ? argv[2] : "cover";
    const int spp     = argc > 3 ? std::atoi(argv[3]) : 10;
    const int depth   = argc > 4 ? std::atoi(argv[4]) : 50;
    const u64 seed    = argc > 5 ? static_cast<u64>(std::atoll(argv[5])) : 42ull;
    int       threads = argc > 6 ? std::atoi(argv[6]) : 0;
    if (threads <= 0) threads = static_cast<int>(std::thread::hardware_concurrency());
    if (threads <= 0) threads = 1;

    const bool is_cornell = (scene_s == "cornell");
    const bool is_cover   = (scene_s == "cover");
    const bool is_mesh    = (scene_s == "mesh");
    const bool is_obj     = (scene_s == "obj");
    const bool is_earth   = (scene_s == "earth");
    if (!(is_cornell || is_cover || is_mesh || is_obj || is_earth)) {
        log_error("unknown scene '{}'. Use cover|cornell|mesh|obj|earth.", scene_s);
        return 1;
    }
    const std::string_view aux_path = argc > 7 ? argv[7]
                                                : (is_earth ? "earth.ppm" : "model.obj");

    int width{}, height{};
    Sky sky{};
    Vec3 lookfrom{}, lookat{}, vup{0, 1, 0};
    f32 vfov{20}, defocus_angle{0.0f}, focus_dist{10.0f};

    if (is_cornell) {
        width = height = 400;
        lookfrom = {278, 278, -800};
        lookat   = {278, 278,    0};
        vfov     = 40.0f;
        focus_dist = 800.0f;
        sky = {Rgb{0,0,0}, Rgb{0,0,0}};
    } else if (is_mesh || is_obj) {
        width = 400; height = 225;
        lookfrom = {4, 2.5, 4};
        lookat   = {0, 0.7, 0};
        vfov     = 30.0f;
        focus_dist = 5.0f;
        sky = {Rgb{1, 1, 1}, Rgb{0.55f, 0.75f, 1.0f}};
    } else if (is_earth) {
        width = 400; height = 225;
        lookfrom = {0, 2.5f, 7};
        lookat   = {0, 1.0f, 0};
        vfov     = 30.0f;
        focus_dist = 7.0f;
        sky = {Rgb{1, 1, 1}, Rgb{0.55f, 0.75f, 1.0f}};
    } else {  // cover
        width = 400; height = 225;
        lookfrom = {13, 2, 3};
        lookat   = {0, 0, 0};
        vfov     = 20.0f;
        defocus_angle = 0.6f;
        focus_dist    = 10.0f;
        sky = {Rgb{1, 1, 1}, Rgb{0.5f, 0.7f, 1.0f}};
    }

    const Camera cam(lookfrom, lookat, vup, vfov, width, height, defocus_angle, focus_dist);

    // ─── Static pools (zero allocations on hot path) ────────────────
    static std::array<Hittable, MAX_HITTABLES> pool;
    static std::array<const Hittable*, MAX_HITTABLES> items_arr;
    static std::array<Material, MAX_MATERIALS> mats_arr;
    SceneBuilder sb{ pool.data(), items_arr.data(), mats_arr.data(),
                     0, 0, MAX_HITTABLES };

    Image earth_tex;   // owns the texture for build_earth
    int n_items = 0;
    if (is_cornell)   n_items = build_cornell(sb);
    else if (is_mesh) n_items = build_mesh(sb);
    else if (is_obj)  n_items = build_obj(sb, aux_path);
    else if (is_earth) n_items = build_earth(sb, aux_path, earth_tex);
    else {
        Pcg32 scene_rng(1u, 1u);
        n_items = build_cover(sb, scene_rng);
    }
    if (n_items == 0) return 1;

    // Lights: collect emissive primitives BEFORE bvh_build (which reorders items).
    std::array<const Hittable*, MAX_LIGHTS> light_arr{};
    int n_lights = 0;
    for (int i = 0; i < n_items && n_lights < MAX_LIGHTS; ++i) {
        const Hittable* h = items_arr[i];
        const u32 mat_idx = std::visit([](const auto& shape) -> u32 {
            using T = std::decay_t<decltype(shape)>;
            if constexpr (std::is_same_v<T, Quad>)        return shape.mat;
            else if constexpr (std::is_same_v<T, Sphere>) return shape.mat;
            else if constexpr (std::is_same_v<T, Tri>)    return shape.mat;
            else return static_cast<u32>(-1);
        }, h->shape);
        if (mat_idx != static_cast<u32>(-1) && is_emissive(mats_arr[mat_idx])) {
            light_arr[n_lights++] = items_arr[i];
        }
    }
    const Lights lights{ std::span<const Hittable* const>{light_arr.data(), static_cast<usize>(n_lights)} };

    std::vector<Hittable> bvh_arena;
    bvh_arena.reserve(static_cast<usize>(MAX_HITTABLES * 2));
    const Hittable* root = bvh_build(
        std::span<const Hittable*>{items_arr.data(), static_cast<usize>(n_items)},
        bvh_arena);
    log_info("scene='{}' items={} bvh_nodes={} lights={}",
              scene_s, n_items, bvh_arena.size(), n_lights);

    Image img(width, height);
    log_info("rendering {}x{}, {} spp, depth {}, seed {}, {} threads -> {}",
              width, height, spp, depth, seed, threads, out_path);

    const TileJob tj{
        .width = width, .height = height,
        .spp = spp, .max_depth = depth,
        .base_seed = seed,
        .cam = &cam, .world = root,
        .mats   = std::span<const Material>{mats_arr.data(), static_cast<usize>(sb.n_mats)},
        .lights = lights,
        .sky    = sky,
        .pixels = std::span<Rgb>{img.pixels()},
        .tile_size = TILE_SIZE,
        .tiles_x   = (width + TILE_SIZE - 1) / TILE_SIZE,
    };

    const auto t0 = std::chrono::steady_clock::now();
    render_tiles(tj, threads);
    const auto t1 = std::chrono::steady_clock::now();
    const f64 elapsed = std::chrono::duration<f64>(t1 - t0).count();
    std::print(stderr, "\rdone in {:.2f}s                       \n", elapsed);

    if (auto r = write_ppm(img, out_path); !r) {
        log_error("write_ppm: {}", r.error());
        return 1;
    }
    log_info("wrote {}", out_path);
    return 0;
}
