#ifndef SOTARK_COMMON_RNG_H
#define SOTARK_COMMON_RNG_H

#include "common/types.h"

/*
 * PCG32 (Permuted Congruential Generator, 32-bit output).
 * Reference: https://www.pcg-random.org/
 *
 * Properties relevant to the raytracer:
 *   - Deterministic given (init_state, init_seq) — reproducible images.
 *   - Each pcg32_t is independent — give one per thread/tile in M8.
 *   - 64-bit state, very long period (~2^64), excellent statistical quality.
 */
typedef struct {
    u64 state;
    u64 inc;
} pcg32_t;

void pcg32_seed(pcg32_t *r, u64 init_state, u64 init_seq);
u32  pcg32_next(pcg32_t *r);

/* Uniform float in [0, 1). 24-bit precision (float mantissa). */
f32  pcg32_float01(pcg32_t *r);

#endif
