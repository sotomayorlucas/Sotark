#ifndef SOTARK_SW_TEXT_H
#define SOTARK_SW_TEXT_H

#include "framebuffer.h"

/*
 * Bitmap font 8x8 minimal (CC0-style hand-baked: digits + uppercase + few
 * punctuation). Suficiente para HUDs editor (FPS, selected face, coords).
 *
 * draw_text dibuja directo al framebuffer.color via framebuf_plot
 * (bounds-checked) — seguro y single-threaded (llamar después de workers).
 */
void draw_text(framebuf_t *fb, int x, int y, const char *s, u32 color);

/* Como draw_text pero pinta un "shadow" 1px de offset en color oscuro
 * primero — mejor legibilidad sobre cualquier background. */
void draw_text_shadowed(framebuf_t *fb, int x, int y, const char *s, u32 fg);

/* Filled rect (para fondos de paneles, barras). Bounds-clipped. */
void draw_rect(framebuf_t *fb, int x, int y, int w, int h, u32 color);

#endif
