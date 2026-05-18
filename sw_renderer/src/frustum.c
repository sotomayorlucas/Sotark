#include "frustum.h"

void frustum_from_pv(frustum_t *f, mat4_t m) {
    /* MVP es column-major: element (row i, col j) = m.m[j*4 + i].
     * Para extraer "row 0 + row 3" sumamos componentes en cada columna.
     *
     * Plano "punto dentro": en clip coords clip.x >= -clip.w (left), etc.
     * Como clip = M*v, eso es (row_x + row_w) · v >= 0. El vector
     * (row_x + row_w) son los coeficientes (a, b, c, d) del plano. */
    for (int j = 0; j < 4; ++j) {
        const f32 r0 = m.m[j * 4 + 0];   /* row 0 (x) */
        const f32 r1 = m.m[j * 4 + 1];   /* row 1 (y) */
        const f32 r2 = m.m[j * 4 + 2];   /* row 2 (z) */
        const f32 r3 = m.m[j * 4 + 3];   /* row 3 (w) */
        f->planes[0][j] = r3 + r0;   /* left:   x + w >= 0  */
        f->planes[1][j] = r3 - r0;   /* right:  -x + w >= 0 */
        f->planes[2][j] = r3 + r1;   /* bottom: y + w >= 0  */
        f->planes[3][j] = r3 - r1;   /* top:    -y + w >= 0 */
        f->planes[4][j] = r3 + r2;   /* near:   z + w >= 0  */
        f->planes[5][j] = r3 - r2;   /* far:    -z + w >= 0 */
    }
}

bool frustum_cull_aabb(const frustum_t *f, aabb_t box) {
    for (int i = 0; i < 6; ++i) {
        const f32 a = f->planes[i][0];
        const f32 b = f->planes[i][1];
        const f32 c = f->planes[i][2];
        const f32 d = f->planes[i][3];
        /* "Positive vertex": esquina de la AABB más lejos en la dirección
         * (a, b, c). Si esa esquina está fuera del plano, TODA la AABB
         * lo está. Esto descarta casos "obviamente fuera" rápido. */
        const f32 px = (a > 0.0f) ? box.max.x : box.min.x;
        const f32 py = (b > 0.0f) ? box.max.y : box.min.y;
        const f32 pz = (c > 0.0f) ? box.max.z : box.min.z;
        if (a * px + b * py + c * pz + d < 0.0f) {
            return true;   /* fuera de este plano -> fuera del frustum */
        }
    }
    return false;          /* no se pudo descartar; dibujar */
}
