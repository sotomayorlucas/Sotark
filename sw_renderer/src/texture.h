#ifndef SOTARK_SW_TEXTURE_H
#define SOTARK_SW_TEXTURE_H

#include "common/types.h"

/*
 * M3-era texture sampling: una función pura (u, v) -> ARGB. Suficiente para
 * el demo del swimming afín. M4 va a agregar mipmaps; M6 carga TGA real.
 */
typedef u32 (*tex_sample_fn)(f32 u, f32 v);

/* Checker de 1 unidad: claro/oscuro en cells de tamaño 1. */
u32 tex_checker(f32 u, f32 v);

/* Ladrillos: 2 unidades de ancho × 1 de alto, ofset alterno por fila. */
u32 tex_brick(f32 u, f32 v);

/* Suelo tipo baldosa: cells de 0.5, con bordes sutiles. */
u32 tex_floor(f32 u, f32 v);

/* Cielo raso: claro uniforme con leves variaciones procedurales. */
u32 tex_ceiling(f32 u, f32 v);

#endif
