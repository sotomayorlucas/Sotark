// Sotark sw_renderer — C++26 port of the M10-era multithread editor.
//
// Threading model: persistent std::jthread pool + std::barrier (replaces
// pthread + pthread_barrier_t). Per-stripe work; each worker has its own
// span buffer covering the full screen but only writes to its stripe range.

#include <atomic>
#include <barrier>
#include <thread>
#include <array>
#include <vector>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <print>
#include <format>
#include <limits>
#include <algorithm>
#include <span>

#include <SDL2/SDL.h>

import sotark.common;
import sotark.sw;

using namespace sotark;
using namespace sotark::sw;

namespace {

constexpr int WIDTH       = 800;
constexpr int HEIGHT      = 600;
constexpr int MAX_THREADS = 16;

constexpr u32 argb8888(u8 r, u8 g, u8 b) noexcept {
    return (0xFFu << 24) | (static_cast<u32>(r) << 16) | (static_cast<u32>(g) << 8) | static_cast<u32>(b);
}

Scene g_scene;

// Ray-vs-quad intersection.
f32 face_intersect(const BspFace& face, Vec3 orig, Vec3 dir) noexcept {
    const Vec3 e1 = face.verts[1] - face.verts[0];
    const Vec3 e2 = face.verts[3] - face.verts[0];
    const Vec3 n  = cross(e1, e2);
    const f32  denom = dot(n, dir);
    if (std::abs(denom) < 1e-8f) return -1.0f;
    const f32 D = dot(n, face.verts[0]);
    const f32 t = (D - dot(n, orig)) / denom;
    if (t < 0.001f) return -1.0f;
    const Vec3 P     = orig + dir * t;
    const Vec3 local = P - face.verts[0];
    const f32  a = dot(local, e1) / dot(e1, e1);
    const f32  b = dot(local, e2) / dot(e2, e2);
    if (a < 0.0f || a > 1.0f || b < 0.0f || b > 1.0f) return -1.0f;
    return t;
}

int pick_face(int sx, int sy, Vec3 eye, Vec3 target, Vec3 world_up,
              f32 vfov_rad, f32 aspect) noexcept {
    const f32 ndc_x = 2.0f * static_cast<f32>(sx) / static_cast<f32>(WIDTH)  - 1.0f;
    const f32 ndc_y = 1.0f - 2.0f * static_cast<f32>(sy) / static_cast<f32>(HEIGHT);
    const f32 t_y   = std::tan(vfov_rad * 0.5f);
    const f32 t_x   = t_y * aspect;

    const Vec3 fwd   = normalize(target - eye);
    const Vec3 right = normalize(cross(fwd, world_up));
    const Vec3 up    = normalize(cross(right, fwd));

    const Vec3 ray_dir = right * (ndc_x * t_x) + up * (ndc_y * t_y) + fwd;

    int best = -1;
    f32 best_t = std::numeric_limits<f32>::infinity();
    const auto faces = g_scene.faces();
    for (int i = 0; i < g_scene.n_faces(); ++i) {
        const f32 t = face_intersect(faces[i], eye, ray_dir);
        if (t > 0.0f && t < best_t) { best_t = t; best = i; }
    }
    return best;
}

Vec3 face_center(const BspFace& f) noexcept {
    return Vec3{
        (f.verts[0].x + f.verts[1].x + f.verts[2].x + f.verts[3].x) * 0.25f,
        (f.verts[0].y + f.verts[1].y + f.verts[2].y + f.verts[3].y) * 0.25f,
        (f.verts[0].z + f.verts[1].z + f.verts[2].z + f.verts[3].z) * 0.25f,
    };
}

// ─── jthread + barrier thread pool ──────────────────────────────────
struct Worker {
    int         id{0};
    int         y_start{0}, y_end{0};
    SpanBuffer  sb;
};

ScreenVert to_screen(ClipVert cv, int W, int H) noexcept {
    const f32 invw = 1.0f / cv.w;
    return ScreenVert{
        (cv.x * invw * 0.5f + 0.5f) * static_cast<f32>(W),
        (1.0f - (cv.y * invw * 0.5f + 0.5f)) * static_cast<f32>(H),
        cv.z * invw,
        cv.u, cv.v,
        invw,
    };
}

class Pool {
public:
    explicit Pool(int n) : n_(std::clamp(n, 1, MAX_THREADS)),
                            start_(n_ + 1),
                            done_(n_ + 1),
                            workers_(static_cast<usize>(n_)) {
        for (int i = 0; i < n_; ++i) {
            workers_[i].id      = i;
            workers_[i].y_start = (HEIGHT * i)       / n_;
            workers_[i].y_end   = (HEIGHT * (i + 1)) / n_;
            workers_[i].sb      = SpanBuffer(WIDTH, HEIGHT);
        }
        for (int i = 0; i < n_; ++i) {
            threads_.emplace_back([this, i] {
                for (;;) {
                    start_.arrive_and_wait();
                    if (quit_.load(std::memory_order_acquire)) return;
                    render_stripe(workers_[i]);
                    done_.arrive_and_wait();
                }
            });
        }
    }

