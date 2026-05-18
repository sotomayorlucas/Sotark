#include "common/types.h"
#include "common/log.h"
#include "common/vec.h"
#include "common/color.h"
#include "common/image.h"
#include "common/rng.h"
#include "rt/ray.h"
#include "rt/hit.h"
#include "rt/hittable.h"
#include "rt/material.h"
#include "rt/camera.h"
#include "rt/bvh.h"
#include "rt/obj.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

/*
 * M7 — Importance sampling vía NEE (Next Event Estimation).
 *
 * Cornell box sin NEE: la única manera de iluminar es que un rayo random
 * caiga sobre la luz chica del techo. Con 200 spp se ve, pero con MUCHO
 * noise. Cada lambertian bounce shootea ~uniform en la hemisfera y casi
 * nunca apunta a la luz.
 *
 * Con NEE: en CADA hit lambertian, ADEMÁS del rebote indirecto random,
 * sampleamos explícitamente un punto en cada luz y mandamos un shadow ray.
 * Si llega sin obstrucción, sumamos la contribución directa. El indirecto
 * sigue funcionando para iluminación global (color bleeding) pero ahora
 * el bulk de la energía la trae la sampling directa.
 *
 * Para evitar double-counting, el indirect ray recurre con
 * include_emission=false — si por azar el bounce le pega a la luz, NO
 * cuenta su emisión otra vez (la NEE de este nivel ya lo hizo).
 *
 * Esto es "biased toward NEE" si se interpreta literalmente (sub-optimal
 * MIS), pero unbiased en el sentido path-tracing estándar siempre que
 * cada lambertian haga NEE. La varianza baja ~10× para Cornell box.
 */

#define MAX_HITTABLES  2000
#define MAX_MATERIALS  600
#define MAX_LIGHTS     16

typedef struct {
    rgb_t low;
    rgb_t high;
} sky_t;

typedef struct {
    hittable_t **items;     /* pointers a hittables emisivos */
    int          count;
} lights_t;

/* Sample uniforme sobre el quad de una luz y devuelve la contribución
 * directa de Lambertian shading. Retorna negro si:
 *   - la geometría está back-to-back (cos < 0)
 *   - shadow ray hit algo NO emisivo en el camino
 */
static rgb_t direct_light_quad(const hittable_t *light,
                                const material_t *light_mat,
                                const material_t *surf_mat,
                                vec3_t hit_p, vec3_t hit_normal,
                                const hittable_t *world,
                                const material_t *all_mats,
                                pcg32_t *rng) {
    if (light->kind != HIT_QUAD)              return (rgb_t){0,0,0};
    if (light_mat->kind != MAT_EMISSIVE)      return (rgb_t){0,0,0};

    /* Punto random uniforme sobre el quad: P = Q + a·u + b·v, (a, b) ∈ U(0,1)². */
    const f32 ra = pcg32_float01(rng);
    const f32 rb = pcg32_float01(rng);
    const vec3_t P = v3_add(v3_add(light->u.quad.Q,
                                    v3_scale(light->u.quad.u, ra)),
                             v3_scale(light->u.quad.v, rb));

    const vec3_t to_light = v3_sub(P, hit_p);
    const f32    dist_sq  = v3_length_sq(to_light);
    if (dist_sq < 1e-6f) return (rgb_t){0,0,0};
    const f32    dist     = sqrtf(dist_sq);

    /* Cosines en superficie shading y en superficie de la luz. */
    const f32 cos_theta = v3_dot(hit_normal, to_light) / dist;
    if (cos_theta <= 0.0f) return (rgb_t){0,0,0};
    const f32 cos_alpha = -v3_dot(light->u.quad.normal, to_light) / dist;
    if (cos_alpha <= 0.0f) return (rgb_t){0,0,0};

    /* Shadow test. Direction NO normalizada → t ≈ 1.0 corresponde al hit en
     * la luz. Como hit_quad usa strict-less (t < t_max), pasamos 1.001 para
     * incluir el hit en la luz. Si el primer hit no es emisivo, hay oclusión. */
    const ray_t shadow = { hit_p, to_light };
    hit_record_t shadow_rec;
    if (!hit_hittable(world, shadow, 0.001f, 1.001f, &shadow_rec)) {
        return (rgb_t){0,0,0};  /* sin hits, no debería pasar pero por las dudas */
    }
    if (all_mats[shadow_rec.mat].kind != MAT_EMISSIVE) {
        return (rgb_t){0,0,0};  /* algo no-luz se interpuso */
    }

    /* L_in = (area * cos_θ * cos_α / dist²) * L_emit * BRDF
     * BRDF lambertian = albedo / π */
    const f32    area = v3_length(v3_cross(light->u.quad.u, light->u.quad.v));
    const vec3_t albedo = surf_mat->u.lambertian.albedo;
    const vec3_t emit   = light_mat->u.emissive.emit;
    const f32    factor = cos_theta * cos_alpha * area / (dist_sq * 3.14159265f);
    return v3_scale(v3_mul(albedo, emit), factor);
}

