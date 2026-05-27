module;

#include <vector>
#include <functional>

export module sotark.sw:span_buffer;

import sotark.common;

export namespace sotark::sw {

// Callback that draws a run of NOT-yet-covered pixels.
using SpanDrawFn = std::function<void(int y, int x0, int x1)>;

// Per-pixel covered bitmap (1 byte/pixel). For each emitted span we find the
// runs still uncovered, call draw_fn for those, and mark them. Guarantees
// each pixel is shaded at most once per frame (zero overdraw) as long as
// polygons are processed front-to-back.
class SpanBuffer {
public:
    SpanBuffer() = default;
    SpanBuffer(int w, int h)
        : covered_(static_cast<usize>(w) * static_cast<usize>(h), 0u),
          w_{w}, h_{h} {}

    void clear() noexcept {
        std::fill(covered_.begin(), covered_.end(), u8{0});
        pixels_drawn_   = 0;
        pixels_skipped_ = 0;
    }

    void emit(int y, int x0, int x1, const SpanDrawFn& fn) {
        if (y < 0 || y >= h_) return;
        if (x0 < 0)   x0 = 0;
        if (x1 > w_)  x1 = w_;
        if (x0 >= x1) return;

        u8* row = covered_.data() + static_cast<usize>(y) * w_;
        int run_start = -1;
        for (int x = x0; x < x1; ++x) {
            if (!row[x]) {
                if (run_start < 0) run_start = x;
                row[x] = 1;
            } else {
                ++pixels_skipped_;
                if (run_start >= 0) {
                    fn(y, run_start, x);
                    pixels_drawn_ += x - run_start;
                    run_start = -1;
                }
            }
        }
        if (run_start >= 0) {
            fn(y, run_start, x1);
            pixels_drawn_ += x1 - run_start;
        }
    }

    int pixels_drawn()   const noexcept { return pixels_drawn_; }
    int pixels_skipped() const noexcept { return pixels_skipped_; }
    int width()          const noexcept { return w_; }
    int height()         const noexcept { return h_; }

private:
    std::vector<u8> covered_;
    int w_{0}, h_{0};
    int pixels_drawn_{0};
    int pixels_skipped_{0};
};

}  // namespace sotark::sw
