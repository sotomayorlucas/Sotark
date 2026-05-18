#include "rt/obj.h"
#include "common/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool obj_load(obj_mesh_t *m, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        LOG_ERROR("obj_load: could not open '%s'", path);
        return false;
    }

    int cap_v = 1024;
    int cap_t = 1024;
    m->verts = (vec3_t *)malloc((size_t)cap_v * sizeof(vec3_t));
    m->tris  = (int (*)[3])malloc((size_t)cap_t * 3 * sizeof(int));
    m->n_verts = 0;
    m->n_tris  = 0;
    if (!m->verts || !m->tris) {
        LOG_ERROR("obj_load: oom");
        fclose(f);
        free(m->verts); free(m->tris);
        m->verts = NULL; m->tris = NULL;
        return false;
    }

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == 'v' && line[1] == ' ') {
            /* Vertex. */
            vec3_t v;
            if (sscanf(line, "v %f %f %f", &v.x, &v.y, &v.z) == 3) {
                if (m->n_verts >= cap_v) {
                    cap_v *= 2;
                    m->verts = (vec3_t *)realloc(m->verts, (size_t)cap_v * sizeof(vec3_t));
                }
                m->verts[m->n_verts++] = v;
            }
        } else if (line[0] == 'f' && line[1] == ' ') {
            /* Face: parse hasta 16 vertex indices, ignorando los slashes
             * (para 1/2/3 → solo agarramos el 1). */
            int idx[16];
            int n_idx = 0;
            const char *p = line + 2;
            while (*p && n_idx < 16) {
                while (*p == ' ' || *p == '\t') p++;
                if (*p == '\0' || *p == '\n' || *p == '\r') break;
                int i = 0;
                if (sscanf(p, "%d", &i) != 1) break;
                /* OBJ permite índices negativos (offset desde el final). */
                if (i < 0) i = m->n_verts + i + 1;
                idx[n_idx++] = i - 1;   /* OBJ 1-indexed → 0-indexed */
                while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') p++;
            }
            /* Fan triangulation: (idx[0], idx[i], idx[i+1]) para i in 1..n-2. */
            for (int i = 1; i < n_idx - 1; ++i) {
                if (m->n_tris >= cap_t) {
                    cap_t *= 2;
                    m->tris = (int (*)[3])realloc(m->tris, (size_t)cap_t * 3 * sizeof(int));
                }
                /* Validar índices contra n_verts */
                int a = idx[0], b = idx[i], c = idx[i + 1];
                if (a < 0 || a >= m->n_verts ||
                    b < 0 || b >= m->n_verts ||
                    c < 0 || c >= m->n_verts) continue;
                m->tris[m->n_tris][0] = a;
                m->tris[m->n_tris][1] = b;
                m->tris[m->n_tris][2] = c;
                m->n_tris++;
            }
        }
        /* otras líneas (vt, vn, g, mtllib, usemtl, #, etc.) → ignored */
    }
    fclose(f);
    LOG_INFO("obj_load '%s': %d vertices, %d triangles",
             path, m->n_verts, m->n_tris);
    return true;
}

void obj_free(obj_mesh_t *m) {
    free(m->verts);
    free(m->tris);
    m->verts = NULL;
    m->tris  = NULL;
    m->n_verts = 0;
    m->n_tris  = 0;
}