static rgb_t ray_color(ray_t r, int depth,
                       const hittable_t *world,
                       const material_t *mats,
                       const lights_t *lights,
                       sky_t sky,
                       bool include_emission,
                       pcg32_t *rng) {
    if (depth <= 0) return (rgb_t){0,0,0};

    hit_record_t rec;
    if (hit_hittable(world, r, 0.001f, INFINITY, &rec)) {
        const material_t *surf_mat = &mats[rec.mat];
        const rgb_t emitted = include_emission
            ? material_emitted(surf_mat)
            : (rgb_t){0,0,0};

        if (surf_mat->kind == MAT_EMISSIVE) {
            /* Emissive no dispersa; solo aporta su emisión. */
            return emitted;
        }

        /* NEE solo para lambertian (metal/dielectric scatter en cono estrecho,
         * NEE no aporta mucho). */
        rgb_t direct = {0,0,0};
        if (lights && lights->count > 0 && surf_mat->kind == MAT_LAMBERTIAN) {
            for (int i = 0; i < lights->count; ++i) {
                const hittable_t *L = lights->items[i];
                if (L->kind != HIT_QUAD) continue;
                const material_t *L_mat = &mats[L->u.quad.mat];
                direct = v3_add(direct, direct_light_quad(
                    L, L_mat, surf_mat, rec.p, rec.normal,
                    world, mats, rng));
            }
        }

        ray_t  scattered;
        vec3_t attenuation;
        if (material_scatter(surf_mat, r, &rec, &attenuation, &scattered, rng)) {
            /* Si hicimos NEE en este nivel, el indirect NO debe contar
             * emission (la NEE ya cubrió ese path). */
            const bool nee_done = (lights && lights->count > 0)
                                && (surf_mat->kind == MAT_LAMBERTIAN);
            const rgb_t indirect = ray_color(scattered, depth - 1,
                                              world, mats, lights, sky,
                                              !nee_done, rng);
            return v3_add(v3_add(emitted, direct),
                          v3_mul(attenuation, indirect));
        }
        return v3_add(emitted, direct);
    }

    const vec3_t unit = v3_normalize(r.dir);
    const f32    a    = 0.5f * (unit.y + 1.0f);
    return v3_lerp(sky.low, sky.high, a);
}

/* ─── Scene builders (sin cambios desde M6) ────────────────────────── */

static void add_sphere(hittable_t *arr, hittable_t **items, int *n,
                       vec3_t center, f32 radius, u32 mat_idx) {
    arr[*n] = hittable_sphere(center, radius, mat_idx);
    items[*n] = &arr[*n];
    (*n)++;
}

static void add_quad(hittable_t *arr, hittable_t **items, int *n,
                     vec3_t Q, vec3_t u, vec3_t v, u32 mat_idx) {
    arr[*n] = hittable_quad(Q, u, v, mat_idx);
    items[*n] = &arr[*n];
    (*n)++;
}

static void add_box(hittable_t *arr, hittable_t **items, int *n,
                    vec3_t mn, vec3_t mx, u32 mat) {
    const f32 dx = mx.x - mn.x, dy = mx.y - mn.y, dz = mx.z - mn.z;
    add_quad(arr, items, n, (vec3_t){mn.x, mn.y, mx.z}, (vec3_t){ dx, 0, 0}, (vec3_t){0, dy, 0}, mat);
    add_quad(arr, items, n, (vec3_t){mx.x, mn.y, mn.z}, (vec3_t){-dx, 0, 0}, (vec3_t){0, dy, 0}, mat);
    add_quad(arr, items, n, (vec3_t){mx.x, mn.y, mx.z}, (vec3_t){0, 0, -dz}, (vec3_t){0, dy, 0}, mat);
    add_quad(arr, items, n, (vec3_t){mn.x, mn.y, mn.z}, (vec3_t){0, 0,  dz}, (vec3_t){0, dy, 0}, mat);
    add_quad(arr, items, n, (vec3_t){mn.x, mx.y, mx.z}, (vec3_t){ dx, 0, 0}, (vec3_t){0, 0, -dz}, mat);
    add_quad(arr, items, n, (vec3_t){mn.x, mn.y, mn.z}, (vec3_t){ dx, 0, 0}, (vec3_t){0, 0,  dz}, mat);
}

