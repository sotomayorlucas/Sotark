module;

#include <string_view>
#include <algorithm>

export module sotark.sw:ui;

import sotark.common;
import :framebuffer;
import :text;

export namespace sotark::sw {

constexpr u32 argb(u8 r, u8 g, u8 b) noexcept {
    return (0xFFu << 24) | (static_cast<u32>(r) << 16) | (static_cast<u32>(g) << 8) | static_cast<u32>(b);
}

// Immediate-mode UI ctx. Position-based IDs (x*4096 + y).
struct UiCtx {
    int  mouse_x{0}, mouse_y{0};
    bool mouse_down{false};
    bool mouse_pressed{false};
    int  hot_id{0};
    int  active_id{0};
    Framebuf* fb{nullptr};

    constexpr bool in_rect(int x, int y, int w, int h) const noexcept {
        return (mouse_x >= x) && (mouse_x < x + w) && (mouse_y >= y) && (mouse_y < y + h);
    }
};

inline void ui_begin(UiCtx& ui, Framebuf* fb, int mx, int my,
                     bool md, bool mp) noexcept {
    ui.mouse_x = mx; ui.mouse_y = my;
    ui.mouse_down = md; ui.mouse_pressed = mp;
    ui.hot_id = 0; ui.fb = fb;
}

inline void ui_end(UiCtx& ui) noexcept {
    if (!ui.mouse_down) ui.active_id = 0;
}

inline void ui_panel(UiCtx& ui, int x, int y, int w, int h, std::string_view title) {
    draw_rect(*ui.fb, x, y, w, h, argb(28, 32, 44));
    draw_rect(*ui.fb, x, y, w, 20, argb(50, 60, 86));
    if (!title.empty()) draw_text_shadowed(*ui.fb, x + 6, y + 6, title, argb(230, 230, 230));
    draw_rect(*ui.fb, x, y, w, 1, argb(80, 90, 120));
    draw_rect(*ui.fb, x, y + h - 1, w, 1, argb(15, 18, 25));
}

inline void ui_label(UiCtx& ui, int x, int y, std::string_view text, u32 color) {
    draw_text_shadowed(*ui.fb, x, y, text, color);
}

inline bool ui_button(UiCtx& ui, int x, int y, int w, int h, std::string_view label) {
    const int id = x * 4096 + y;
    const bool over = ui.in_rect(x, y, w, h);
    if (over) ui.hot_id = id;

    bool clicked = false;
    if (ui.hot_id == id && ui.mouse_pressed) ui.active_id = id;
    if (ui.active_id == id && !ui.mouse_down) {
        if (over) clicked = true;
        ui.active_id = 0;
    }

    const u32 bg = (ui.active_id == id) ? argb(45, 65, 110)
                  : (ui.hot_id == id)    ? argb(70, 90, 140)
                                            : argb(50, 58, 80);
    draw_rect(*ui.fb, x, y, w, h, bg);
    draw_rect(*ui.fb, x, y, w, 1, argb(120, 140, 180));
    draw_rect(*ui.fb, x, y + h - 1, w, 1, argb(20, 25, 35));
    if (!label.empty()) draw_text_shadowed(*ui.fb, x + 6, y + (h - 8) / 2, label, 0xFFFFFFFFu);
    return clicked;
}

inline bool ui_slider_f(UiCtx& ui, int x, int y, int w, int h,
                        f32& value, f32 minv, f32 maxv) {
    const int id = x * 4096 + y;
    const bool over = ui.in_rect(x, y, w, h);
    if (over) ui.hot_id = id;
    if (ui.hot_id == id && ui.mouse_pressed) ui.active_id = id;

    bool changed = false;
    if (ui.active_id == id) {
        const f32 t  = static_cast<f32>(ui.mouse_x - x) / static_cast<f32>(w - 1);
        const f32 tc = std::clamp(t, 0.0f, 1.0f);
        const f32 new_value = minv + tc * (maxv - minv);
        if (new_value != value) {
            value   = new_value;
            changed = true;
        }
    }

    draw_rect(*ui.fb, x, y, w, h, argb(20, 25, 35));
    const f32 t = std::clamp((value - minv) / (maxv - minv), 0.0f, 1.0f);
    const int fw = static_cast<int>(t * static_cast<f32>(w));
    const u32 fg = (ui.active_id == id) ? argb(110, 160, 230)
                  : (ui.hot_id == id)    ? argb(90, 140, 210)
                                            : argb(75, 120, 190);
    if (fw > 0) draw_rect(*ui.fb, x, y, fw, h, fg);
    const int handle_x = x + fw - 2;
    if (handle_x >= x && handle_x + 4 <= x + w) {
        draw_rect(*ui.fb, handle_x, y - 1, 4, h + 2, argb(230, 240, 255));
    }
    return changed;
}

}  // namespace sotark::sw
