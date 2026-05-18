#ifndef SOTARK_COMMON_MAT_H
#define SOTARK_COMMON_MAT_H

#include "common/types.h"
#include "common/vec.h"

/* Column-major mat4. Element (row r, col c) is m[c*4 + r].
 * Convention matches OpenGL / glm so vectors are right-multiplied:
 *   v_clip = P * V * M * v_world. */
typedef struct { f32 m[16]; } mat4_t;

mat4_t mat4_zero(void);
mat4_t mat4_identity(void);

/* Returns a * b. Equivalent to: apply b first, then a. */
mat4_t mat4_mul(mat4_t a, mat4_t b);
vec4_t mat4_mul_vec4(mat4_t a, vec4_t v);

mat4_t mat4_translate(vec3_t t);
mat4_t mat4_scale(vec3_t s);
mat4_t mat4_rotate_x(f32 angle_rad);
mat4_t mat4_rotate_y(f32 angle_rad);
mat4_t mat4_rotate_z(f32 angle_rad);

/* Right-handed perspective. Camera looks down -Z. Maps z=-near -> NDC z=-1,
 * z=-far -> NDC z=+1. fov_y is the full vertical field-of-view in radians. */
mat4_t mat4_perspective(f32 fov_y_rad, f32 aspect, f32 near, f32 far);

mat4_t mat4_look_at(vec3_t eye, vec3_t target, vec3_t up);

#endif