static int build_cover_scene(hittable_t *arr, hittable_t **items, material_t *mats,
                              int max_items, pcg32_t *scene_rng) {
    int n = 0, nm = 0;
    mats[nm] = (material_t){ .kind = MAT_LAMBERTIAN, .u.lambertian = { { 0.5f, 0.5f, 0.5f } } };
    add_sphere(arr, items, &n, (vec3_t){ 0.0f, -1000.0f, 0.0f }, 1000.0f, (u32)nm); nm++;

    for (int a = -11; a < 11 && n < max_items - 4; ++a) {
        for (int b = -11; b < 11 && n < max_items - 4; ++b) {
            const f32 choose = pcg32_float01(scene_rng);
            const vec3_t center = {
                (f32)a + 0.9f * pcg32_float01(scene_rng),
                0.2f,
                (f32)b + 0.9f * pcg32_float01(scene_rng),
            };
            const vec3_t to_anchor = v3_sub(center, (vec3_t){ 4.0f, 0.2f, 0.0f });
            if (v3_length(to_anchor) <= 0.9f) continue;

            if (choose < 0.8f) {
                const vec3_t albedo = {
                    pcg32_float01(scene_rng) * pcg32_float01(scene_rng),
                    pcg32_float01(scene_rng) * pcg32_float01(scene_rng),
                    pcg32_float01(scene_rng) * pcg32_float01(scene_rng),
                };
                mats[nm] = (material_t){ .kind = MAT_LAMBERTIAN, .u.lambertian = { albedo } };
            } else if (choose < 0.95f) {
                const vec3_t albedo = {
                    0.5f + 0.5f * pcg32_float01(scene_rng),
                    0.5f + 0.5f * pcg32_float01(scene_rng),
                    0.5f + 0.5f * pcg32_float01(scene_rng),
                };
                mats[nm] = (material_t){ .kind = MAT_METAL,
                                          .u.metal = { albedo, 0.5f * pcg32_float01(scene_rng) } };
            } else {
                mats[nm] = (material_t){ .kind = MAT_DIELECTRIC, .u.dielectric = { 1.5f } };
            }
            add_sphere(arr, items, &n, center, 0.2f, (u32)nm); nm++;
        }
    }

    mats[nm] = (material_t){ .kind = MAT_DIELECTRIC, .u.dielectric = { 1.5f } };
    add_sphere(arr, items, &n, (vec3_t){  0.0f, 1.0f, 0.0f }, 1.0f, (u32)nm); nm++;
    mats[nm] = (material_t){ .kind = MAT_LAMBERTIAN, .u.lambertian = { { 0.4f, 0.2f, 0.1f } } };
    add_sphere(arr, items, &n, (vec3_t){ -4.0f, 1.0f, 0.0f }, 1.0f, (u32)nm); nm++;
    mats[nm] = (material_t){ .kind = MAT_METAL, .u.metal = { { 0.7f, 0.6f, 0.5f }, 0.0f } };
    add_sphere(arr, items, &n, (vec3_t){  4.0f, 1.0f, 0.0f }, 1.0f, (u32)nm); nm++;
    return n;
}

/* ─── Icosphere: subdivide an icosahedron N veces para mesh test ────── */

static void add_triangle(hittable_t *arr, hittable_t **items, int *n,
                         vec3_t a, vec3_t b, vec3_t c, u32 mat) {
    arr[*n] = hittable_triangle(a, b, c, mat);
    items[*n] = &arr[*n];
    (*n)++;
}