    ~Pool() {
        quit_.store(true, std::memory_order_release);
        start_.arrive_and_wait();
        // jthreads join on destruction.
    }

    void render_frame(Framebuf& fb,
                      std::span<const std::array<ClipVert, 4>> clip_verts,
                      std::span<const int> order,
                      std::span<const Lightmap> lightmaps,
                      std::span<const BspFace> faces,
                      int n_faces, f32 near_w) {
        fb_         = &fb;
        clip_verts_ = clip_verts;
        order_      = order;
        lightmaps_  = lightmaps;
        faces_      = faces;
        n_faces_    = n_faces;
        near_w_     = near_w;

        start_.arrive_and_wait();
        done_.arrive_and_wait();
    }

    int n() const noexcept { return n_; }
    std::span<const Worker> workers() const noexcept { return workers_; }

private:
    void render_stripe(Worker& w) {
        w.sb.clear();
        static constexpr std::array<std::array<u8, 3>, 2> quad_tris{{ {0,1,2}, {0,2,3} }};
        for (int oi = 0; oi < n_faces_; ++oi) {
            const int idx = order_[oi];
            const BspFace&  face = faces_[idx];
            const auto&     cv   = clip_verts_[idx];
            const Lightmap* lm   = &lightmaps_[idx];
            for (int t = 0; t < 2; ++t) {
                const ClipVert a = cv[quad_tris[t][0]];
                const ClipVert b = cv[quad_tris[t][1]];
                const ClipVert c = cv[quad_tris[t][2]];
                std::array<ClipVert, 6> clipped{};
                const int n_tris = clip_triangle_near(a, b, c, near_w_, clipped);
                for (int k = 0; k < n_tris; ++k) {
                    const ScreenVert s0 = to_screen(clipped[k*3 + 0], fb_->w, fb_->h);
                    const ScreenVert s1 = to_screen(clipped[k*3 + 1], fb_->w, fb_->h);
                    const ScreenVert s2 = to_screen(clipped[k*3 + 2], fb_->w, fb_->h);
                    rast_scan_textured(*fb_, w.sb, s0, s1, s2, face.tex, lm,
                                        w.y_start, w.y_end);
                }
            }
        }
    }

    int                       n_;
    std::barrier<>            start_;
    std::barrier<>            done_;
    std::vector<Worker>       workers_;
    std::vector<std::jthread> threads_;
    std::atomic<bool>         quit_{false};

    // Per-frame state set by render_frame:
    Framebuf*                                         fb_{nullptr};
    std::span<const std::array<ClipVert, 4>>          clip_verts_;
    std::span<const int>                              order_;
    std::span<const Lightmap>                         lightmaps_;
    std::span<const BspFace>                          faces_;
    int                                               n_faces_{0};
    f32                                               near_w_{0.1f};
};

}  // namespace

