#ifndef SOTARK_SW_SPAN_BUFFER_H
#define SOTARK_SW_SPAN_BUFFER_H

#include "common/types.h"

#include <stdbool.h>

/* Callback que dibuja un run de pixels NO cubiertos. */
typedef void (*span_draw_fn)(int y, int x0, int x1, void *userdata);

/*
 * Span buffer "didáctico" basado en bitmap per-pixel (1 byte por pixel).
 *
 * Para cada span emitido en una scanline, encontramos los runs de pixels
 * todavía NO cubiertos, llamamos a `draw_fn` solo para esos, y los marcamos
 * como cubiertos. Garantiza que cada pixel se texturiza exactamente UNA
 * vez por frame — zero overdraw, siempre que los polígonos se procesen
 * front-to-back.
 *
 * El span buffer "real" de Quake usa una lista ordenada de intervalos por
 * scanline: conceptualmente equivalente, pero más friendly al cache y a
 * SIMD. Lo dejamos para M9.
 */
typedef struct {
    u8 *covered;          /* w*h bytes; 1 = ya texturizado este frame */
    int w, h;
    int pixels_drawn;     /* total pixels texturizados (≤ w*h, garantizado) */
    int pixels_skipped;   /* tests que ya estaban marcados (overdraw evitado) */
} span_buffer_t;

bool span_buffer_init(span_buffer_t *sb, int w, int h);
void span_buffer_free(span_buffer_t *sb);
void span_buffer_clear(span_buffer_t *sb);

/* Emit a span [x0, x1) en la row y. Llama fn por cada subrun de pixels
 * no cubiertos. Después marca toda la región [x0, x1) como cubierta. */
void span_buffer_emit(span_buffer_t *sb, int y, int x0, int x1,
                      span_draw_fn fn, void *userdata);

#endif