static void ico_subdiv(vec3_t a, vec3_t b, vec3_t c, int depth,
                        vec3_t center, f32 radius,
                        hittable_t *arr, hittable_t **items, int *n, u32 mat) {
    if (depth == 0) {
        const vec3_t p0 = v3_add(center, v3_scale(a, radius));
        const vec3_t p1 = v3_add(center, v3_scale(b, radius));
        const vec3_t p2 = v3_add(center, v3_scale(c, radius));
        add_triangle(arr, items, n, p0, p1, p2, mat);
        return;
    }
    /* Midpoints proyectados a la esfera unidad. */
    const vec3_t ab = v3_normalize(v3_add(a, b));
    const vec3_t bc = v3_normalize(v3_add(b, c));
    const vec3_t ca = v3_normalize(v3_add(c, a));
    ico_subdiv(a,  ab, ca, depth - 1, center, radius, arr, items, n, mat);
    ico_subdiv(b,  bc, ab, depth - 1, center, radius, arr, items, n, mat);
    ico_subdiv(c,  ca, bc, depth - 1, center, radius, arr, items, n, mat);
    ico_subdiv(ab, bc, ca, depth - 1, center, radius, arr, items, n, mat);
}

static void build_icosphere(hittable_t *arr, hittable_t **items, int *n,
                             vec3_t center, f32 radius, int depth, u32 mat) {
    const f32 phi = (1.0f + sqrtf(5.0f)) * 0.5f;
    const f32 inv = 1.0f / sqrtf(1.0f + phi * phi);
    const f32 a = inv, b = phi * inv;
    /* 12 vertices del icosahedron, ya en esfera unidad. */
    const vec3_t V[12] = {
        {-a,  b,  0}, { a,  b,  0}, {-a, -b,  0}, { a, -b,  0},
        { 0, -a,  b}, { 0,  a,  b}, { 0, -a, -b}, { 0,  a, -b},
        { b,  0, -a}, { b,  0,  a}, {-b,  0, -a}, {-b,  0,  a},
    };
    static const u8 T[20][3] = {
        {0,11,5}, {0,5,1},  {0,1,7},  {0,7,10}, {0,10,11},
        {1,5,9},  {5,11,4}, {11,10,2},{10,7,6}, {7,1,8},
        {3,9,4},  {3,4,2},  {3,2,6},  {3,6,8},  {3,8,9},
        {4,9,5},  {2,4,11}, {6,2,10}, {8,6,7},  {9,8,1},
    };
    for (int i = 0; i < 20; ++i) {
        ico_subdiv(V[T[i][0]], V[T[i][1]], V[T[i][2]],
                    depth, center, radius, arr, items, n, mat);
    }
}

/* "Earth" scene: una esfera grande con textura image cargada de PPM, + ground.
 * Demuestra image-based textures + spherical UV mapping (Shirley book 2 §6). */
static image_t g_earth_tex;
static bool    g_earth_tex_loaded = false;

static int build_earth_scene(hittable_t *arr, hittable_t **items, material_t *mats,
                              int max_items, const char *tex_path) {
    (void)max_items;
    int n = 0, nm = 0;

    if (!g_earth_tex_loaded) {
        if (image_load_ppm(&g_earth_tex, tex_path) != 0) {
            LOG_ERROR("could not load earth texture from '%s'", tex_path);
            return 0;
        }
        g_earth_tex_loaded = true;
        LOG_INFO("loaded texture %dx%d from '%s'", g_earth_tex.w, g_earth_tex.h, tex_path);
    }

    /* Ground. */
    mats[nm++] = (material_t){ .kind = MAT_LAMBERTIAN, .u.lambertian = { { 0.5f, 0.5f, 0.5f } } };
    add_sphere(arr, items, &n, (vec3_t){ 0.0f, -1000.0f, 0.0f }, 1000.0f, (u32)(nm - 1));

    /* Earth — esfera texturada. */
    mats[nm++] = (material_t){
        .kind = MAT_TEXTURED_LAMBERTIAN,
        .u.textured_lambertian = { &g_earth_tex, 1.0f, 1.0f },
    };
    add_sphere(arr, items, &n, (vec3_t){ 0.0f, 1.5f, 0.0f }, 1.5f, (u32)(nm - 1));

    /* Esfera metal de acompañamiento. */
    mats[nm++] = (material_t){ .kind = MAT_METAL,
                                .u.metal = { { 0.85f, 0.75f, 0.55f }, 0.08f } };
    add_sphere(arr, items, &n, (vec3_t){ 3.0f, 1.0f, -1.0f }, 1.0f, (u32)(nm - 1));

    /* Esfera glass. */
    mats[nm++] = (material_t){ .kind = MAT_DIELECTRIC, .u.dielectric = { 1.5f } };
    add_sphere(arr, items, &n, (vec3_t){ -3.0f, 1.0f, -0.5f }, 1.0f, (u32)(nm - 1));

    return n;
}

