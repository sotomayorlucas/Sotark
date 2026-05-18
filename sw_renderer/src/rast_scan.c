#include "rast_scan.h"

#include <math.h>

static clip_vert_t lerp_clip(clip_vert_t a, clip_vert_t b, f32 t) {
    return (clip_vert_t){
        .x = a.x + (b.x - a.x) * t,
        .y = a.y + (b.y - a.y) * t,
        .z = a.z + (b.z - a.z) * t,
        .w = a.w + (b.w - a.w) * t,
        .u = a.u + (b.u - a.u) * t,
        .v = a.v + (b.v - a.v) * t,
    };
}

int clip_triangle_near(clip_vert_t a, clip_vert_t b, clip_vert_t c,
                       f32 near_w, clip_vert_t out[6]) {
    const clip_vert_t verts[3] = { a, b, c };
    const int ins[3] = {
        a.w >= near_w ? 1 : 0,
        b.w >= near_w ? 1 : 0,
        c.w >= near_w ? 1 : 0,
    };
    const int n_in = ins[0] + ins[1] + ins[2];
    if (n_in == 0) return 0;
    if (n_in == 3) {
        out[0] = a; out[1] = b; out[2] = c;
        return 1;
    }

    /* Walk los 3 edges (i → i+1). Output el vert si está dentro; si el
     * edge cruza el plano, output el intersection. Construye polígono. */
    clip_vert_t poly[4];
    int n = 0;
    for (int i = 0; i < 3; ++i) {
        const int j = (i + 1) % 3;
        if (ins[i]) poly[n++] = verts[i];
        if (ins[i] != ins[j]) {
            const f32 t = (near_w - verts[i].w) / (verts[j].w - verts[i].w);
            poly[n++] = lerp_clip(verts[i], verts[j], t);
            poly[n - 1].w = near_w;     /* exactamente en el plano */
        }
    }

    /* Fan triangulation: (poly[0], poly[1], poly[2]) y opcionalmente
     * (poly[0], poly[2], poly[3]). */
    int n_tris = 0;
    for (int i = 1; i < n - 1; ++i) {
        out[n_tris * 3 + 0] = poly[0];
        out[n_tris * 3 + 1] = poly[i];
        out[n_tris * 3 + 2] = poly[i + 1];
        n_tris++;
    }
    return n_tris;
}

typedef struct {
    f32 inv_w_dx, inv_w_dy, inv_w_origin;
    f32 u_w_dx,   u_w_dy,   u_w_origin;
    f32 v_w_dx,   v_w_dy,   v_w_origin;
    tex_sample_fn     tex;
    const lightmap_t *lm;       /* NULL = sin modulador (full bright) */
    framebuf_t       *fb;
} grad_ctx_t;

static inline u32 modulate_argb(u32 base, u32 mod) {
    /* Multiplica canal-a-canal: out = (base * mod) / 255. */
    const u32 br = (base >> 16) & 0xFFu, bg = (base >> 8) & 0xFFu, bb = base & 0xFFu;
    const u32 mr = (mod  >> 16) & 0xFFu, mg = (mod  >> 8) & 0xFFu, mb = mod  & 0xFFu;
    const u32 rr = (br * mr) >> 8;
    const u32 rg = (bg * mg) >> 8;
    const u32 rb = (bb * mb) >> 8;
    return 0xFF000000u | (rr << 16) | (rg << 8) | rb;
}

/*
 * M9: subspan FDIV (truco clásico de Abrash, Quake era).
 *
 * Perspective correcta requiere dividir (u/w)/(1/w) por pixel — 1 FDIV de
 * latencia alta. La observación clave: si entre dos subspan boundaries
 * los valores correctos de u, v son lineales (afín), entonces podemos
 * interpolar u, v lineal DENTRO del subspan, dividiendo solo en los
 * extremos. Para subspans de 16 pixels, eso baja FDIVs por pixel de 1.0
 * a 1/16 (~0.06) sin artifact visible.
 *
 * Inner loop ahora es 2 adds + tex sample (+ lightmap sample) — perfecto
 * para que el FDIV de inicio de subspan se solape con el bucle por
 * pipelining del CPU.
 */
