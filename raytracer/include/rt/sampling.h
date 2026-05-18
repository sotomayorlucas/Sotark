#ifndef SOTARK_RT_SAMPLING_H
#define SOTARK_RT_SAMPLING_H

#include "common/types.h"
#include "common/vec.h"
#include "common/rng.h"

/*
 * Rejection sampling: tira puntos en el cubo [-1,1]^3 hasta encontrar uno
 * dentro de la esfera unidad. Cuesta ~2 intentos en promedio (volumen
 * 4/3*pi vs 8 del cubo). Más simple y suele ser más rápido que el método
 * trigonométrico con sqrt+sin+cos.
 */
static inline vec3_t rng_in_unit_sphere(pcg32_t *r) {
    for (;;) {
        vec3_t p = {
            pcg32_float01(r) * 2.0f - 1.0f,
            pcg32_float01(r) * 2.0f - 1.0f,
            pcg32_float01(r) * 2.0f - 1.0f,
        };
        if (v3_length_sq(p) < 1.0f) return p;
    }
}

/* Vector unitario uniformemente distribuido sobre la esfera. */
static inline vec3_t rng_unit_vec3(pcg32_t *r) {
    return v3_normalize(rng_in_unit_sphere(r));
}

/* Vector unitario en el hemisferio orientado por `normal`. Sirve para el
 * scatter alternativo (uniform hemisphere) que aparece en Shirley §9 antes
 * del refinamiento Lambertian. No lo usamos en M3 pero queda disponible. */
static inline vec3_t rng_hemisphere(pcg32_t *r, vec3_t normal) {
    vec3_t v = rng_unit_vec3(r);
    return v3_dot(v, normal) > 0.0f ? v : v3_neg(v);
}

/* Punto random dentro del disco unidad en el plano XY (z=0). Usado por la
 * cámara con defocus blur para muestrear el lens. */
static inline vec3_t rng_in_unit_disk(pcg32_t *r) {
    for (;;) {
        f32 x = pcg32_float01(r) * 2.0f - 1.0f;
        f32 y = pcg32_float01(r) * 2.0f - 1.0f;
        if (x * x + y * y < 1.0f) return (vec3_t){ x, y, 0.0f };
    }
}

#endif
