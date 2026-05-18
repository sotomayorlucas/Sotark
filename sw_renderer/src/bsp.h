#ifndef SOTARK_SW_BSP_H
#define SOTARK_SW_BSP_H

#include "common/types.h"
#include "common/vec.h"
#include "common/plane.h"
#include "texture.h"

/*
 * Estructuras BSP estilo Quake (simplificado):
 *   - bsp_face_t: un quad texturado (4 verts + 4 UVs + textura).
 *   - bsp_node_t: nodo interno con plano de partición + 2 hijos.
 *   - bsp_leaf_t: hoja con una lista de índices a faces visibles allí.
 *
 * Convención de índices: un valor positivo apunta a un nodo interno; un
 * valor negativo apunta a una hoja con (leaf_id = -idx - 1). Esto permite
 * un solo "puntero" int por hijo, igual que el .bsp v29.
 *
 * La walk recursiva visita primero el lado del plano donde está la cámara
 * — front-to-back. Con z-buffer (M2) eso es opcional para correctness pero
 * vital para M7 (span buffer) donde el orden DETERMINA visibilidad.
 */

typedef struct {
    vec3_t        verts[4];
    vec2_t        uvs[4];
    tex_sample_fn tex;
} bsp_face_t;

typedef struct {
    plane_t plane;
    int     front;     /* index de hijo: + interno, - = -(leaf_id+1) */
    int     back;
} bsp_node_t;

typedef struct {
    const int *face_indices;
    int        n_faces;
} bsp_leaf_t;

typedef struct {
    const bsp_face_t *faces;
    int               n_faces;
    const bsp_node_t *nodes;
    int               n_nodes;
    const bsp_leaf_t *leaves;
    int               n_leaves;
} bsp_t;

/* Callback por cada face visitada. orden = orden de la walk (front-to-back). */
typedef void (*bsp_face_cb)(const bsp_face_t *face, void *userdata);

/* Recorre el BSP desde el root (idx = nodo 0 o leaf -1) y llama cb por cada
 * face en orden front-to-back desde la cámara. */
void bsp_walk(const bsp_t *bsp, int root_idx, vec3_t camera,
              bsp_face_cb cb, void *userdata);

#endif