#define SUBSPAN 16

static void draw_span_perspective(int y, int x0, int x1, void *userdata) {
    grad_ctx_t *c = (grad_ctx_t *)userdata;
    const f32 xs = (f32)x0 + 0.5f;
    const f32 ys = (f32)y  + 0.5f;

    /* Acumuladores lineales de 1/w, u/w, v/w en screen space. */
    f32 inv_w_acc = c->inv_w_origin + xs * c->inv_w_dx + ys * c->inv_w_dy;
    f32 u_w_acc   = c->u_w_origin   + xs * c->u_w_dx   + ys * c->u_w_dy;
    f32 v_w_acc   = c->v_w_origin   + xs * c->v_w_dx   + ys * c->v_w_dy;

    /* u, v exactos en el inicio del span (FDIV #1). */
    f32 tu_left = u_w_acc / inv_w_acc;
    f32 tv_left = v_w_acc / inv_w_acc;

    u32 *p = c->fb->color + (size_t)y * c->fb->pitch_pixels + x0;

    int x = x0;
    while (x < x1) {
        int x_next = x + SUBSPAN;
        if (x_next > x1) x_next = x1;
        const int len = x_next - x;
        const f32 len_f = (f32)len;

        /* Step interpolants al próximo subspan boundary. */
        inv_w_acc += c->inv_w_dx * len_f;
        u_w_acc   += c->u_w_dx   * len_f;
        v_w_acc   += c->v_w_dx   * len_f;

        /* u, v exactos al final del subspan (otro FDIV, una cada 16 pixels). */
        const f32 w_right  = 1.0f / inv_w_acc;
        const f32 tu_right = u_w_acc * w_right;
        const f32 tv_right = v_w_acc * w_right;

        /* Deltas afín dentro del subspan. */
        const f32 inv_len = 1.0f / len_f;
        const f32 du = (tu_right - tu_left) * inv_len;
        const f32 dv = (tv_right - tv_left) * inv_len;

        f32 tu = tu_left;
        f32 tv = tv_left;

        /* Dos copias del inner loop para que el branch del lightmap salga
         * de la región caliente. El compilador suele inline el tex callback. */
        if (c->lm) {
            for (int i = 0; i < len; ++i) {
                const u32 base = c->tex(tu, tv);
                const u32 mod  = lightmap_sample_bilinear(c->lm, tu, tv);
                *p++ = modulate_argb(base, mod);
                tu += du; tv += dv;
            }
        } else {
            for (int i = 0; i < len; ++i) {
                *p++ = c->tex(tu, tv);
                tu += du; tv += dv;
            }
        }

        tu_left = tu_right;
        tv_left = tv_right;
        x = x_next;
    }
}

