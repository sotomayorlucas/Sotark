#ifndef SOTARK_COMMON_TYPES_H
#define SOTARK_COMMON_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;
typedef float    f32;
typedef double   f64;

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

/* Fixed-point 16.16 — used by the edge sorter in sw_renderer from M7. */
typedef i32 fixed16_16;
#define FIXED_FROM_INT(x)   ((fixed16_16)((x) << 16))
#define FIXED_FROM_FLOAT(x) ((fixed16_16)((x) * 65536.0f))
#define FIXED_TO_INT(x)     ((i32)((x) >> 16))
#define FIXED_TO_FLOAT(x)   ((f32)(x) / 65536.0f)

#endif