/* Scene desde archivo OBJ. Carga, centra y escala el mesh para fit dentro
 * de [-1, 1]³ con base en y=0. Material: lambertian rojizo. */
static int build_obj_scene(hittable_t *arr, hittable_t **items, material_t *mats,
                            int max_items, const char *path) {
    int n = 0, nm = 0;

    /* Ground gris. */
    mats[nm++] = (material_t){ .kind = MAT_LAMBERTIAN, .u.lambertian = { { 0.5f, 0.5f, 0.5f } } };
    add_sphere(arr, items, &n, (vec3_t){ 0.0f, -1000.0f, 0.0f }, 1000.0f, (u32)(nm - 1));

    /* Material para el mesh: metal cobrizo, queda bonito reflejando. */
    mats[nm++] = (material_t){ .kind = MAT_METAL,
                                .u.metal = { { 0.85f, 0.5f, 0.4f }, 0.02f } };
    const u32 mesh_mat = (u32)(nm - 1);

    obj_mesh_t mesh;
    if (!obj_load(&mesh, path)) {
        LOG_ERROR("could not load mesh from '%s'", path);
        return 0;
    }
    if (mesh.n_tris == 0 || mesh.n_verts == 0) {
        LOG_ERROR("mesh empty");
        obj_free(&mesh);
        return 0;
    }

    /* Bbox del mesh para centrar y escalar a [-1, 1]³. */
    vec3_t mn = mesh.verts[0], mx = mesh.verts[0];
    for (int i = 1; i < mesh.n_verts; ++i) {
        const vec3_t v = mesh.verts[i];
        if (v.x < mn.x) mn.x = v.x; if (v.x > mx.x) mx.x = v.x;
        if (v.y < mn.y) mn.y = v.y; if (v.y > mx.y) mx.y = v.y;
        if (v.z < mn.z) mn.z = v.z; if (v.z > mx.z) mx.z = v.z;
    }
    const vec3_t center = { (mn.x+mx.x)*0.5f, (mn.y+mx.y)*0.5f, (mn.z+mx.z)*0.5f };
    f32 ext = mx.x - mn.x;
    if (mx.y - mn.y > ext) ext = mx.y - mn.y;
    if (mx.z - mn.z > ext) ext = mx.z - mn.z;
    if (ext < 1e-6f) ext = 1.0f;
    const f32 scale = 2.0f / ext;

    /* Offset Y para que la base toque y=0. */
    const f32 min_y_after = (mn.y - center.y) * scale;
    const vec3_t offset = { 0.0f, -min_y_after, 0.0f };

    for (int i = 0; i < mesh.n_tris && n < max_items; ++i) {
        const vec3_t va = mesh.verts[mesh.tris[i][0]];
        const vec3_t vb = mesh.verts[mesh.tris[i][1]];
        const vec3_t vc = mesh.verts[mesh.tris[i][2]];
        const vec3_t pa = v3_add(v3_scale(v3_sub(va, center), scale), offset);
        const vec3_t pb = v3_add(v3_scale(v3_sub(vb, center), scale), offset);
        const vec3_t pc = v3_add(v3_scale(v3_sub(vc, center), scale), offset);
        add_triangle(arr, items, &n, pa, pb, pc, mesh_mat);
    }
    LOG_INFO("OBJ scene: %d triangles cargados de '%s'", mesh.n_tris, path);
    obj_free(&mesh);
    return n;
}

static int build_mesh_scene(hittable_t *arr, hittable_t **items, material_t *mats,
                             int max_items) {
    (void)max_items;
    int n = 0, nm = 0;

    /* Ground: huge sphere lambertian gray. */
    mats[nm++] = (material_t){ .kind = MAT_LAMBERTIAN, .u.lambertian = { { 0.5f, 0.5f, 0.5f } } };
    add_sphere(arr, items, &n, (vec3_t){ 0.0f, -1000.0f, 0.0f }, 1000.0f, (u32)(nm - 1));

    /* Mesh material: metal pulido dorado. */
    mats[nm++] = (material_t){ .kind = MAT_METAL, .u.metal = { { 0.85f, 0.75f, 0.55f }, 0.05f } };
    const u32 mesh_mat = (u32)(nm - 1);

    /* Icosphere subdivided 3 veces = 20 × 4³ = 1280 triángulos. */
    build_icosphere(arr, items, &n, (vec3_t){0, 1, 0}, 1.0f, 3, mesh_mat);

    return n;
}

