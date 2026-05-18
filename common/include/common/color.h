#ifndef SOTARK_COMMON_COLOR_H
#define SOTARK_COMMON_COLOR_H

#include "common/types.h"
#include "common/vec.h"

typedef vec3_t rgb_t;

/* Linear-space rgb (0..1) -> 3 bytes RGB, no gamma. Used in M0 to match Shirley §2. */
static inline void rgb_to_bytes(rgb_t c, u8 out[3]) {
    f32 r = c.x < 0.0f ? 0.0f : (c.x > 1.0f ? 1.0f : c.x);
    f32 g = c.y < 0.0f ? 0.0f : (c.y > 1.0f ? 1.0f : c.y);
    f32 b = c.z < 0.0f ? 0.0f : (c.z > 1.0f ? 1.0f : c.z);
    out[0] = (u8)(255.999f * r);
    out[1] = (u8)(255.999f * g);
    out[2] = (u8)(255.999f * b);
}

/* sqrt gamma 2.0, for M2+ once antialiasing kicks in. */
static inline void rgb_to_bytes_gamma2(rgb_t c, u8 out[3]) {
    f32 r = sqrtf(c.x < 0.0f ? 0.0f : (c.x > 1.0f ? 1.0f : c.x));
    f32 g = sqrtf(c.y < 0.0f ? 0.0f : (c.y > 1.0f ? 1.0f : c.y));
    f32 b = sqrtf(c.z < 0.0f ? 0.0f : (c.z > 1.0f ? 1.0f : c.z));
    out[0] = (u8)(255.999f * r);
    out[1] = (u8)(255.999f * g);
    out[2] = (u8)(255.999f * b);
}

#endif
