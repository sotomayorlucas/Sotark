#include "ui.h"
#include "text.h"

static u32 argb(u8 r, u8 g, u8 b) {
    return (0xFFu << 24) | ((u32)r << 16) | ((u32)g << 8) | (u32)b;
}

static bool in_rect(int px, int py, int x, int y, int w, int h) {
    return (px >= x) && (px < x + w) && (py >= y) && (py < y + h);
}

void ui_begin(ui_ctx_t *ui, framebuf_t *fb, int mx, int my,
              bool md, bool mp) {
    ui->mouse_x       = mx;
    ui->mouse_y       = my;
    ui->mouse_down    = md;
    ui->mouse_pressed = mp;
    ui->hot_id        = 0;
    ui->fb            = fb;
}

void ui_end(ui_ctx_t *ui) {
    /* Liberar active si mouse soltado afuera de cualquier widget. */
    if (!ui->mouse_down) ui->active_id = 0;
}

bool ui_mouse_in_rect(const ui_ctx_t *ui, int x, int y, int w, int h) {
    return in_rect(ui->mouse_x, ui->mouse_y, x, y, w, h);
}

void ui_panel(ui_ctx_t *ui, int x, int y, int w, int h, const char *title) {
    draw_rect(ui->fb, x, y, w, h, argb(28, 32, 44));
    /* Title bar. */
    draw_rect(ui->fb, x, y, w, 20, argb(50, 60, 86));
    if (title) draw_text_shadowed(ui->fb, x + 6, y + 6, title, argb(230, 230, 230));
    /* 1px border. */
    draw_rect(ui->fb, x, y, w, 1,     argb(80, 90, 120));
    draw_rect(ui->fb, x, y + h - 1, w, 1, argb(15, 18, 25));
}

void ui_label(ui_ctx_t *ui, int x, int y, const char *text, u32 color) {
    draw_text_shadowed(ui->fb, x, y, text, color);
}

bool ui_button(ui_ctx_t *ui, int x, int y, int w, int h, const char *label) {
    const int id = x * 4096 + y;
    const bool over = in_rect(ui->mouse_x, ui->mouse_y, x, y, w, h);
    if (over) ui->hot_id = id;

    bool clicked = false;
    if (ui->hot_id == id && ui->mouse_pressed) ui->active_id = id;
    if (ui->active_id == id && !ui->mouse_down) {
        if (over) clicked = true;
        ui->active_id = 0;
    }

    const u32 bg = (ui->active_id == id) ? argb(45, 65, 110)
                  : (ui->hot_id == id)    ? argb(70, 90, 140)
                                            : argb(50, 58, 80);
    draw_rect(ui->fb, x, y, w, h, bg);
    /* 1px highlight + shadow. */
    draw_rect(ui->fb, x, y, w, 1, argb(120, 140, 180));
    draw_rect(ui->fb, x, y + h - 1, w, 1, argb(20, 25, 35));
    if (label) draw_text_shadowed(ui->fb, x + 6, y + (h - 8) / 2, label, 0xFFFFFFFFu);
    return clicked;
}

bool ui_slider_f(ui_ctx_t *ui, int x, int y, int w, int h,
                 f32 *value, f32 minv, f32 maxv) {
    const int id = x * 4096 + y;
    const bool over = in_rect(ui->mouse_x, ui->mouse_y, x, y, w, h);
    if (over) ui->hot_id = id;
    if (ui->hot_id == id && ui->mouse_pressed) ui->active_id = id;

    bool changed = false;
    if (ui->active_id == id) {
        const f32 t = (f32)(ui->mouse_x - x) / (f32)(w - 1);
        f32 tc = t;
        if (tc < 0.0f) tc = 0.0f;
        if (tc > 1.0f) tc = 1.0f;
        const f32 new_value = minv + tc * (maxv - minv);
        if (new_value != *value) {
            *value = new_value;
            changed = true;
        }
    }

    /* Track. */
    draw_rect(ui->fb, x, y, w, h, argb(20, 25, 35));
    /* Filled part. */
    f32 t = (*value - minv) / (maxv - minv);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    const int fw = (int)(t * (f32)w);
    const u32 fg = (ui->active_id == id) ? argb(110, 160, 230)
                  : (ui->hot_id == id)    ? argb(90, 140, 210)
                                            : argb(75, 120, 190);
    if (fw > 0) draw_rect(ui->fb, x, y, fw, h, fg);
    /* Handle pip. */
    const int handle_x = x + fw - 2;
    if (handle_x >= x && handle_x + 4 <= x + w) {
        draw_rect(ui->fb, handle_x, y - 1, 4, h + 2, argb(230, 240, 255));
    }
    return changed;
}