static int build_cornell_scene(hittable_t *arr, hittable_t **items, material_t *mats,
                                int max_items) {
    int n = 0, nm = 0;
    (void)max_items;

    mats[nm++] = (material_t){ .kind = MAT_LAMBERTIAN, .u.lambertian = { { 0.65f, 0.05f, 0.05f } } };
    mats[nm++] = (material_t){ .kind = MAT_LAMBERTIAN, .u.lambertian = { { 0.12f, 0.45f, 0.15f } } };
    mats[nm++] = (material_t){ .kind = MAT_LAMBERTIAN, .u.lambertian = { { 0.73f, 0.73f, 0.73f } } };
    mats[nm++] = (material_t){ .kind = MAT_EMISSIVE,   .u.emissive   = { { 15.0f, 15.0f, 15.0f } } };

    const u32 RED = 0, GREEN = 1, WHITE = 2, LIGHT = 3;

    add_quad(arr, items, &n, (vec3_t){555, 0, 0},   (vec3_t){0, 555, 0}, (vec3_t){0, 0, 555}, GREEN);
    add_quad(arr, items, &n, (vec3_t){0,   0, 0},   (vec3_t){0, 555, 0}, (vec3_t){0, 0, 555}, RED);
    add_quad(arr, items, &n, (vec3_t){343, 554, 332}, (vec3_t){-130, 0, 0}, (vec3_t){0, 0, -105}, LIGHT);
    add_quad(arr, items, &n, (vec3_t){0,   0,   0}, (vec3_t){555, 0, 0}, (vec3_t){0, 0, 555}, WHITE);
    add_quad(arr, items, &n, (vec3_t){555, 555, 0}, (vec3_t){-555, 0, 0}, (vec3_t){0, 0, 555}, WHITE);
    add_quad(arr, items, &n, (vec3_t){0, 0, 555},   (vec3_t){555, 0, 0}, (vec3_t){0, 555, 0}, WHITE);

    add_box(arr, items, &n, (vec3_t){265, 0,  295}, (vec3_t){430, 330, 460}, WHITE);
    add_box(arr, items, &n, (vec3_t){130, 0,   65}, (vec3_t){295, 165, 230}, WHITE);

    return n;
}

/* ─── M9: tile-based multithreading ─────────────────────────────────── */

#define TILE_SIZE 32

typedef struct {
    int width, height;
    int spp, max_depth;
    u64 base_seed;
    const camera_t   *cam;
    const hittable_t *world;
    const material_t *mats;
    const lights_t   *lights;
    sky_t             sky;
    rgb_t            *pixels;
    int tile_size;
    int tiles_x;
} tile_job_t;

static void render_one_tile(const tile_job_t *tj, int tile_idx) {
    const int tx = tile_idx % tj->tiles_x;
    const int ty = tile_idx / tj->tiles_x;
    const int x0 = tx * tj->tile_size;
    const int y0 = ty * tj->tile_size;
    int x1 = x0 + tj->tile_size; if (x1 > tj->width)  x1 = tj->width;
    int y1 = y0 + tj->tile_size; if (y1 > tj->height) y1 = tj->height;

    /* RNG sembrado con tile_idx → mismo output independiente del orden
     * de procesamiento entre threads. Cada tile es independiente. */
    pcg32_t rng;
    pcg32_seed(&rng, tj->base_seed, (u64)tile_idx + 1ull);

    const f32 inv_spp = 1.0f / (f32)tj->spp;

    for (int j = y0; j < y1; ++j) {
        for (int i = x0; i < x1; ++i) {
            rgb_t accum = { 0.0f, 0.0f, 0.0f };
            for (int s = 0; s < tj->spp; ++s) {
                ray_t r = camera_get_ray(tj->cam, i, j, &rng);
                accum = v3_add(accum, ray_color(r, tj->max_depth,
                                                 tj->world, tj->mats, tj->lights,
                                                 tj->sky, true, &rng));
            }
            tj->pixels[j * tj->width + i] = v3_scale(accum, inv_spp);
        }
    }
}

