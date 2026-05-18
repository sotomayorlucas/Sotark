#include "rt/camera.h"
#include "rt/sampling.h"

#include <math.h>

void camera_init(camera_t *cam,
                 vec3_t lookfrom, vec3_t lookat, vec3_t vup,
                 f32 vfov_deg,
                 int image_width, int image_height,
                 f32 defocus_angle_deg, f32 focus_dist) {
    cam->center = lookfrom;

    /* Viewport dimensions en el plano focal. */
    const f32 theta           = vfov_deg * 3.14159265358979f / 180.0f;
    const f32 h               = tanf(theta * 0.5f);
    const f32 viewport_height = 2.0f * h * focus_dist;
    const f32 viewport_width  = viewport_height * (f32)image_width / (f32)image_height;

    /* Base ortonormal de la cámara: w=back, u=right, v=up. */
    const vec3_t w = v3_normalize(v3_sub(lookfrom, lookat));
    const vec3_t u = v3_normalize(v3_cross(vup, w));
    const vec3_t v = v3_cross(w, u);

    const vec3_t viewport_u = v3_scale(u,           viewport_width);
    const vec3_t viewport_v = v3_scale(v3_neg(v),   viewport_height); /* Y down */
    cam->pixel_delta_u = v3_scale(viewport_u, 1.0f / (f32)image_width);
    cam->pixel_delta_v = v3_scale(viewport_v, 1.0f / (f32)image_height);

    const vec3_t viewport_upper_left =
        v3_sub(v3_sub(v3_sub(lookfrom, v3_scale(w, focus_dist)),
                      v3_scale(viewport_u, 0.5f)),
               v3_scale(viewport_v, 0.5f));
    cam->pixel00_loc =
        v3_add(viewport_upper_left,
               v3_scale(v3_add(cam->pixel_delta_u, cam->pixel_delta_v), 0.5f));

    cam->has_defocus = defocus_angle_deg > 0.0f;
    if (cam->has_defocus) {
        const f32 defocus_rad =
            focus_dist * tanf(defocus_angle_deg * 3.14159265358979f / 180.0f * 0.5f);
        cam->defocus_disk_u = v3_scale(u, defocus_rad);
        cam->defocus_disk_v = v3_scale(v, defocus_rad);
    } else {
        cam->defocus_disk_u = (vec3_t){ 0.0f, 0.0f, 0.0f };
        cam->defocus_disk_v = (vec3_t){ 0.0f, 0.0f, 0.0f };
    }
}

ray_t camera_get_ray(const camera_t *cam, int i, int j, pcg32_t *rng) {
    const f32 du = pcg32_float01(rng) - 0.5f;
    const f32 dv = pcg32_float01(rng) - 0.5f;
    const vec3_t pixel_sample =
        v3_add(cam->pixel00_loc,
               v3_add(v3_scale(cam->pixel_delta_u, (f32)i + du),
                      v3_scale(cam->pixel_delta_v, (f32)j + dv)));

    vec3_t origin = cam->center;
    if (cam->has_defocus) {
        const vec3_t disk = rng_in_unit_disk(rng);
        origin = v3_add(v3_add(cam->center, v3_scale(cam->defocus_disk_u, disk.x)),
                                            v3_scale(cam->defocus_disk_v, disk.y));
    }
    return (ray_t){ origin, v3_sub(pixel_sample, origin) };
}
