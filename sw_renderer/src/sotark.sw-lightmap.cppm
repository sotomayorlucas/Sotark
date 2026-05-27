module;

#include <vector>
#include <cmath>
#include <algorithm>
#include <span>

export module sotark.sw:lightmap;

import sotark.common;
import :bsp;

export namespace sotark::sw {

// Per-face static lightmap (Quake-era idea).
// Owns the texel storage. Sampled bilinearly during render and multiplied
// against the base texture, producing baked shadows / falloff "for free".
class Lightmap {
public:
    Lightmap() = default;
    Lightmap(int w, int h)
        : texels_(static_cast<usize>(w) * static_cast<usize>(h), 0u),
          w_{w}, h_{h} {}

    int  width()  const noexcept { return w_; }
    int  height() const noexcept { return h_; }
    std::span<u32>       texels()       noexcept { return texels_; }
    std::span<const u32> texels() const noexcept { return texels_; }

    f32 u_min{0}, u_max{1};
    f32 v_min{0}, v_max{1};

    u32 sample_bilinear(f32 u, f32 v) const noexcept;

private:
    std::vector<u32> texels_;
    int              w_{0}, h_{0};
};

inline u32 Lightmap::sample_bilinear(f32 u, f32 v) const noexcept {
    f32 fu = (u - u_min) / (u_max - u_min) * static_cast<f32>(w_ - 1);
    f32 fv = (v - v_min) / (v_max - v_min) * static_cast<f32>(h_ - 1);
    fu = std::max(0.0f, fu);
    fv = std::max(0.0f, fv);
    const f32 max_u = static_cast<f32>(w_ - 1) - 1e-4f;
    const f32 max_v = static_cast<f32>(h_ - 1) - 1e-4f;
    if (fu > max_u) fu = max_u;
    if (fv > max_v) fv = max_v;

    const int iu = static_cast<int>(std::floor(fu));
    const int iv = static_cast<int>(std::floor(fv));
    const f32 fx = fu - static_cast<f32>(iu);
    const f32 fy = fv - static_cast<f32>(iv);

    const u32 c00 = texels_[iv      * w_ + iu    ];
    const u32 c10 = texels_[iv      * w_ + iu + 1];
    const u32 c01 = texels_[(iv + 1)* w_ + iu    ];
    const u32 c11 = texels_[(iv + 1)* w_ + iu + 1];

    const f32 w00 = (1.0f - fx) * (1.0f - fy);
    const f32 w10 = fx          * (1.0f - fy);
    const f32 w01 = (1.0f - fx) * fy;
    const f32 w11 = fx          * fy;

    auto mix = [&](u32 shift) {
        return static_cast<u32>(
            static_cast<f32>((c00 >> shift) & 0xFFu) * w00 +
            static_cast<f32>((c10 >> shift) & 0xFFu) * w10 +
            static_cast<f32>((c01 >> shift) & 0xFFu) * w01 +
            static_cast<f32>((c11 >> shift) & 0xFFu) * w11);
    };
    const u32 r = mix(16);
    const u32 g = mix( 8);
    const u32 b = mix( 0);
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

// Bilinear world-pos interp of a quad.
constexpr Vec3 quad_interp(const std::array<Vec3, 4>& verts, f32 u, f32 v) noexcept {
    const Vec3 bottom = verts[0] * (1.0f - u) + verts[1] * u;
    const Vec3 top    = verts[3] * (1.0f - u) + verts[2] * u;
    return bottom * (1.0f - v) + top * v;
}

// Ray-vs-quad blocker test.
inline bool quad_blocks(const BspFace& face, Vec3 origin, Vec3 dir, f32 t_max) noexcept {
    const Vec3 e1 = face.verts[1] - face.verts[0];
    const Vec3 e2 = face.verts[3] - face.verts[0];
    const Vec3 n  = cross(e1, e2);
    const f32  denom = dot(n, dir);
    if (std::abs(denom) < 1e-8f) return false;
    const f32 D = dot(n, face.verts[0]);
    const f32 t = (D - dot(n, origin)) / denom;
    if (t <= 0.001f || t >= t_max) return false;

    const Vec3 P = origin + dir * t;
    const Vec3 local = P - face.verts[0];
    const f32  a = dot(local, e1) / dot(e1, e1);
    const f32  b = dot(local, e2) / dot(e2, e2);
    return (a >= 0.0f && a <= 1.0f && b >= 0.0f && b <= 1.0f);
}

inline void lightmap_bake(Lightmap& lm,
                           const BspFace& face,
                           std::span<const BspFace> all_faces, int skip_idx,
                           Vec3 light_pos, f32 light_intensity, f32 ambient) {
    const Vec3 e1 = face.verts[1] - face.verts[0];
    const Vec3 e2 = face.verts[3] - face.verts[0];
    const Vec3 normal = normalize(cross(e1, e2));

    // UV range of this face.
    f32 u_min = face.uvs[0].x, u_max = face.uvs[0].x;
    f32 v_min = face.uvs[0].y, v_max = face.uvs[0].y;
    for (int i = 1; i < 4; ++i) {
        u_min = std::min(u_min, face.uvs[i].x);
        u_max = std::max(u_max, face.uvs[i].x);
        v_min = std::min(v_min, face.uvs[i].y);
        v_max = std::max(v_max, face.uvs[i].y);
    }
    lm.u_min = u_min; lm.u_max = u_max;
    lm.v_min = v_min; lm.v_max = v_max;

    auto texels = lm.texels();
    const int W = lm.width(), H = lm.height();
    const int n_faces = static_cast<int>(all_faces.size());

    for (int j = 0; j < H; ++j) {
        for (int i = 0; i < W; ++i) {
            const f32 pu = (static_cast<f32>(i) + 0.5f) / static_cast<f32>(W);
            const f32 pv = (static_cast<f32>(j) + 0.5f) / static_cast<f32>(H);
            const Vec3 P = quad_interp(face.verts, pu, pv);
            const Vec3 P_off = P + normal * 0.001f;
            const Vec3 to_L  = light_pos - P_off;
            const f32  dist_sq = length_sq(to_L);
            const f32  dist    = std::sqrt(dist_sq);
            const Vec3 L_dir   = to_L * (1.0f / dist);
            const f32  cos_theta = dot(normal, L_dir);

            f32 lit = ambient;
            if (cos_theta > 0.0f) {
                bool blocked = false;
                for (int k = 0; k < n_faces; ++k) {
                    if (k == skip_idx) continue;
                    if (quad_blocks(all_faces[k], P_off, to_L, 1.0f)) { blocked = true; break; }
                }
                if (!blocked) lit += light_intensity * cos_theta / dist_sq;
            }
            lit = std::clamp(lit, 0.0f, 1.0f);
            const u8 b = static_cast<u8>(lit * 255.0f);
            texels[j * W + i] = 0xFF000000u
                              | (static_cast<u32>(b) << 16)
                              | (static_cast<u32>(b) <<  8)
                              | static_cast<u32>(b);
        }
    }
}

}  // namespace sotark::sw
