#include "common/rng.h"

u32 pcg32_next(pcg32_t *r) {
    u64 old = r->state;
    r->state = old * 6364136223846793005ULL + r->inc;
    u32 xorshifted = (u32)(((old >> 18u) ^ old) >> 27u);
    u32 rot        = (u32)(old >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

void pcg32_seed(pcg32_t *r, u64 init_state, u64 init_seq) {
    r->state = 0u;
    r->inc   = (init_seq << 1u) | 1u;       /* inc must be odd */
    (void)pcg32_next(r);
    r->state += init_state;
    (void)pcg32_next(r);
}

f32 pcg32_float01(pcg32_t *r) {
    /* Top 24 bits as a float in [0, 1). */
    return (f32)(pcg32_next(r) >> 8) * (1.0f / 16777216.0f);
}
