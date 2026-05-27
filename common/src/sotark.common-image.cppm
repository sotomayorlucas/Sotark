module;

#include <vector>
#include <string>
#include <string_view>
#include <expected>
#include <mdspan>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <format>
#include <utility>

export module sotark.common:image;

import :types;
import :vec;
import :color;

export namespace sotark {

// Row-major, top-left origin. Backed by std::vector for owning storage.
// std::mdspan view exposed for 2D-indexed iteration without bounds-check overhead.
class Image {
public:
    Image() = default;
    Image(int w, int h) : w_{w}, h_{h}, pixels_(static_cast<usize>(w) * static_cast<usize>(h)) {}

    int  width()  const noexcept { return w_; }
    int  height() const noexcept { return h_; }
    bool empty()  const noexcept { return pixels_.empty(); }

    Rgb&       operator()(int x, int y)       noexcept { return pixels_[y * w_ + x]; }
    const Rgb& operator()(int x, int y) const noexcept { return pixels_[y * w_ + x]; }

    // C++23 std::mdspan 2D view: pixels(y, x).
    using View      = std::mdspan<Rgb,       std::dextents<usize, 2>>;
    using ConstView = std::mdspan<const Rgb, std::dextents<usize, 2>>;

    View      view()       noexcept { return View{pixels_.data(), static_cast<usize>(h_), static_cast<usize>(w_)}; }
    ConstView view() const noexcept { return ConstView{pixels_.data(), static_cast<usize>(h_), static_cast<usize>(w_)}; }

    std::span<Rgb>       pixels()       noexcept { return pixels_; }
    std::span<const Rgb> pixels() const noexcept { return pixels_; }

private:
    int              w_{0};
    int              h_{0};
    std::vector<Rgb> pixels_;
};

// Binary PPM (P6). Writes gamma-2 corrected bytes (Shirley convention).
inline std::expected<void, std::string>
write_ppm(const Image& img, std::string_view path) noexcept {
    std::string path_str(path);
    std::FILE* f = std::fopen(path_str.c_str(), "wb");
    if (!f) return std::unexpected(std::format("could not open '{}' for writing", path));
    std::fprintf(f, "P6\n%d %d\n255\n", img.width(), img.height());
    for (const Rgb& px : img.pixels()) {
        const auto bytes = to_bytes_gamma2(px);
        std::fwrite(bytes.data(), 1, 3, f);
    }
    std::fclose(f);
    return {};
}

// Read binary PPM (P6, 8-bit). Reverses gamma-2.0 on load so the in-memory
// pixels are in linear space (matches what write_ppm produces).
inline std::expected<Image, std::string>
load_ppm(std::string_view path) noexcept {
    std::string path_str(path);
    std::FILE* f = std::fopen(path_str.c_str(), "rb");
    if (!f) return std::unexpected(std::format("could not open '{}' for reading", path));

    char magic[3]{};
    int  w = 0, h = 0, maxval = 0;
    if (std::fscanf(f, "%2s %d %d %d", magic, &w, &h, &maxval) != 4 ||
        std::strcmp(magic, "P6") != 0 || maxval != 255 || w <= 0 || h <= 0) {
        std::fclose(f);
        return std::unexpected(std::format("bad PPM header in '{}'", path));
    }
    std::fgetc(f);  // exactly one whitespace per PPM convention

    Image img(w, h);
    const int n = w * h;
    for (int i = 0; i < n; ++i) {
        u8 bytes[3];
        if (std::fread(bytes, 1, 3, f) != 3) {
            std::fclose(f);
            return std::unexpected(std::format(
                "PPM '{}': truncated pixel data at {}/{}", path, i, n));
        }
        const f32 r = static_cast<f32>(bytes[0]) / 255.0f;
        const f32 g = static_cast<f32>(bytes[1]) / 255.0f;
        const f32 b = static_cast<f32>(bytes[2]) / 255.0f;
        img.pixels()[i] = Rgb{ r * r, g * g, b * b };
    }
    std::fclose(f);
    return img;
}

// Bilinear sample at (u, v) ∈ [0, 1]. Wrap on u (no seam at sphere meridian),
// clamp on v (no pole-mixing).
inline Rgb sample_bilinear(const Image& img, f32 u, f32 v) noexcept {
    u = u - std::floor(u);
    if (u < 0.0f) u += 1.0f;
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;

    const int w = img.width();
    const int h = img.height();

    const f32 fu = u * static_cast<f32>(w) - 0.5f;
    const f32 fv = v * static_cast<f32>(h) - 0.5f;
    int iu0 = static_cast<int>(std::floor(fu));
    int iv0 = static_cast<int>(std::floor(fv));
    const f32 fx = fu - static_cast<f32>(iu0);
    const f32 fy = fv - static_cast<f32>(iv0);

    int iu1 = iu0 + 1;
    int iv1 = iv0 + 1;

    iu0 = ((iu0 % w) + w) % w;
    iu1 = ((iu1 % w) + w) % w;
    if (iv0 < 0)      iv0 = 0;
    if (iv0 > h - 1)  iv0 = h - 1;
    if (iv1 < 0)      iv1 = 0;
    if (iv1 > h - 1)  iv1 = h - 1;

    const Rgb c00 = img(iu0, iv0);
    const Rgb c10 = img(iu1, iv0);
    const Rgb c01 = img(iu0, iv1);
    const Rgb c11 = img(iu1, iv1);

    const f32 w00 = (1.0f - fx) * (1.0f - fy);
    const f32 w10 = fx          * (1.0f - fy);
    const f32 w01 = (1.0f - fx) * fy;
    const f32 w11 = fx          * fy;

    return Rgb{
        c00.x*w00 + c10.x*w10 + c01.x*w01 + c11.x*w11,
        c00.y*w00 + c10.y*w10 + c01.y*w01 + c11.y*w11,
        c00.z*w00 + c10.z*w10 + c01.z*w01 + c11.z*w11,
    };
}

}  // namespace sotark