typedef struct {
    const tile_job_t *tj;
    int               total_tiles;
    int              *next_tile;       /* shared counter */
    pthread_mutex_t  *mutex;
} thread_arg_t;

static void *worker(void *arg) {
    thread_arg_t *ta = (thread_arg_t *)arg;
    for (;;) {
        pthread_mutex_lock(ta->mutex);
        const int t = (*ta->next_tile)++;
        pthread_mutex_unlock(ta->mutex);
        if (t >= ta->total_tiles) break;
        render_one_tile(ta->tj, t);
    }
    return NULL;
}

static void render_tiles(const tile_job_t *tj, int num_threads) {
    const int tiles_y = (tj->height + tj->tile_size - 1) / tj->tile_size;
    const int total   = tj->tiles_x * tiles_y;

    if (num_threads <= 1) {
        for (int t = 0; t < total; ++t) {
            render_one_tile(tj, t);
            if ((t & 7) == 0) {
                fprintf(stderr, "\rtiles remaining: %4d ", total - t);
                fflush(stderr);
            }
        }
        return;
    }

    int next_tile = 0;
    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);

    pthread_t *threads = (pthread_t *)malloc((size_t)num_threads * sizeof(pthread_t));
    thread_arg_t *args = (thread_arg_t *)malloc((size_t)num_threads * sizeof(thread_arg_t));
    for (int i = 0; i < num_threads; ++i) {
        args[i].tj          = tj;
        args[i].total_tiles = total;
        args[i].next_tile   = &next_tile;
        args[i].mutex       = &mutex;
        pthread_create(&threads[i], NULL, worker, &args[i]);
    }
    for (int i = 0; i < num_threads; ++i) {
        pthread_join(threads[i], NULL);
    }
    pthread_mutex_destroy(&mutex);
    free(threads);
    free(args);
}

