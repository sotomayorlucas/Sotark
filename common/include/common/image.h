#ifndef SOTARK_COMMON_IMAGE_H
#define SOTARK_COMMON_IMAGE_H

#include "common/types.h"
#include "common/color.h"

/* Row-major image buffer, top-left origin (row 0 is the topmost in the PPM). */
typedef struct {
    int    w, h;
    rgb_t *pixels;
} image_t;

int  image_init(image_t *img, int w, int h);
void image_free(image_t *img);

/* Writes binary PPM (P6). Returns 0 on success, -1 on failure. */
int  image_write_ppm(const image_t *img, const char *path);

/* Lee PPM (P6, 8-bit RGB). Asume gamma 2.0 — convierte a linear al leer
 * (matchea lo que image_write_ppm hace al escribir). Returns 0 on success. */
int  image_load_ppm(image_t *img, const char *path);

/* Sample bilineal en (u, v) ∈ [0, 1]. Wraps fuera de rango (modulo). */
rgb_t image_sample_bilinear(const image_t *img, f32 u, f32 v);

#endif
