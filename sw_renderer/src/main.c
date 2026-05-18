#include "common/types.h"
#include "common/log.h"
#include "common/util.h"
#include "common/vec.h"
#include "common/mat.h"
#include "common/plane.h"
#include "framebuffer.h"
#include "draw_tri.h"
#include "texture.h"
#include "bsp.h"
#include "span_buffer.h"
#include "rast_scan.h"
#include "lightmap.h"
#include "draw_line.h"
#include "text.h"

#include <stdio.h>   /* snprintf for HUD */

#include <SDL2/SDL.h>
#include <pthread.h>
#include <unistd.h>
#include <math.h>
#include <stdlib.h>
#include <stdbool.h>

/*
 * M10 — Multithread por horizontal stripes (M9 introdujo subspan FDIV).
 *
 * Pipeline por frame:
 *   1. Main thread: compute MVP, sort caras front-to-back, project los
 *      4 verts de cada cara una sola vez al array compartido.
 *   2. Main barrier-signal a los N workers.
 *   3. Cada worker renderiza su stripe [y_start, y_end) iterando las caras
 *      en orden y rasterizando con rast_scan_textured (con y-clip).
 *   4. Main espera a que todos los workers terminen vía done_barrier.
 *   5. Main hace blit + present.
 *
 * Cada thread tiene su PROPIO span_buffer (covered bitmap) cubriendo el
 * full screen — solo escribe en su stripe, así no hay races. Persistent
 * thread pool con pthread_barrier_t para evitar overhead de spawn/join
 * por frame.
 *
 * Run con `./sw_renderer [N]` donde N = thread count (default: auto via
 * sysconf, capeado a 8 — rendimientos decrecientes más allá).
 */

#define WIDTH    800
#define HEIGHT   600
#define N_FACES  10
#define LM_DIM   32
#define MAX_THREADS 16

static u32 argb8888(u8 r, u8 g, u8 b) {
    return (0xFFu << 24) | ((u32)r << 16) | ((u32)g << 8) | (u32)b;
}

/* ─── Escena (misma de M7-M9) ───────────────────────────────────────── */

static const bsp_face_t SCENE_FACES[N_FACES] = {
    { .verts = { {-6, 0,  6}, { 6, 0,  6}, { 6, 0, -6}, {-6, 0, -6} },
      .uvs   = { {-6,  6}, { 6,  6}, { 6, -6}, {-6, -6} }, .tex = tex_floor },
    { .verts = { {-6, 4, -6}, { 6, 4, -6}, { 6, 4,  6}, {-6, 4,  6} },
      .uvs   = { {-6, -6}, { 6, -6}, { 6,  6}, {-6,  6} }, .tex = tex_ceiling },
    { .verts = { {-6, 0, 6}, {-6, 4, 6}, { 6, 4, 6}, { 6, 0, 6} },
      .uvs   = { {-6, 0}, {-6, 4}, { 6, 4}, { 6, 0} }, .tex = tex_brick },
    { .verts = { { 6, 0, -6}, { 6, 4, -6}, {-6, 4, -6}, {-6, 0, -6} },
      .uvs   = { {-6, 0}, {-6, 4}, { 6, 4}, { 6, 0} }, .tex = tex_brick },
    { .verts = { { 6, 0,  6}, { 6, 4,  6}, { 6, 4, -6}, { 6, 0, -6} },
      .uvs   = { {-6, 0}, {-6, 4}, { 6, 4}, { 6, 0} }, .tex = tex_brick },
    { .verts = { {-6, 0, -6}, {-6, 4, -6}, {-6, 4,  6}, {-6, 0,  6} },
      .uvs   = { {-6, 0}, {-6, 4}, { 6, 4}, { 6, 0} }, .tex = tex_brick },
    { .verts = { {-0.5f, 0, -2}, { 0.5f, 0, -2}, { 0.5f, 4, -2}, {-0.5f, 4, -2} },
      .uvs   = { {-0.5f, 0}, { 0.5f, 0}, { 0.5f, 4}, {-0.5f, 4} }, .tex = tex_brick },
    { .verts = { { 0.5f, 0, -3}, {-0.5f, 0, -3}, {-0.5f, 4, -3}, { 0.5f, 4, -3} },
      .uvs   = { { 0.5f, 0}, {-0.5f, 0}, {-0.5f, 4}, { 0.5f, 4} }, .tex = tex_brick },
    { .verts = { { 0.5f, 0, -2}, { 0.5f, 0, -3}, { 0.5f, 4, -3}, { 0.5f, 4, -2} },
      .uvs   = { {-2, 0}, {-3, 0}, {-3, 4}, {-2, 4} }, .tex = tex_brick },
    { .verts = { {-0.5f, 0, -3}, {-0.5f, 0, -2}, {-0.5f, 4, -2}, {-0.5f, 4, -3} },
      .uvs   = { {-3, 0}, {-2, 0}, {-2, 4}, {-3, 4} }, .tex = tex_brick },
};

