module;

#include <cmath>

export module sotark.rt:camera;

import sotark.common;
import :ray;
import :sampling;

export namespace sotark::rt {

// Positionable camera with optional defocus blur (Shirley §13). If
// defocus_angle <= 0 the lens degenerates to a pinhole.
class Camera {
public:
    Camera(Vec3 lookfrom, Vec3 lookat, Vec3 vup,
           f32 vfov_deg,
           int image_width, int image_height,
           f32 defocus_angle_deg, f32 focus_dist) noexcept
        : center_{lookfrom} {
        const f32 theta            = deg_to_rad(vfov_deg);
        const f32 h                = std::tan(theta * 0.5f);
        const f32 viewport_height  = 2.0f * h * focus_dist;
        const f32 viewport_width   = viewport_height *
                                      static_cast<f32>(image_width) / static_cast<f32>(image_height);

        const Vec3 w = normalize(lookfrom - lookat);
        const Vec3 u = normalize(cross(vup, w));
        const Vec3 v = cross(w, u);

        const Vec3 viewport_u = u * viewport_width;
        const Vec3 viewport_v = -v * viewport_height;   // Y down
        pixel_delta_u_ = viewport_u * (1.0f / static_cast<f32>(image_width));
        pixel_delta_v_ = viewport_v * (1.0f / static_cast<f32>(image_height));

        const Vec3 viewport_upper_left =
            lookfrom - w * focus_dist - viewport_u * 0.5f - viewport_v * 0.5f;
        pixel00_loc_ = viewport_upper_left + (pixel_delta_u_ + pixel_delta_v_) * 0.5f;

        has_defocus_ = defocus_angle_deg > 0.0f;
        if (has_defocus_) {
            const f32 defocus_rad =
                focus_dist * std::tan(deg_to_rad(defocus_angle_deg) * 0.5f);
            defocus_disk_u_ = u * defocus_rad;
            defocus_disk_v_ = v * defocus_rad;
        }
    }

    Ray get_ray(int i, int j, Pcg32& rng) const noexcept {
        const f32 du = rng.float01() - 0.5f;
        const f32 dv = rng.float01() - 0.5f;
        const Vec3 pixel_sample =
            pixel00_loc_ + pixel_delta_u_ * (static_cast<f32>(i) + du)
                          + pixel_delta_v_ * (static_cast<f32>(j) + dv);

        Vec3 origin = center_;
        if (has_defocus_) {
            const Vec3 disk = rng_in_unit_disk(rng);
            origin = center_ + defocus_disk_u_ * disk.x + defocus_disk_v_ * disk.y;
        }
        return Ray{ origin, pixel_sample - origin };
    }

private:
    Vec3 center_{};
    Vec3 pixel00_loc_{};
    Vec3 pixel_delta_u_{};
    Vec3 pixel_delta_v_{};
    Vec3 defocus_disk_u_{};
    Vec3 defocus_disk_v_{};
    bool has_defocus_{false};
};

}  // namespace sotark::rt