void rast_scan_textured(framebuf_t *fb, span_buffer_t *sb,
                        screen_vert_t v0, screen_vert_t v1, screen_vert_t v2,
                        tex_sample_fn tex,
                        const lightmap_t *lm,
                        int y_clip_min, int y_clip_max) {
    /* Backface cull. En mi convención (Y flipped en pantalla), una cara
     * front-facing tiene signed area NEGATIVA. */
    const f32 signed_area = (v1.x - v0.x) * (v2.y - v0.y)
                          - (v1.y - v0.y) * (v2.x - v0.x);
    if (signed_area > 0.0f) return;

    /* Sort por Y: v0 = top, v1 = mid, v2 = bot. */
    screen_vert_t tmp;
    if (v0.y > v1.y) { tmp = v0; v0 = v1; v1 = tmp; }
    if (v1.y > v2.y) { tmp = v1; v1 = v2; v2 = tmp; }
    if (v0.y > v1.y) { tmp = v0; v0 = v1; v1 = tmp; }

    /* Plane gradients para 1/w, u/w, v/w (linear en screen space). */
    const f32 dx10 = v1.x - v0.x, dy10 = v1.y - v0.y;
    const f32 dx20 = v2.x - v0.x, dy20 = v2.y - v0.y;
    const f32 det  = dx10 * dy20 - dy10 * dx20;
    if (fabsf(det) < 1e-6f) return;
    const f32 inv_det = 1.0f / det;

    const f32 iw0 = v0.inv_w, iw1 = v1.inv_w, iw2 = v2.inv_w;
    const f32 uw0 = v0.u * iw0, uw1 = v1.u * iw1, uw2 = v2.u * iw2;
    const f32 vw0 = v0.v * iw0, vw1 = v1.v * iw1, vw2 = v2.v * iw2;

    grad_ctx_t c;
    c.fb  = fb;
    c.tex = tex;
    c.lm  = lm;

#define GRAD(a0, a1, a2, dx_out, dy_out, org_out) do {                     \
        const f32 _d10 = (a1) - (a0);                                       \
        const f32 _d20 = (a2) - (a0);                                       \
        (dx_out)  = (_d10 * dy20 - _d20 * dy10) * inv_det;                  \
        (dy_out)  = (_d20 * dx10 - _d10 * dx20) * inv_det;                  \
        (org_out) = (a0) - (dx_out) * v0.x - (dy_out) * v0.y;               \
    } while (0)

    GRAD(iw0, iw1, iw2, c.inv_w_dx, c.inv_w_dy, c.inv_w_origin);
    GRAD(uw0, uw1, uw2, c.u_w_dx,   c.u_w_dy,   c.u_w_origin);
    GRAD(vw0, vw1, vw2, c.v_w_dx,   c.v_w_dy,   c.v_w_origin);

#undef GRAD

    /* Scanline range. Top-left fill rule via ceilf en y_top y x_left. */
    const int y_top = (int)ceilf(v0.y);
    const int y_bot = (int)ceilf(v2.y);
    const int y_mid = (int)ceilf(v1.y);

    const f32 dy_long  = v2.y - v0.y;
    const f32 dxdy_long = (dy_long > 1e-6f) ? (v2.x - v0.x) / dy_long : 0.0f;
    const f32 dy_upper = v1.y - v0.y;
    const f32 dxdy_upper = (dy_upper > 1e-6f) ? (v1.x - v0.x) / dy_upper : 0.0f;
    const f32 dy_lower = v2.y - v1.y;
    const f32 dxdy_lower = (dy_lower > 1e-6f) ? (v2.x - v1.x) / dy_lower : 0.0f;

    int y_start = y_top;
    int y_end   = y_bot;
    if (y_start < y_clip_min) y_start = y_clip_min;
    if (y_end   > y_clip_max) y_end   = y_clip_max;
    if (y_start < 0)           y_start = 0;
    if (y_end   > fb->h)       y_end   = fb->h;

    for (int y = y_start; y < y_end; ++y) {
        const f32 yc     = (f32)y + 0.5f;
        const f32 x_long = v0.x + (yc - v0.y) * dxdy_long;

        f32 x_short;
        if (y < y_mid && dy_upper > 1e-6f) {
            x_short = v0.x + (yc - v0.y) * dxdy_upper;
        } else if (dy_lower > 1e-6f) {
            x_short = v1.x + (yc - v1.y) * dxdy_lower;
        } else {
            continue;
        }

        const f32 xl_f = (x_long < x_short) ? x_long : x_short;
        const f32 xr_f = (x_long < x_short) ? x_short : x_long;
        const int xl   = (int)ceilf(xl_f);
        const int xr   = (int)ceilf(xr_f);
        if (xl >= xr) continue;

        span_buffer_emit(sb, y, xl, xr, draw_span_perspective, &c);
    }
}
