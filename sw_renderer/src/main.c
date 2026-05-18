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

    const mat4_t proj = mat4_perspective(deg2rad(75.0f),
                                          (f32)WIDTH / (f32)HEIGHT, 0.05f, 100.0f);

    vec3_t cam_pos = { 0.0f, 2.0f, 0.0f };
    f32    yaw = 0.0f, pitch = 0.0f;
    const f32 move_speed = 4.0f, look_speed = 1.8f;

    const u64 freq = SDL_GetPerformanceFrequency();
    u64 t_last      = SDL_GetPerformanceCounter();
    u32 frames      = 0;
    u32 last_fps_ms = SDL_GetTicks();
    u64 sum_drawn = 0, sum_skipped = 0;

    LOG_INFO("WASD/flechas para moverse. ESC sale.");

    bool running = true;
    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = false;
            else if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE) running = false;
        }

        const u64 t_now = SDL_GetPerformanceCounter();
        const f32 dt    = (f32)(t_now - t_last) / (f32)freq;
        t_last = t_now;

        const Uint8 *keys = SDL_GetKeyboardState(NULL);
        f32 dz = 0, dx = 0, dyaw = 0, dpitch = 0;
        /* W = forward (cam_pos += fwd_xz). fwd_xz = (-sin(yaw), 0, -cos(yaw)),
         * que en yaw=0 es (0,0,-1) → moviendo cam_pos en esa dir baja z, que
         * es "adelante" porque el view matrix mira -z. */
        if (keys[SDL_SCANCODE_W]) dz += 1.0f;
        if (keys[SDL_SCANCODE_S]) dz -= 1.0f;
        if (keys[SDL_SCANCODE_A]) dx -= 1.0f;
        if (keys[SDL_SCANCODE_D]) dx += 1.0f;
        /* Para "look right" necesitamos yaw negativo (forward rota hacia +x).
         * Para "look up" necesitamos pitch positivo (forward.y > 0). */
        if (keys[SDL_SCANCODE_LEFT])  dyaw   += 1.0f;
        if (keys[SDL_SCANCODE_RIGHT]) dyaw   -= 1.0f;
        if (keys[SDL_SCANCODE_UP])    dpitch += 1.0f;
        if (keys[SDL_SCANCODE_DOWN])  dpitch -= 1.0f;

        yaw   += dyaw   * look_speed * dt;
        pitch += dpitch * look_speed * dt;
        if (pitch >  1.4f) pitch =  1.4f;
        if (pitch < -1.4f) pitch = -1.4f;

        const vec3_t fwd_xz   = { -sinf(yaw), 0.0f, -cosf(yaw) };
        const vec3_t right_xz = {  cosf(yaw), 0.0f, -sinf(yaw) };
        cam_pos = v3_add(cam_pos, v3_scale(fwd_xz,   dz * move_speed * dt));
        cam_pos = v3_add(cam_pos, v3_scale(right_xz, dx * move_speed * dt));
        if (cam_pos.x >  5.7f) cam_pos.x =  5.7f;
        if (cam_pos.x < -5.7f) cam_pos.x = -5.7f;
        if (cam_pos.z >  5.7f) cam_pos.z =  5.7f;
        if (cam_pos.z < -5.7f) cam_pos.z = -5.7f;

        const mat4_t view = mat4_mul(
            mat4_rotate_x(-pitch),
            mat4_mul(mat4_rotate_y(-yaw),
                      mat4_translate(v3_neg(cam_pos))));
        const mat4_t mvp  = mat4_mul(proj, view);

        framebuf_clear_color(&fb, argb8888(0, 0, 0));

        /* Sort + project (single-threaded). */
        f32 dist_sq[N_FACES];
        for (int i = 0; i < N_FACES; ++i) {
            order[i] = i;
            dist_sq[i] = v3_length_sq(v3_sub(face_center(&SCENE_FACES[i]), cam_pos));
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
            LOG_INFO("[%d threads] fps=%u  fill=%.3f×  effic=%.0f%%  pos=(%.1f, %.1f, %.1f)",
                     pool.n, frames, fill, effic * 100.0f,
                     cam_pos.x, cam_pos.y, cam_pos.z);
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
