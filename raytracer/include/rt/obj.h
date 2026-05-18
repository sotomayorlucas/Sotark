#ifndef SOTARK_RT_OBJ_H
#define SOTARK_RT_OBJ_H

#include "common/types.h"
#include "common/vec.h"

/*
 * Loader minimal para Wavefront OBJ.
 *
 * Soporta:
 *   - `v x y z`            — vertex position
 *   - `f i j k ...`        — polygon face (1-indexed). Fan triangulation
 *                            de polygons con > 3 vertices.
 *   - `f i/t/n j/t/n ...`  — slashes ignorados (solo se usa el primer índice)
 *   - `#` comments         — skipped
 *   - groups, materials    — silently ignored
 *
 * Las vertices y triangles se alocan con malloc; liberar con obj_free.
 */
typedef struct {
    vec3_t *verts;
    int     n_verts;
    int    (*tris)[3];   /* índices a verts (0-indexed) */
    int     n_tris;
} obj_mesh_t;

/* Devuelve true si pudo abrir y parsear el archivo (mismo si quedó vacío). */
bool obj_load(obj_mesh_t *mesh, const char *path);
void obj_free(obj_mesh_t *mesh);

#endif