/* Ray-vs-quad intersection. Devuelve t del hit en (0, ∞), o -1 si miss. */
static f32 face_intersect(const bsp_face_t *face, vec3_t orig, vec3_t dir) {
    const vec3_t e1 = v3_sub(face->verts[1], face->verts[0]);
    const vec3_t e2 = v3_sub(face->verts[3], face->verts[0]);
    const vec3_t n  = v3_cross(e1, e2);
    const f32    denom = v3_dot(n, dir);
    if (fabsf(denom) < 1e-8f) return -1.0f;
    const f32 D = v3_dot(n, face->verts[0]);
    const f32 t = (D - v3_dot(n, orig)) / denom;
    if (t < 0.001f) return -1.0f;
    const vec3_t P     = v3_add(orig, v3_scale(dir, t));
    const vec3_t local = v3_sub(P, face->verts[0]);
    const f32 a = v3_dot(local, e1) / v3_dot(e1, e1);
    const f32 b = v3_dot(local, e2) / v3_dot(e2, e2);
    if (a < 0.0f || a > 1.0f || b < 0.0f || b > 1.0f) return -1.0f;
    return t;
}

/* Picking: convierte (screen_x, screen_y) a un rayo y testea contra cada
 * face. Devuelve idx de la más cercana, o -1 si no hay hit. */
static int pick_face(int sx, int sy,
                     vec3_t eye, vec3_t target, vec3_t world_up,
                     f32 vfov_rad, f32 aspect) {
    const f32 ndc_x = 2.0f * (f32)sx / (f32)WIDTH  - 1.0f;
    const f32 ndc_y = 1.0f - 2.0f * (f32)sy / (f32)HEIGHT;
    const f32 t_y   = tanf(vfov_rad * 0.5f);
    const f32 t_x   = t_y * aspect;

    const vec3_t fwd   = v3_normalize(v3_sub(target, eye));
    const vec3_t right = v3_normalize(v3_cross(fwd, world_up));
    const vec3_t up    = v3_normalize(v3_cross(right, fwd));

    const vec3_t ray_dir = v3_add(v3_add(
        v3_scale(right, ndc_x * t_x),
        v3_scale(up,    ndc_y * t_y)),
        fwd);

    int best = -1;
    f32 best_t = INFINITY;
    for (int i = 0; i < N_FACES; ++i) {
        const f32 t = face_intersect(&SCENE_FACES[i], eye, ray_dir);
        if (t > 0.0f && t < best_t) {
            best_t = t;
            best = i;
        }
    }
    return best;
}

static vec3_t face_center(const bsp_face_t *f) {
    return (vec3_t){
        (f->verts[0].x + f->verts[1].x + f->verts[2].x + f->verts[3].x) * 0.25f,
        (f->verts[0].y + f->verts[1].y + f->verts[2].y + f->verts[3].y) * 0.25f,
        (f->verts[0].z + f->verts[1].z + f->verts[2].z + f->verts[3].z) * 0.25f,
    };
}

/* ─── Thread pool ───────────────────────────────────────────────────── */

typedef struct {
    int            id;
    int            y_start, y_end;     /* stripe range, [y_start, y_end) */
    span_buffer_t  sb;                 /* per-thread, cubre full screen */
} worker_t;