int main(int argc, char** argv) {
    int n_threads = (argc > 1) ? std::atoi(argv[1]) : 0;
    if (n_threads <= 0) {
        n_threads = static_cast<int>(std::thread::hardware_concurrency());
        n_threads = std::clamp(n_threads, 1, 8);
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        log_error("SDL_Init: {}", SDL_GetError());
        return 1;
    }

    SDL_Window* win = SDL_CreateWindow(
        "Sotark — sw_renderer C++26 (jthread + barrier)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT, 0);
    if (!win) { log_error("SDL_CreateWindow: {}", SDL_GetError()); SDL_Quit(); return 1; }

    SDL_Surface* back = SDL_CreateRGBSurfaceWithFormat(
        0, WIDTH, HEIGHT, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!back) {
        log_error("SDL_CreateRGBSurfaceWithFormat: {}", SDL_GetError());
        SDL_DestroyWindow(win); SDL_Quit(); return 1;
    }

    const int pitch_pixels = back->pitch / 4;
    std::vector<f32> depth_buf(static_cast<usize>(pitch_pixels) * HEIGHT);
    Framebuf fb{
        static_cast<u32*>(back->pixels), depth_buf.data(),
        WIDTH, HEIGHT, pitch_pixels,
    };

    g_scene.load_default();
    Vec3  light_pos{3.0f, 8.0f, 4.0f};
    f32   light_intensity = 35.0f;
    f32   ambient_level   = 0.12f;
    {
        const auto t0 = std::chrono::steady_clock::now();
        g_scene.bake_lightmaps(light_pos, light_intensity, ambient_level);
        const auto t1 = std::chrono::steady_clock::now();
        const f32 bake_ms = std::chrono::duration<f32, std::milli>(t1 - t0).count();
        log_info("lightmaps baked ({} faces) in {:.1f} ms", g_scene.n_faces(), bake_ms);
    }

    Pool pool(n_threads);
    log_info("pool: {} threads, ~{} scanlines/stripe", pool.n(), HEIGHT / pool.n());

    const Mat4 proj = Mat4::perspective(deg_to_rad(60.0f),
                                          static_cast<f32>(WIDTH) / static_cast<f32>(HEIGHT),
                                          0.05f, 100.0f);

    Vec3 cam_target{0.0f, 1.0f, 0.0f};
    f32  cam_yaw    = 0.3f;
    f32  cam_pitch  = 0.25f;
    f32  cam_radius = 8.0f;
    constexpr f32 ORBIT_SPEED = 0.008f;
    constexpr f32 PAN_SPEED   = 0.012f;
    constexpr f32 ZOOM_FACTOR = 1.12f;

    bool mouse_left_down  = false;
    bool mouse_right_down = false;
    int  last_mouse_x = 0, last_mouse_y = 0;
    int  mouse_press_x = 0, mouse_press_y = 0;
    bool mouse_left_dragged = false;
    bool prev_mouse_left_down = false;

    int selected_face = -1;
    constexpr int PANEL_X = WIDTH - 280;
    constexpr int PANEL_Y = 80;
    constexpr int PANEL_W = 270;
    constexpr int PANEL_H = 200;
    UiCtx ui{};

    u32  frames      = 0;
    u32  last_fps_ms = SDL_GetTicks();
    u64  sum_drawn = 0, sum_skipped = 0;

    log_info("Editor — LMB orbit / RMB pan / wheel zoom / F focus / 1 cube / 2 pillar / DEL remove / ESC quit");

    std::array<std::array<ClipVert, 4>, SCENE_MAX_FACES> clip_verts{};
    std::array<int, SCENE_MAX_FACES>                     order{};

    bool running = true;
    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) { running = false; }
            else if (ev.type == SDL_KEYDOWN) {
                const SDL_Keycode sym = ev.key.keysym.sym;
                if (sym == SDLK_ESCAPE) {
                    if (selected_face >= 0) selected_face = -1;
                    else running = false;
                } else if (sym == SDLK_f) {
                    if (selected_face >= 0) {
                        cam_target = face_center(g_scene.faces()[selected_face]);
                        cam_radius = 4.5f;
                    } else {
                        cam_target = {0.0f, 1.0f, 0.0f};
                        cam_yaw = 0.3f; cam_pitch = 0.25f; cam_radius = 8.0f;
                    }
                } else if (sym == SDLK_1) {
                    g_scene.add_cube(cam_target, 1.0f, tex_brick);
                    g_scene.bake_lightmaps(light_pos, light_intensity, ambient_level);
                    log_info("added cube (now {} faces)", g_scene.n_faces());
                } else if (sym == SDLK_2) {
                    g_scene.add_pillar({cam_target.x, 0.0f, cam_target.z}, 0.6f, 2.0f, tex_brick);
                    g_scene.bake_lightmaps(light_pos, light_intensity, ambient_level);
                    log_info("added pillar (now {} faces)", g_scene.n_faces());
                } else if (sym == SDLK_DELETE || sym == SDLK_BACKSPACE) {
                    if (selected_face >= 0) {
                        g_scene.remove_face(selected_face);
                        g_scene.bake_lightmaps(light_pos, light_intensity, ambient_level);
                        log_info("removed face (now {} faces)", g_scene.n_faces());
                        selected_face = -1;
                    }
                }
            }
            else if (ev.type == SDL_MOUSEBUTTONDOWN) {
                if (ev.button.button == SDL_BUTTON_LEFT) {
                    mouse_left_down    = true;
                    mouse_left_dragged = false;
                    mouse_press_x      = ev.button.x;
                    mouse_press_y      = ev.button.y;
                }
                if (ev.button.button == SDL_BUTTON_RIGHT) mouse_right_down = true;
                last_mouse_x = ev.button.x;
                last_mouse_y = ev.button.y;
            }
            else if (ev.type == SDL_MOUSEBUTTONUP) {
                if (ev.button.button == SDL_BUTTON_LEFT) {
                    mouse_left_down = false;
                    if (!mouse_left_dragged) {
                        const Vec3 eye_pick{
                            cam_target.x + cam_radius * std::sin(cam_yaw) * std::cos(cam_pitch),
                            cam_target.y + cam_radius * std::sin(cam_pitch),
                            cam_target.z + cam_radius * std::cos(cam_yaw) * std::cos(cam_pitch),
                        };
                        selected_face = pick_face(ev.button.x, ev.button.y,
                                                   eye_pick, cam_target, Vec3{0, 1, 0},
                                                   deg_to_rad(60.0f),
                                                   static_cast<f32>(WIDTH) / static_cast<f32>(HEIGHT));
                        log_info("picked face: {}", selected_face);
                    }
                }
                if (ev.button.button == SDL_BUTTON_RIGHT) mouse_right_down = false;
            }
            else if (ev.type == SDL_MOUSEMOTION) {
                const int dx = ev.motion.x - last_mouse_x;
                const int dy = ev.motion.y - last_mouse_y;
                last_mouse_x = ev.motion.x;
                last_mouse_y = ev.motion.y;
                const bool in_ui = (ev.motion.x >= PANEL_X &&
                                     ev.motion.x <  PANEL_X + PANEL_W &&
                                     ev.motion.y >= PANEL_Y &&
                                     ev.motion.y <  PANEL_Y + PANEL_H);
                if (in_ui) continue;
                if (mouse_left_down) {
                    const int total_dx = ev.motion.x - mouse_press_x;
                    const int total_dy = ev.motion.y - mouse_press_y;
                    if (std::abs(total_dx) + std::abs(total_dy) > 4) mouse_left_dragged = true;
                    cam_yaw   -= static_cast<f32>(dx) * ORBIT_SPEED;
                    cam_pitch -= static_cast<f32>(dy) * ORBIT_SPEED;
                    cam_pitch = std::clamp(cam_pitch, -1.5f, 1.5f);
                } else if (mouse_right_down) {
                    const Vec3 fwd{
                        std::sin(cam_yaw) * std::cos(cam_pitch),
                        std::sin(cam_pitch),
                        std::cos(cam_yaw) * std::cos(cam_pitch),
                    };
                    const Vec3 right = normalize(cross(fwd, Vec3{0, 1, 0}));
                    const Vec3 up    = normalize(cross(right, fwd));
                    const f32  scale = cam_radius * PAN_SPEED;
                    cam_target += right * (-static_cast<f32>(dx) * scale);
                    cam_target += up    * ( static_cast<f32>(dy) * scale);
                }
            }
            else if (ev.type == SDL_MOUSEWHEEL) {
                if (ev.wheel.y > 0)      cam_radius /= ZOOM_FACTOR;
                else if (ev.wheel.y < 0) cam_radius *= ZOOM_FACTOR;
                cam_radius = std::clamp(cam_radius, 0.5f, 40.0f);
            }
        }

        const Vec3 cam_eye{
            cam_target.x + cam_radius * std::sin(cam_yaw) * std::cos(cam_pitch),
            cam_target.y + cam_radius * std::sin(cam_pitch),
            cam_target.z + cam_radius * std::cos(cam_yaw) * std::cos(cam_pitch),
        };
        const Mat4 view = Mat4::look_at(cam_eye, cam_target, Vec3{0, 1, 0});
        const Mat4 mvp  = proj * view;

        // Sky gradient.
        for (int yy = 0; yy < HEIGHT; ++yy) {
            const f32 t = static_cast<f32>(yy) / static_cast<f32>(HEIGHT - 1);
            const u8 r = static_cast<u8>(50  * t + 110 * (1.0f - t));
            const u8 g = static_cast<u8>(80  * t + 140 * (1.0f - t));
            const u8 b = static_cast<u8>(140 * t + 180 * (1.0f - t));
            const u32 c = argb8888(r, g, b);
            std::fill_n(fb.color + static_cast<usize>(yy) * fb.pitch_pixels, WIDTH, c);
        }

        // Sort + project (single-threaded).
        const int N = g_scene.n_faces();
        const auto faces_v = g_scene.faces();
        std::array<f32, SCENE_MAX_FACES> dist_sq{};
        for (int i = 0; i < N; ++i) {
            order[i] = i;
            dist_sq[i] = length_sq(face_center(faces_v[i]) - cam_eye);
        }
        // Insertion sort (small N, stable, cheap)
        for (int i = 1; i < N; ++i) {
            const int key = order[i];
            const f32 kd  = dist_sq[key];
            int j = i - 1;
            while (j >= 0 && dist_sq[order[j]] > kd) { order[j + 1] = order[j]; --j; }
            order[j + 1] = key;
        }
        for (int i = 0; i < N; ++i) {
            const BspFace& face = faces_v[i];
            for (int v = 0; v < 4; ++v) {
                const Vec4 v4{face.verts[v].x, face.verts[v].y, face.verts[v].z, 1.0f};
                const Vec4 clip = mvp * v4;
                clip_verts[i][v] = ClipVert{
                    clip.x, clip.y, clip.z, clip.w,
                    face.uvs[v].x, face.uvs[v].y,
                };
            }
        }

        pool.render_frame(fb,
            std::span{clip_verts.data(), static_cast<usize>(N)},
            std::span{order.data(), static_cast<usize>(N)},
            g_scene.lightmaps(),
            faces_v, N, 0.1f);

        // HUD + UI panel (post-workers, single-threaded).
        const u64 hud_now = SDL_GetPerformanceCounter();
        static u64 last_hud_ts = 0;
        static u32 cached_fps = 0;
        static u32 hud_frame_count = 0;
        ++hud_frame_count;
        if (last_hud_ts == 0) last_hud_ts = hud_now;
        const f64 elapsed_s = static_cast<f64>(hud_now - last_hud_ts)
                            / static_cast<f64>(SDL_GetPerformanceFrequency());
        if (elapsed_s >= 0.25) {
            cached_fps = static_cast<u32>(static_cast<f64>(hud_frame_count) / elapsed_s);
            hud_frame_count = 0;
            last_hud_ts = hud_now;
        }

        const bool curr_mouse_down = mouse_left_down;
        const bool ui_mouse_pressed = curr_mouse_down && !prev_mouse_left_down;
        prev_mouse_left_down = curr_mouse_down;

        ui_begin(ui, &fb, last_mouse_x, last_mouse_y, curr_mouse_down, ui_mouse_pressed);
        ui_panel(ui, PANEL_X, PANEL_Y, PANEL_W, PANEL_H, "LIGHT");
        ui_label(ui, PANEL_X + 8, PANEL_Y + 30, "INTENSITY", 0xFFFFFFFFu);
        const f32 prev_intensity = light_intensity;
        ui_slider_f(ui, PANEL_X + 8, PANEL_Y + 46, PANEL_W - 16, 16,
                     light_intensity, 0.0f, 100.0f);
        ui_label(ui, PANEL_X + 8, PANEL_Y + 72, "AMBIENT", 0xFFFFFFFFu);
        const f32 prev_ambient = ambient_level;
        ui_slider_f(ui, PANEL_X + 8, PANEL_Y + 88, PANEL_W - 16, 16,
                     ambient_level, 0.0f, 0.5f);
        ui_label(ui, PANEL_X + 8, PANEL_Y + 114, "LIGHT HEIGHT", 0xFFFFFFFFu);
        const f32 prev_lh = light_pos.y;
        ui_slider_f(ui, PANEL_X + 8, PANEL_Y + 130, PANEL_W - 16, 16,
                     light_pos.y, 1.0f, 20.0f);

        if (light_intensity != prev_intensity || ambient_level != prev_ambient || light_pos.y != prev_lh) {
            g_scene.bake_lightmaps(light_pos, light_intensity, ambient_level);
        }
        ui_end(ui);

        if (selected_face >= 0) {
            const BspFace& sf = faces_v[selected_face];
            std::array<int, 4> sx{}, sy{};
            bool all_in_front = true;
            for (int i = 0; i < 4; ++i) {
                const Vec4 v4{sf.verts[i].x, sf.verts[i].y, sf.verts[i].z, 1.0f};
                const Vec4 clip = mvp * v4;
                if (clip.w <= 0.1f) { all_in_front = false; break; }
                const f32 invw = 1.0f / clip.w;
                sx[i] = static_cast<int>((clip.x * invw * 0.5f + 0.5f) * static_cast<f32>(WIDTH));
                sy[i] = static_cast<int>((1.0f - (clip.y * invw * 0.5f + 0.5f)) * static_cast<f32>(HEIGHT));
            }
            if (all_in_front) {
                const u32 outline = argb8888(255, 180, 0);
                for (int i = 0; i < 4; ++i) {
                    const int j = (i + 1) % 4;
                    draw_line(fb, sx[i], sy[i], sx[j], sy[j], outline);
                    draw_line(fb, sx[i] + 1, sy[i], sx[j] + 1, sy[j], outline);
                }
            }
        }

        // HUD text.
        const u32 hud_color = argb8888(255, 255, 255);
        draw_rect(fb, 8, 8, 360, 64, argb8888(0, 0, 0));
        draw_text_shadowed(fb, 14, 14, std::format("FPS {}  THREADS {}", cached_fps, pool.n()), hud_color);
        draw_text_shadowed(fb, 14, 28,
            selected_face >= 0
                ? std::format("SELECTED FACE {}", selected_face)
                : "SELECTED NONE  CLICK TO PICK",
            hud_color);
        draw_text_shadowed(fb, 14, 42,
            std::format("TARGET {:.1f} {:.1f} {:.1f}  R {:.1f}",
                         cam_target.x, cam_target.y, cam_target.z, cam_radius),
            hud_color);
        draw_text_shadowed(fb, 14, 56,
            std::format("FACES {} / {}", g_scene.n_faces(), SCENE_MAX_FACES),
            argb8888(180, 180, 180));
        draw_rect(fb, 8, HEIGHT - 36, 480, 28, argb8888(0, 0, 0));
        draw_text_shadowed(fb, 14, HEIGHT - 30,
            "LMB ORBIT  RMB PAN  WHEEL ZOOM  CLICK PICK  F FOCUS",
            argb8888(200, 200, 200));
        draw_text_shadowed(fb, 14, HEIGHT - 18,
            "1 ADD CUBE  2 ADD PILLAR  DEL REMOVE SEL  ESC DESELECT/QUIT",
            argb8888(200, 200, 200));

        SDL_Surface* win_surface = SDL_GetWindowSurface(win);
        if (win_surface) {
            SDL_BlitSurface(back, nullptr, win_surface, nullptr);
            SDL_UpdateWindowSurface(win);
        }

        ++frames;
        for (const Worker& w : pool.workers()) {
            sum_drawn   += static_cast<u64>(w.sb.pixels_drawn());
            sum_skipped += static_cast<u64>(w.sb.pixels_skipped());
        }

        const u32 now_ms = SDL_GetTicks();
        if (now_ms - last_fps_ms >= 1000) {
            const u64 total = sum_drawn + sum_skipped;
            const f32 fill  = static_cast<f32>(sum_drawn)
                            / static_cast<f32>(WIDTH * HEIGHT * static_cast<int>(frames));
            const f32 effic = total ? static_cast<f32>(sum_drawn) / static_cast<f32>(total) : 0.0f;
            log_info("[{} threads] fps={}  fill={:.3f}x  effic={:.0f}%  target=({:.1f}, {:.1f}, {:.1f}) r={:.1f}",
                      pool.n(), frames, fill, effic * 100.0f,
                      cam_target.x, cam_target.y, cam_target.z, cam_radius);
            frames = 0; sum_drawn = sum_skipped = 0;
            last_fps_ms = now_ms;
        }
    }

    SDL_FreeSurface(back);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
