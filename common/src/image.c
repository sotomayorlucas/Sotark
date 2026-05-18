#include "common/image.h"
#include "common/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

int image_init(image_t *img, int w, int h) {
    img->w = w;
    img->h = h;
    img->pixels = (rgb_t *)calloc((size_t)w * (size_t)h, sizeof(rgb_t));
    return img->pixels ? 0 : -1;
}

void image_free(image_t *img) {
    free(img->pixels);
    img->pixels = NULL;
    img->w = img->h = 0;
}

int image_load_ppm(image_t *img, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        LOG_ERROR("could not open %s for reading", path);
        return -1;
    }
    char magic[3] = { 0 };
    int  w = 0, h = 0, maxval = 0;
    if (fscanf(f, "%2s %d %d %d", magic, &w, &h, &maxval) != 4 ||
        strcmp(magic, "P6") != 0 || maxval != 255 || w <= 0 || h <= 0) {
        LOG_ERROR("bad PPM header in %s", path);
        fclose(f);
        return -1;
    }
    /* Skip exactly ONE whitespace after the maxval (PPM convention). */
    fgetc(f);

    if (image_init(img, w, h) != 0) {
        fclose(f);
        return -1;
    }
    const int n = w * h;
    for (int i = 0; i < n; ++i) {
        u8 bytes[3];
        if (fread(bytes, 1, 3, f) != 3) {
            LOG_ERROR("PPM %s: truncated pixel data at %d/%d", path, i, n);
            fclose(f);
            image_free(img);
            return -1;
        }
        /* Reverse gamma 2.0: linear = (byte/255)². Matches image_write_ppm. */
        const f32 r = (f32)bytes[0] / 255.0f;
        const f32 g = (f32)bytes[1] / 255.0f;
        const f32 b = (f32)bytes[2] / 255.0f;
        img->pixels[i] = (rgb_t){ r * r, g * g, b * b };
    }
    fclose(f);
    return 0;
}

rgb_t image_sample_bilinear(const image_t *img, f32 u, f32 v) {
    /* WRAP en u (para no tener seam en el meridiano de la esfera);
     * CLAMP en v (para que el polo no mezcle texels del polo opuesto). */
    u = u - floorf(u);
    if (u < 0.0f) u += 1.0f;
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;

    const int w = img->w, h = img->h;

    /* Centros de los texels están en (i+0.5)/w en u-space. Para muestrear
     * en (u, v), encontramos los dos texels más cercanos a ambos lados. */
    const f32 fu = u * (f32)w - 0.5f;
    const f32 fv = v * (f32)h - 0.5f;
    int iu0 = (int)floorf(fu);
    int iv0 = (int)floorf(fv);
    const f32 fx = fu - (f32)iu0;
    const f32 fy = fv - (f32)iv0;

    int iu1 = iu0 + 1;
    int iv1 = iv0 + 1;

    /* u: módulo positivo (handle negative indices). Esto elimina el seam. */
    iu0 = ((iu0 % w) + w) % w;
    iu1 = ((iu1 % w) + w) % w;
    /* v: clamp. */
    if (iv0 < 0)      iv0 = 0;
    if (iv0 > h - 1)  iv0 = h - 1;
    if (iv1 < 0)      iv1 = 0;
    if (iv1 > h - 1)  iv1 = h - 1;

    const rgb_t c00 = img->pixels[iv0 * w + iu0];
    const rgb_t c10 = img->pixels[iv0 * w + iu1];
    const rgb_t c01 = img->pixels[iv1 * w + iu0];
    const rgb_t c11 = img->pixels[iv1 * w + iu1];

    const f32 w00 = (1.0f - fx) * (1.0f - fy);
    const f32 w10 = fx          * (1.0f - fy);
    const f32 w01 = (1.0f - fx) * fy;
    const f32 w11 = fx          * fy;

    return (rgb_t){
        c00.x * w00 + c10.x * w10 + c01.x * w01 + c11.x * w11,
        c00.y * w00 + c10.y * w10 + c01.y * w01 + c11.y * w11,
        c00.z * w00 + c10.z * w10 + c01.z * w01 + c11.z * w11,
    };
}

int image_write_ppm(const image_t *img, const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        LOG_ERROR("could not open %s for writing", path);
        return -1;
    }
    fprintf(f, "P6\n%d %d\n255\n", img->w, img->h);
    const int n = img->w * img->h;
    /* Gamma 2.0 (sqrt). Path tracing accumulates linear-space color; viewers
     * assume sRGB-ish gamma, so without this the image looks muddy/dark. */
    for (int i = 0; i < n; ++i) {
        u8 bytes[3];
        rgb_to_bytes_gamma2(img->pixels[i], bytes);
        fwrite(bytes, 1, 3, f);
    }
    fclose(f);
    return 0;
}