typedef struct {
    int                n;
    pthread_t          tids[MAX_THREADS];
    worker_t           workers[MAX_THREADS];
    pthread_barrier_t  start_barrier;  /* main + N workers (count = n+1) */
    pthread_barrier_t  done_barrier;   /* same */
    volatile bool      quit;

    /* Shared per-frame state — main writes, workers read after start_barrier. */
    framebuf_t           *fb;
    const clip_vert_t   (*clip_verts)[4];        /* [N_FACES][4] clip-space */
    const int            *order;                  /* sorted indices */
    const lightmap_t     *lightmaps;
    const bsp_face_t     *faces;
    f32                   near_w;                 /* clip plane threshold */
} pool_t;

/* Convierte un clip_vert_t a screen_vert_t haciendo el perspective divide. */
static screen_vert_t to_screen(clip_vert_t cv, int W, int H) {
    const f32 invw = 1.0f / cv.w;
    return (screen_vert_t){
        .x     = (cv.x * invw * 0.5f + 0.5f) * (f32)W,
        .y     = (1.0f - (cv.y * invw * 0.5f + 0.5f)) * (f32)H,
        .z     = cv.z * invw,
        .u     = cv.u,
        .v     = cv.v,
        .inv_w = invw,
    };
}

static void render_stripe(worker_t *w, pool_t *pool) {
    span_buffer_clear(&w->sb);
    static const u8 quad_tris[2][3] = { {0, 1, 2}, {0, 2, 3} };

    for (int oi = 0; oi < N_FACES; ++oi) {
        const int idx = pool->order[oi];
        const bsp_face_t   *face = &pool->faces[idx];
        const clip_vert_t  *cv   = pool->clip_verts[idx];
        const lightmap_t   *lm   = &pool->lightmaps[idx];

        for (int t = 0; t < 2; ++t) {
            const clip_vert_t a = cv[quad_tris[t][0]];
            const clip_vert_t b = cv[quad_tris[t][1]];
            const clip_vert_t c = cv[quad_tris[t][2]];

            /* Clip contra near plane → 0..2 sub-triangles. */
            clip_vert_t clipped[6];
            const int n_tris = clip_triangle_near(a, b, c, pool->near_w, clipped);

            for (int k = 0; k < n_tris; ++k) {
                const screen_vert_t s0 = to_screen(clipped[k*3 + 0], pool->fb->w, pool->fb->h);
                const screen_vert_t s1 = to_screen(clipped[k*3 + 1], pool->fb->w, pool->fb->h);
                const screen_vert_t s2 = to_screen(clipped[k*3 + 2], pool->fb->w, pool->fb->h);
                rast_scan_textured(pool->fb, &w->sb, s0, s1, s2, face->tex, lm,
                                   w->y_start, w->y_end);
            }
        }
    }
}

typedef struct { worker_t *w; pool_t *pool; } worker_arg_t;

static void *worker_fn(void *arg_p) {
    worker_arg_t *wa = (worker_arg_t *)arg_p;
    for (;;) {
        pthread_barrier_wait(&wa->pool->start_barrier);
        if (wa->pool->quit) break;
        render_stripe(wa->w, wa->pool);
        pthread_barrier_wait(&wa->pool->done_barrier);
    }
    return NULL;
}

/* worker_args persistente (no en stack del main thread) — los workers les
 * apuntan durante toda la vida del pool. */
static worker_arg_t worker_args[MAX_THREADS];

static bool pool_init(pool_t *pool, int n) {
    if (n > MAX_THREADS) n = MAX_THREADS;
    if (n < 1) n = 1;
    pool->n = n;
    pool->quit = false;
    pthread_barrier_init(&pool->start_barrier, NULL, (unsigned)(n + 1));
    pthread_barrier_init(&pool->done_barrier,  NULL, (unsigned)(n + 1));

    /* Divide los HEIGHT scanlines en n stripes consecutivos. */
    for (int i = 0; i < n; ++i) {
        pool->workers[i].id      = i;
        pool->workers[i].y_start = (HEIGHT * i)       / n;
        pool->workers[i].y_end   = (HEIGHT * (i + 1)) / n;
        if (!span_buffer_init(&pool->workers[i].sb, WIDTH, HEIGHT)) {
            LOG_ERROR("oom worker[%d] span buffer", i);
            return false;
        }
        worker_args[i].w    = &pool->workers[i];
        worker_args[i].pool = pool;
        if (pthread_create(&pool->tids[i], NULL, worker_fn, &worker_args[i]) != 0) {
            LOG_ERROR("pthread_create failed for worker %d", i);
            return false;
        }
    }
    return true;
}

