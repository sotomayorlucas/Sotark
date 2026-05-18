#ifndef SOTARK_RT_CAMERA_H
#define SOTARK_RT_CAMERA_H

#include "common/types.h"
#include "common/vec.h"
#include "common/rng.h"
#include "rt/ray.h"

/*
 * Cámara positionable con defocus blur (a.k.a. depth of field).
 *
 * Shirley §13: el "lens disk" simula un objetivo real. Cada sample saca su
 * ray no desde el centro de la cámara sino desde un punto random del disco;
 * todos los rayos del lens convergen en el plano focal. Resultado: objetos
 * en el focal plane están nítidos; lo demás queda fuera de foco con bokeh
 * proporcional al defocus_angle.
 *
 * Si defocus_angle <= 0, el lens degenera a un pinhole (cámara M3 clásica).
 */
typedef struct {
    vec3_t center;          /* lookfrom — origen de los rayos pinhole */
    vec3_t pixel00_loc;
    vec3_t pixel_delta_u;
    vec3_t pixel_delta_v;
    vec3_t defocus_disk_u;  /* base del lens disk (radio = defocus_radius) */
    vec3_t defocus_disk_v;
    bool   has_defocus;
} camera_t;

void  camera_init(camera_t *cam,
                  vec3_t lookfrom, vec3_t lookat, vec3_t vup,
                  f32 vfov_deg,
                  int image_width, int image_height,
                  f32 defocus_angle_deg, f32 focus_dist);

/* Genera un ray para el pixel (i, j) con jitter de antialiasing + sampling
 * del lens disk si has_defocus. */
ray_t camera_get_ray(const camera_t *cam, int i, int j, pcg32_t *rng);

#endif
