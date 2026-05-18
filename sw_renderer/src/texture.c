#include "texture.h"

#include <math.h>

u32 tex_checker(f32 u, f32 v) {
    /* floorf maneja correctamente valores negativos: floor(-1.5) = -2.
     * El XOR de las dos paridades da el patrón alternado clásico. */
    int cu = (int)floorf(u);
    int cv = (int)floorf(v);
    return ((cu ^ cv) & 1) ? 0xFFE0E0E0u : 0xFF303030u;
}

u32 tex_brick(f32 u, f32 v) {
    /* Ladrillos 2x1 con offset cada otra fila. */
    int row = (int)floorf(v);
    f32 u_off = (row & 1) ? 1.0f : 0.0f;
    f32 uu = u + u_off;
    f32 fu = uu - floorf(uu * 0.5f) * 2.0f;   /* u dentro de [0, 2) */
    f32 fv = v - floorf(v);                    /* v dentro de [0, 1) */
    /* Mortar (líneas grises) en los bordes. */
    if (fu < 0.08f || fu > 1.92f || fv < 0.08f || fv > 0.92f) {
        return 0xFF555555u;
    }
    /* Pequeño noise determinístico por ladrillo. */
    int col = (int)floorf(uu * 0.5f);
    u32 n = (u32)((col * 73856093) ^ (row * 19349663));
    u8 r = 140u + (u8)((n >> 16) & 31);
    u8 g =  70u + (u8)((n >>  8) & 15);
    u8 b =  50u + (u8)( n        & 15);
    return 0xFF000000u | ((u32)r << 16) | ((u32)g << 8) | b;
}

u32 tex_floor(f32 u, f32 v) {
    /* Baldosas 0.5x0.5 con bordes oscuros. */
    f32 fu = u * 2.0f;
    f32 fv = v * 2.0f;
    int cu = (int)floorf(fu);
    int cv = (int)floorf(fv);
    f32 lu = fu - floorf(fu);
    f32 lv = fv - floorf(fv);
    /* Grout fino. */
    if (lu < 0.04f || lu > 0.96f || lv < 0.04f || lv > 0.96f) {
        return 0xFF202020u;
    }
    return ((cu ^ cv) & 1) ? 0xFFB0B0B0u : 0xFF909090u;
}

u32 tex_ceiling(f32 u, f32 v) {
    /* Light beige con micro-variación. */
    u32 n = (u32)(((int)floorf(u * 4.0f)) * 73856093) ^ (u32)(((int)floorf(v * 4.0f)) * 19349663);
    u8 r = 200u + (u8)((n >> 16) & 15);
    u8 g = 195u + (u8)((n >>  8) & 15);
    u8 b = 180u + (u8)( n        & 15);
    return 0xFF000000u | ((u32)r << 16) | ((u32)g << 8) | b;
}