int main(int argc, char **argv) {
    const char *out_path = argc > 1 ? argv[1] : "out.ppm";
    const char *scene    = argc > 2 ? argv[2] : "cover";
    const int   spp      = argc > 3 ? atoi(argv[3])       : 10;
    const int   depth    = argc > 4 ? atoi(argv[4])       : 50;
    const u64   seed     = argc > 5 ? (u64)atoll(argv[5]) : 42ull;
    int         threads  = argc > 6 ? atoi(argv[6])       : 0;   /* 0 = auto */
    if (threads <= 0) threads = (int)sysconf(_SC_NPROCESSORS_ONLN);
    if (threads <= 0) threads = 1;

    const bool is_cornell = strcmp(scene, "cornell") == 0;
    const bool is_cover   = strcmp(scene, "cover")   == 0;
    const bool is_mesh    = strcmp(scene, "mesh")    == 0;
    const bool is_obj     = strcmp(scene, "obj")     == 0;
    const bool is_earth   = strcmp(scene, "earth")   == 0;
    if (!is_cornell && !is_cover && !is_mesh && !is_obj && !is_earth) {
        LOG_ERROR("scene desconocida: '%s'. Usá cover|cornell|mesh|obj|earth.", scene);
        return 1;
    }
    const char *aux_path = argc > 7 ? argv[7]
                                    : (is_earth ? "earth.ppm" : "model.obj");

    int width, height;
    camera_t cam;
    sky_t sky;
    if (is_cornell) {
        width = height = 400;
        camera_init(&cam,
                    (vec3_t){ 278.0f, 278.0f, -800.0f },
                    (vec3_t){ 278.0f, 278.0f,    0.0f },
                    (vec3_t){   0.0f,   1.0f,    0.0f },
                    40.0f, width, height, 0.0f, 800.0f);
        sky = (sky_t){ { 0,0,0 }, { 0,0,0 } };
    } else if (is_mesh || is_obj) {
        width = 400; height = 225;
        camera_init(&cam,
                    (vec3_t){ 4.0f, 2.5f, 4.0f },
                    (vec3_t){ 0.0f, 0.7f, 0.0f },
                    (vec3_t){ 0.0f, 1.0f, 0.0f },
                    30.0f, width, height, 0.0f, 5.0f);
        sky = (sky_t){ { 1.0f, 1.0f, 1.0f }, { 0.55f, 0.75f, 1.0f } };
    } else if (is_earth) {
        width = 400; height = 225;
        camera_init(&cam,
                    (vec3_t){ 0.0f, 2.5f, 7.0f },
                    (vec3_t){ 0.0f, 1.0f, 0.0f },
                    (vec3_t){ 0.0f, 1.0f, 0.0f },
                    30.0f, width, height, 0.0f, 7.0f);
        sky = (sky_t){ { 1.0f, 1.0f, 1.0f }, { 0.55f, 0.75f, 1.0f } };
    } else {
        width  = 400; height = 225;
        camera_init(&cam,
                    (vec3_t){ 13.0f, 2.0f, 3.0f },
                    (vec3_t){  0.0f, 0.0f, 0.0f },
                    (vec3_t){  0.0f, 1.0f, 0.0f },
                    20.0f, width, height, 0.6f, 10.0f);
        sky = (sky_t){ { 1.0f, 1.0f, 1.0f }, { 0.5f, 0.7f, 1.0f } };
    }

    static hittable_t  pool[MAX_HITTABLES];
    static hittable_t *items[MAX_HITTABLES];
    static material_t  mats[MAX_MATERIALS];

    int n_items;
    if (is_cornell) {
        n_items = build_cornell_scene(pool, items, mats, MAX_HITTABLES);
    } else if (is_mesh) {
        n_items = build_mesh_scene(pool, items, mats, MAX_HITTABLES);
    } else if (is_obj) {
        n_items = build_obj_scene(pool, items, mats, MAX_HITTABLES, aux_path);
        if (n_items == 0) return 1;
    } else if (is_earth) {
        n_items = build_earth_scene(pool, items, mats, MAX_HITTABLES, aux_path);
        if (n_items == 0) return 1;
    } else {
        pcg32_t scene_rng;
        pcg32_seed(&scene_rng, 1u, 1u);
        n_items = build_cover_scene(pool, items, mats, MAX_HITTABLES, &scene_rng);
    }

    /* Lights: cualquier item con material emisivo se agrega a la lista.
     * IMPORTANTE: hacer esto ANTES del bvh_build (que reordena items[]). */
    static hittable_t *light_items[MAX_LIGHTS];
    lights_t lights = { light_items, 0 };
    for (int i = 0; i < n_items && lights.count < MAX_LIGHTS; ++i) {
        const hittable_t *h = items[i];
        u32 mat_idx = (h->kind == HIT_QUAD)   ? h->u.quad.mat
                    : (h->kind == HIT_SPHERE) ? h->u.sphere.mat
                    : (h->kind == HIT_TRI)    ? h->u.tri.mat
                    : (u32)-1;
        if (mat_idx != (u32)-1 && mats[mat_idx].kind == MAT_EMISSIVE) {
            light_items[lights.count++] = items[i];
        }
    }

    static hittable_t bvh_arena[MAX_HITTABLES * 2];
    int n_bvh = 0;
    hittable_t *root = bvh_build(items, n_items, bvh_arena, MAX_HITTABLES * 2, &n_bvh);
    LOG_INFO("scene='%s'  items=%d  BVH nodes=%d  lights=%d",
             scene, n_items, n_bvh, lights.count);

    image_t img;
    if (image_init(&img, width, height) != 0) {
        LOG_ERROR("image_init failed");
        return 1;
    }

    LOG_INFO("rendering %dx%d, %d spp, depth %d, seed %llu, %d threads -> %s",
             width, height, spp, depth, (unsigned long long)seed, threads, out_path);

    const tile_job_t tj = {
        .width = width, .height = height,
        .spp = spp, .max_depth = depth,
        .base_seed = seed,
        .cam = &cam, .world = root, .mats = mats, .lights = &lights,
        .sky = sky,
        .pixels = img.pixels,
        .tile_size = TILE_SIZE,
        .tiles_x = (width + TILE_SIZE - 1) / TILE_SIZE,
    };

    struct timespec t_start, t_end;
    clock_gettime(CLOCK_MONOTONIC, &t_start);
    render_tiles(&tj, threads);
    clock_gettime(CLOCK_MONOTONIC, &t_end);
    const f64 elapsed = (f64)(t_end.tv_sec - t_start.tv_sec)
                      + (f64)(t_end.tv_nsec - t_start.tv_nsec) * 1e-9;
    fprintf(stderr, "\rdone in %.2fs                       \n", elapsed);

    if (image_write_ppm(&img, out_path) != 0) {
        image_free(&img);
        return 1;
    }
    LOG_INFO("wrote %s", out_path);

    image_free(&img);
    return 0;
}
