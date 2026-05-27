module;

#include <limits>

export module sotark.common:rng;

import :types;

export namespace sotark {

// PCG32 — Permuted Congruential Generator, 32-bit output.
// Reference: https://www.pcg-random.org/
// Models std::uniform_random_bit_generator.
class Pcg32 {
public:
    using result_type = u32;

    static constexpr result_type min() noexcept { return 0u; }
    static constexpr result_type max() noexcept { return std::numeric_limits<u32>::max(); }

    constexpr Pcg32() noexcept : state_{0}, inc_{0} {
        seed(0u, 0u);
    }

    constexpr Pcg32(u64 init_state, u64 init_seq) noexcept
        : state_{0}, inc_{0} {
        seed(init_state, init_seq);
    }

    constexpr void seed(u64 init_state, u64 init_seq) noexcept {
        state_ = 0u;
        inc_   = (init_seq << 1u) | 1u;   // inc must be odd
        (void)next();
        state_ += init_state;
        (void)next();
    }

    constexpr result_type next() noexcept {
        const u64 old = state_;
        state_ = old * 6364136223846793005ull + inc_;
        const u32 xorshifted = static_cast<u32>(((old >> 18u) ^ old) >> 27u);
        const u32 rot        = static_cast<u32>(old >> 59u);
        return (xorshifted >> rot) | (xorshifted << ((-rot) & 31u));
    }

    constexpr result_type operator()() noexcept { return next(); }

    // Uniform float in [0, 1), 24 bits of precision (top of u32).
    constexpr f32 float01() noexcept {
        return static_cast<f32>(next() >> 8) * (1.0f / 16777216.0f);
    }

private:
    u64 state_;
    u64 inc_;
};

}  // namespace sotark
