#ifndef SOTARK_SW_UI_H
#define SOTARK_SW_UI_H

#include "framebuffer.h"

#include <stdbool.h>

/*
 * Immediate-mode UI library minimal (estilo dear-imgui / nuklear).
 *
 * Pattern por frame:
 *   ui_begin(&ui, &fb, mouse_x, mouse_y, mb_down, mb_pressed);
 *   ui_panel(&ui, x, y, w, h, "Material");
 *   ui_label(&ui, x+8, y+24, "ALBEDO R");
 *   if (ui_slider_f(&ui, x+8, y+38, 200, 16, &r, 0, 1)) on_change();
 *   ui_end(&ui);
 *
 * IDs: position-based (x*1000+y). Único mientras no overlapean rectángulos.
 * Suficiente para nuestros paneles de pocos widgets.
 */
typedef struct {
    int  mouse_x, mouse_y;
    bool mouse_down;        /* held this frame */
    bool mouse_pressed;     /* edge: pressed this frame (not held last) */
    int  hot_id;            /* widget under mouse */
    int  active_id;         /* widget being interacted with (drag) */
    framebuf_t *fb;
} ui_ctx_t;

void ui_begin(ui_ctx_t *ui, framebuf_t *fb, int mx, int my,
              bool mouse_down, bool mouse_pressed);
void ui_end  (ui_ctx_t *ui);

/* Filled rect con borde para visual de panel. */
void ui_panel(ui_ctx_t *ui, int x, int y, int w, int h, const char *title);
void ui_label(ui_ctx_t *ui, int x, int y, const char *text, u32 color);

bool ui_button   (ui_ctx_t *ui, int x, int y, int w, int h, const char *label);
bool ui_slider_f (ui_ctx_t *ui, int x, int y, int w, int h,
                  f32 *value, f32 min, f32 max);

bool ui_mouse_in_rect(const ui_ctx_t *ui, int x, int y, int w, int h);

#endif
