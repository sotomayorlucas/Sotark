#ifndef SOTARK_COMMON_UTIL_H
#define SOTARK_COMMON_UTIL_H

#include "common/types.h"
#include <math.h>

#define MIN(a,b)        ((a) < (b) ? (a) : (b))
#define MAX(a,b)        ((a) > (b) ? (a) : (b))
#define CLAMP(x,lo,hi)  ((x) < (lo) ? (lo) : ((x) > (hi) ? (hi) : (x)))

static inline f32 lerp_f(f32 a, f32 b, f32 t) { return a + (b - a) * t; }
static inline f32 deg2rad(f32 d) { return d * 0.017453292519943295f; }
static inline f32 rad2deg(f32 r) { return r * 57.29577951308232f; }

#endif
