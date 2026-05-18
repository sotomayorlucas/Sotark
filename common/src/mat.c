#include "common/mat.h"

#include <math.h>
#include <string.h>

mat4_t mat4_zero(void) {
    mat4_t r;
    memset(r.m, 0, sizeof(r.m));
    return r;
}

mat4_t mat4_identity(void) {
    mat4_t r = mat4_zero();
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
    return r;
}

mat4_t mat4_mul(mat4_t a, mat4_t b) {
    mat4_t r = mat4_zero();
    for (int j = 0; j < 4; ++j) {
        for (int i = 0; i < 4; ++i) {
            f32 s = 0.0f;
            for (int k = 0; k < 4; ++k) {
                s += a.m[k * 4 + i] * b.m[j * 4 + k];
            }
            r.m[j * 4 + i] = s;
        }
    }
    return r;
}

vec4_t mat4_mul_vec4(mat4_t a, vec4_t v) {
    vec4_t r;
    r.x = a.m[0] * v.x + a.m[4] * v.y + a.m[8]  * v.z + a.m[12] * v.w;
    r.y = a.m[1] * v.x + a.m[5] * v.y + a.m[9]  * v.z + a.m[13] * v.w;
    r.z = a.m[2] * v.x + a.m[6] * v.y + a.m[10] * v.z + a.m[14] * v.w;
    r.w = a.m[3] * v.x + a.m[7] * v.y + a.m[11] * v.z + a.m[15] * v.w;
    return r;
}

mat4_t mat4_translate(vec3_t t) {
    mat4_t r = mat4_identity();
    r.m[12] = t.x;
    r.m[13] = t.y;
    r.m[14] = t.z;
    return r;
}

mat4_t mat4_scale(vec3_t s) {
    mat4_t r = mat4_zero();
    r.m[0]  = s.x;
    r.m[5]  = s.y;
    r.m[10] = s.z;
    r.m[15] = 1.0f;
    return r;
}

mat4_t mat4_rotate_x(f32 a) {
    mat4_t r = mat4_identity();
    f32 c = cosf(a), s = sinf(a);
    r.m[5] = c;  r.m[6]  = s;
    r.m[9] = -s; r.m[10] = c;
    return r;
}

mat4_t mat4_rotate_y(f32 a) {
    mat4_t r = mat4_identity();
    f32 c = cosf(a), s = sinf(a);
    r.m[0] = c;  r.m[2]  = -s;
    r.m[8] = s;  r.m[10] = c;
    return r;
}

mat4_t mat4_rotate_z(f32 a) {
    mat4_t r = mat4_identity();
    f32 c = cosf(a), s = sinf(a);
    r.m[0] = c;  r.m[1] = s;
    r.m[4] = -s; r.m[5] = c;
    return r;
}

mat4_t mat4_perspective(f32 fov_y, f32 aspect, f32 n, f32 f) {
    mat4_t r = mat4_zero();
    f32 fy = 1.0f / tanf(fov_y * 0.5f);
    r.m[0]  = fy / aspect;
    r.m[5]  = fy;
    r.m[10] = (f + n) / (n - f);
    r.m[11] = -1.0f;
    r.m[14] = (2.0f * f * n) / (n - f);
    return r;
}

mat4_t mat4_look_at(vec3_t eye, vec3_t target, vec3_t up) {
    vec3_t f = v3_normalize(v3_sub(target, eye));
    vec3_t s = v3_normalize(v3_cross(f, up));
    vec3_t u = v3_cross(s, f);
    mat4_t r = mat4_identity();
    r.m[0]  = s.x;   r.m[4]  = s.y;   r.m[8]   = s.z;
    r.m[1]  = u.x;   r.m[5]  = u.y;   r.m[9]   = u.z;
    r.m[2]  = -f.x;  r.m[6]  = -f.y;  r.m[10]  = -f.z;
    r.m[12] = -v3_dot(s, eye);
    r.m[13] = -v3_dot(u, eye);
    r.m[14] =  v3_dot(f, eye);
    return r;
}
