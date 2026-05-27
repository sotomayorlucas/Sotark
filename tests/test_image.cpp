#include "doctest.h"
#include <cstdio>
#include <filesystem>

import sotark.common;

using namespace sotark;

TEST_CASE("Image init and indexing") {
    Image img(4, 3);
    CHECK(img.width() == 4);
    CHECK(img.height() == 3);
    img(2, 1) = Rgb{0.5f, 0.25f, 1.0f};
    CHECK(img(2, 1) == Rgb{0.5f, 0.25f, 1.0f});
}

TEST_CASE("Image PPM round-trip") {
    Image src(8, 4);
    for (int y = 0; y < src.height(); ++y) {
        for (int x = 0; x < src.width(); ++x) {
            const f32 r = static_cast<f32>(x) / 8.0f;
            const f32 g = static_cast<f32>(y) / 4.0f;
            src(x, y) = Rgb{r * r, g * g, 0.5f * 0.5f};  // pre-gamma to be invariant
        }
    }
    const std::string path = (std::filesystem::temp_directory_path() / "sotark_test.ppm").string();
    auto w = write_ppm(src, path);
    REQUIRE(w);
    auto r = load_ppm(path);
    REQUIRE(r);
    Image dst = std::move(*r);
    CHECK(dst.width()  == src.width());
    CHECK(dst.height() == src.height());
    // PPM is lossy (8-bit + gamma), so allow small epsilon.
    for (int y = 0; y < src.height(); ++y) {
        for (int x = 0; x < src.width(); ++x) {
            CHECK(dst(x, y).x == doctest::Approx(src(x, y).x).epsilon(0.02f));
            CHECK(dst(x, y).y == doctest::Approx(src(x, y).y).epsilon(0.02f));
            CHECK(dst(x, y).z == doctest::Approx(src(x, y).z).epsilon(0.02f));
        }
    }
    std::remove(path.c_str());
}

TEST_CASE("Image sample_bilinear: solid color preserved") {
    Image img(4, 4);
    for (Rgb& p : img.pixels()) p = Rgb{0.3f, 0.6f, 0.9f};
    const Rgb s = sample_bilinear(img, 0.5f, 0.5f);
    CHECK(s.x == doctest::Approx(0.3f));
    CHECK(s.y == doctest::Approx(0.6f));
    CHECK(s.z == doctest::Approx(0.9f));
}
