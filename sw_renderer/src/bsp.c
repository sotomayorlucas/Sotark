#include "bsp.h"

void bsp_walk(const bsp_t *bsp, int idx, vec3_t camera,
              bsp_face_cb cb, void *userdata) {
    if (idx < 0) {
        /* Hoja: leaf_id = -idx - 1. */
        const int leaf_id = -idx - 1;
        const bsp_leaf_t *leaf = &bsp->leaves[leaf_id];
        for (int i = 0; i < leaf->n_faces; ++i) {
            cb(&bsp->faces[leaf->face_indices[i]], userdata);
        }
        return;
    }

    const bsp_node_t *node = &bsp->nodes[idx];
    const int side = plane_classify(node->plane, camera);
    if (side >= 0) {
        /* Cámara en el front (o sobre el plano): front primero, back después. */
        bsp_walk(bsp, node->front, camera, cb, userdata);
        bsp_walk(bsp, node->back,  camera, cb, userdata);
    } else {
        bsp_walk(bsp, node->back,  camera, cb, userdata);
        bsp_walk(bsp, node->front, camera, cb, userdata);
    }
}