static void pool_destroy(pool_t *pool) {
    pool->quit = true;
    pthread_barrier_wait(&pool->start_barrier);  /* despertar workers para que vean quit */
    for (int i = 0; i < pool->n; ++i) {
        pthread_join(pool->tids[i], NULL);
        span_buffer_free(&pool->workers[i].sb);
    }
    pthread_barrier_destroy(&pool->start_barrier);
    pthread_barrier_destroy(&pool->done_barrier);
}

static void pool_render_frame(pool_t *pool) {
    pthread_barrier_wait(&pool->start_barrier);  /* signal workers */
    pthread_barrier_wait(&pool->done_barrier);   /* wait for completion */
}

/* ─── main ──────────────────────────────────────────────────────────── */

int main(int argc, char **argv) {
    int n_threads = (argc > 1) ? atoi(argv[1]) : 0;
    if (n_threads <= 0) {
        n_threads = (int)sysconf(_SC_NPROCESSORS_ONLN);
        if (n_threads > 8) n_threads = 8;   /* diminishing returns más allá */
        if (n_threads < 1) n_threads = 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        LOG_ERROR("SDL_Init: %s", SDL_GetError());
        return 1;
    }

    SDL_Window *win = SDL_CreateWindow(
        "Sotark — sw_renderer M10 (multithread stripes)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT, 0);
    if (!win) { LOG_ERROR("SDL_CreateWindow: %s", SDL_GetError()); SDL_Quit(); return 1; }

    SDL_Surface *back = SDL_CreateRGBSurfaceWithFormat(0, WIDTH, HEIGHT, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!back) { LOG_ERROR("SDL_CreateRGBSurfaceWithFormat: %s", SDL_GetError());
                  SDL_DestroyWindow(win); SDL_Quit(); return 1; }

    const int pitch_pixels = back->pitch / 4;
    f32 *depth_buf = (f32 *)malloc(sizeof(f32) * (size_t)pitch_pixels * (size_t)HEIGHT);
    if (!depth_buf) { LOG_ERROR("oom depth"); return 1; }

    framebuf_t fb = {
        .color = (u32 *)back->pixels, .depth = depth_buf,
        .w = WIDTH, .h = HEIGHT, .pitch_pixels = pitch_pixels,
    };

    /* Bake lightmaps (M8). */
    static u32 lm_storage[N_FACES][LM_DIM * LM_DIM];
    static lightmap_t lightmaps[N_FACES];
    for (int i = 0; i < N_FACES; ++i) {
        lightmaps[i] = (lightmap_t){ .texels = lm_storage[i], .w = LM_DIM, .h = LM_DIM };
    }
    const vec3_t light_pos       = { 0.0f, 3.8f, 4.0f };
    const f32    light_intensity = 8.0f;
    const f32    ambient_level   = 0.08f;
    const u64 t_bake_start = SDL_GetPerformanceCounter();
    for (int i = 0; i < N_FACES; ++i) {
        lightmap_bake(&lightmaps[i], &SCENE_FACES[i],
                      SCENE_FACES, N_FACES, i,
                      light_pos, light_intensity, ambient_level);
    }
    const f32 bake_ms = (f32)(SDL_GetPerformanceCounter() - t_bake_start) /
                        (f32)SDL_GetPerformanceFrequency() * 1000.0f;
    LOG_INFO("lightmaps baked in %.1f ms", bake_ms);

    /* Init thread pool. */
    static pool_t pool;
    static clip_vert_t clip_verts[N_FACES][4];
    static int         order[N_FACES];
    pool.fb         = &fb;
    pool.clip_verts = (const clip_vert_t (*)[4])clip_verts;
    pool.order      = order;
    pool.lightmaps  = lightmaps;
    pool.faces      = SCENE_FACES;
    pool.near_w     = 0.1f;   /* match perspective near; clip.w >= this required */
    if (!pool_init(&pool, n_threads)) {
        LOG_ERROR("pool init failed");
        return 1;
    }
    LOG_INFO("M10 — pool con %d threads (cada stripe ~%d scanlines)",
             pool.n, HEIGHT / pool.n);

    const mat4_t proj = mat4_perspective(deg2rad(60.0f),
                                          (f32)WIDTH / (f32)HEIGHT, 0.05f, 100.0f);

    /*
     * Cámara orbital tipo editor:
     *   - eye = target + radius·(sin(yaw)cos(pitch), sin(pitch), cos(yaw)cos(pitch))
     *   - left-drag  = orbit (rotate yaw/pitch)
     *   - right-drag = pan (translate target in cam plane)
     *   - wheel      = zoom (change radius)
     *   - F          = reset to default
     */
    vec3_t cam_target = { 0.0f, 2.0f, -2.5f };  /* pillar center */
    f32    cam_yaw    = 0.0f;
    f32    cam_pitch  = 0.2f;                    /* slight tilt down */
    f32    cam_radius = 6.5f;
    const f32 ORBIT_SPEED = 0.008f;
    const f32 PAN_SPEED   = 0.012f;
    const f32 ZOOM_FACTOR = 1.12f;

    int mouse_left_down = 0;
    int mouse_right_down = 0;
    int last_mouse_x = 0, last_mouse_y = 0;
    int mouse_press_x = 0, mouse_press_y = 0;
    int mouse_left_dragged = 0;

    int selected_face = -1;

    u32 frames      = 0;
    u32 last_fps_ms = SDL_GetTicks();
    u64 sum_drawn = 0, sum_skipped = 0;

    LOG_INFO("Editor E1 — orbit camera.");
    LOG_INFO("  left-drag = orbit, right-drag = pan, wheel = zoom, F = reset, ESC sale.");

    bool running = true;
    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = false;
            else if (ev.type == SDL_KEYDOWN) {
                if (ev.key.keysym.sym == SDLK_ESCAPE && selected_face < 0) running = false;
                else if (ev.key.keysym.sym == SDLK_f) {
                    /* F: focus selected (frame view on selection), o reset si nada. */
                    if (selected_face >= 0) {
                        cam_target = face_center(&SCENE_FACES[selected_face]);
                        cam_radius = 4.5f;
                    } else {
                        cam_target = (vec3_t){ 0.0f, 2.0f, -2.5f };
                        cam_yaw    = 0.0f;
                        cam_pitch  = 0.2f;
                        cam_radius = 6.5f;
                    }
                }
                else if (ev.key.keysym.sym == SDLK_ESCAPE && selected_face >= 0) {
                    /* ESC también deselecciona. Se sigue saliendo si no hay selección. */
                    selected_face = -1;
                }
            }
            else if (ev.type == SDL_MOUSEBUTTONDOWN) {
                if (ev.button.button == SDL_BUTTON_LEFT) {
                    mouse_left_down    = 1;
                    mouse_left_dragged = 0;
                    mouse_press_x      = ev.button.x;
                    mouse_press_y      = ev.button.y;
                }
                if (ev.button.button == SDL_BUTTON_RIGHT) mouse_right_down = 1;
                last_mouse_x = ev.button.x;
                last_mouse_y = ev.button.y;
            }
            else if (ev.type == SDL_MOUSEBUTTONUP) {
                if (ev.button.button == SDL_BUTTON_LEFT) {
                    mouse_left_down = 0;
                    /* Click sin drag = pick. */
                    if (!mouse_left_dragged) {
                        const vec3_t eye_pick = {
                            cam_target.x + cam_radius * sinf(cam_yaw) * cosf(cam_pitch),
                            cam_target.y + cam_radius * sinf(cam_pitch),
                            cam_target.z + cam_radius * cosf(cam_yaw) * cosf(cam_pitch),
                        };
                        selected_face = pick_face(ev.button.x, ev.button.y,
                                                   eye_pick, cam_target,
                                                   (vec3_t){0.0f, 1.0f, 0.0f},
                                                   deg2rad(60.0f),
                                                   (f32)WIDTH / (f32)HEIGHT);
                        LOG_INFO("picked face: %d", selected_face);
                    }
                }
                if (ev.button.button == SDL_BUTTON_RIGHT) mouse_right_down = 0;
            }
            else if (ev.type == SDL_MOUSEMOTION) {
                const int dx = ev.motion.x - last_mouse_x;
                const int dy = ev.motion.y - last_mouse_y;
                last_mouse_x = ev.motion.x;
                last_mouse_y = ev.motion.y;
                if (mouse_left_down) {
                    const int total_dx = ev.motion.x - mouse_press_x;
                    const int total_dy = ev.motion.y - mouse_press_y;
                    if (abs(total_dx) + abs(total_dy) > 4) mouse_left_dragged = 1;
                    /* Orbit: drag horizontal = yaw, vertical = pitch. */
                    cam_yaw   -= (f32)dx * ORBIT_SPEED;
                    cam_pitch -= (f32)dy * ORBIT_SPEED;
                    if (cam_pitch >  1.5f) cam_pitch =  1.5f;
                    if (cam_pitch < -1.5f) cam_pitch = -1.5f;
                } else if (mouse_right_down) {
                    /* Pan: mover target a lo largo de los ejes right/up de la cámara. */
                    const vec3_t fwd = {
                        sinf(cam_yaw) * cosf(cam_pitch),
                        sinf(cam_pitch),
                        cosf(cam_yaw) * cosf(cam_pitch),
                    };
                    const vec3_t world_up = { 0.0f, 1.0f, 0.0f };
                    const vec3_t right = v3_normalize(v3_cross(fwd, world_up));
                    const vec3_t up    = v3_normalize(v3_cross(right, fwd));
                    const f32 scale = cam_radius * PAN_SPEED;
                    cam_target = v3_add(cam_target, v3_scale(right,  -(f32)dx * scale));
                    cam_target = v3_add(cam_target, v3_scale(up,      (f32)dy * scale));
                }
            }
            else if (ev.type == SDL_MOUSEWHEEL) {
                if (ev.wheel.y > 0)      cam_radius /= ZOOM_FACTOR;
                else if (ev.wheel.y < 0) cam_radius *= ZOOM_FACTOR;
                if (cam_radius <  0.5f)  cam_radius =  0.5f;
                if (cam_radius > 40.0f)  cam_radius = 40.0f;
            }
        }

        /* Computar eye desde orbit params. */
        const vec3_t cam_eye = {
            cam_target.x + cam_radius * sinf(cam_yaw) * cosf(cam_pitch),
            cam_target.y + cam_radius * sinf(cam_pitch),
            cam_target.z + cam_radius * cosf(cam_yaw) * cosf(cam_pitch),
        };
        const mat4_t view = mat4_look_at(cam_eye, cam_target, (vec3_t){0.0f, 1.0f, 0.0f});
        const mat4_t mvp  = mat4_mul(proj, view);

        framebuf_clear_color(&fb, argb8888(0, 0, 0));

        /* Sort + project (single-threaded). */
        f32 dist_sq[N_FACES];
        for (int i = 0; i < N_FACES; ++i) {
            order[i] = i;
            dist_sq[i] = v3_length_sq(v3_sub(face_center(&SCENE_FACES[i]), cam_eye));
        }
        for (int i = 1; i < N_FACES; ++i) {
            const int key = order[i];
            const f32 kd  = dist_sq[key];
            int j = i - 1;
            while (j >= 0 && dist_sq[order[j]] > kd) {
                order[j + 1] = order[j];
                j--;
            }
            order[j + 1] = key;
        }
        /* Project a clip space (sin perspective divide — eso lo hacemos POST
         * near-plane clipping, en cada worker). */
        for (int i = 0; i < N_FACES; ++i) {
            const bsp_face_t *face = &SCENE_FACES[i];
            for (int v = 0; v < 4; ++v) {
                const vec4_t v4   = { face->verts[v].x, face->verts[v].y, face->verts[v].z, 1.0f };
                const vec4_t clip = mat4_mul_vec4(mvp, v4);
                clip_verts[i][v]  = (clip_vert_t){
                    .x = clip.x, .y = clip.y, .z = clip.z, .w = clip.w,
                    .u = face->uvs[v].x, .v = face->uvs[v].y,
                };
            }
        }

        /* Dispatch a workers + wait. */
        pool_render_frame(&pool);

        /* ─── HUD overlay (single-threaded, post workers) ─── */
        const u64 hud_now = SDL_GetPerformanceCounter();
        static u64 last_hud_ts = 0;
        static u32 cached_fps = 0;
        static u32 hud_frame_count = 0;
        hud_frame_count++;
        if (last_hud_ts == 0) last_hud_ts = hud_now;
        const f64 elapsed_s = (f64)(hud_now - last_hud_ts) / (f64)SDL_GetPerformanceFrequency();
        if (elapsed_s >= 0.25) {
            cached_fps = (u32)((f64)hud_frame_count / elapsed_s);
            hud_frame_count = 0;
            last_hud_ts = hud_now;
        }

        /* Selection outline overlay (single-threaded, post workers). Dibuja
         * los 4 edges del quad seleccionado como wireframe naranja. */
        if (selected_face >= 0) {
            const bsp_face_t *sf = &SCENE_FACES[selected_face];
            int sx[4], sy[4];
            bool all_in_front = true;
            for (int i = 0; i < 4; ++i) {
                const vec4_t v4 = { sf->verts[i].x, sf->verts[i].y, sf->verts[i].z, 1.0f };
                const vec4_t clip = mat4_mul_vec4(mvp, v4);
                if (clip.w <= 0.1f) { all_in_front = false; break; }
                const f32 invw = 1.0f / clip.w;
                sx[i] = (int)((clip.x * invw * 0.5f + 0.5f) * (f32)WIDTH);
                sy[i] = (int)((1.0f - (clip.y * invw * 0.5f + 0.5f)) * (f32)HEIGHT);
            }
            if (all_in_front) {
                const u32 outline = argb8888(255, 180, 0);
                for (int i = 0; i < 4; ++i) {
                    const int j = (i + 1) % 4;
                    draw_line(&fb, sx[i], sy[i], sx[j], sy[j], outline);
                    /* Doble pixel para más visibilidad */
                    draw_line(&fb, sx[i] + 1, sy[i], sx[j] + 1, sy[j], outline);
                }
            }
        }

        /* ─── HUD text overlay ─── */
        char buf[96];
        const u32 hud_color = argb8888(255, 255, 255);
        /* Semi-transparent black backdrop (drawn solid for now; alpha later). */
        draw_rect(&fb, 8, 8, 360, 64, argb8888(0, 0, 0));
        snprintf(buf, sizeof(buf), "FPS %u  THREADS %d", cached_fps, pool.n);
        draw_text_shadowed(&fb, 14, 14, buf, hud_color);
        if (selected_face >= 0) {
            snprintf(buf, sizeof(buf), "SELECTED FACE %d", selected_face);
        } else {
            snprintf(buf, sizeof(buf), "SELECTED NONE  CLICK TO PICK");
        }
        draw_text_shadowed(&fb, 14, 28, buf, hud_color);
        snprintf(buf, sizeof(buf), "TARGET %.1f %.1f %.1f  R %.1f",
                 (f64)cam_target.x, (f64)cam_target.y, (f64)cam_target.z, (f64)cam_radius);
        draw_text_shadowed(&fb, 14, 42, buf, hud_color);
        snprintf(buf, sizeof(buf), "L-DRAG ORBIT  R-DRAG PAN  WHEEL ZOOM  F RESET");
        draw_text_shadowed(&fb, 14, 56, buf, argb8888(180, 180, 180));

        SDL_Surface *win_surface = SDL_GetWindowSurface(win);
        if (win_surface) {
            SDL_BlitSurface(back, NULL, win_surface, NULL);
            SDL_UpdateWindowSurface(win);
        }

        frames++;
        for (int i = 0; i < pool.n; ++i) {
            sum_drawn   += (u64)pool.workers[i].sb.pixels_drawn;
            sum_skipped += (u64)pool.workers[i].sb.pixels_skipped;
        }

        const u32 now_ms = SDL_GetTicks();
        if (now_ms - last_fps_ms >= 1000) {
            const u64 total = sum_drawn + sum_skipped;
            const f32 fill  = (f32)sum_drawn / (f32)(WIDTH * HEIGHT * frames);
            const f32 effic = total ? (f32)sum_drawn / (f32)total : 0.0f;
            LOG_INFO("[%d threads] fps=%u  fill=%.3f×  effic=%.0f%%  target=(%.1f, %.1f, %.1f) r=%.1f",
                     pool.n, frames, fill, effic * 100.0f,
                     cam_target.x, cam_target.y, cam_target.z, cam_radius);
            frames = 0; sum_drawn = sum_skipped = 0;
            last_fps_ms = now_ms;
        }
    }

    pool_destroy(&pool);
    free(depth_buf);
    SDL_FreeSurface(back);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
