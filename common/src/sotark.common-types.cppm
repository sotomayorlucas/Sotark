module;

#include <cstdint>
#include <cstddef>

export module sotark.common:types;

export namespace sotark {

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i8  = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;
using f32 = float;
using f64 = double;
using usize = std::size_t;

using fixed16_16 = i32;

consteval fixed16_16 fixed_from_int(i32 x)    { return static_cast<fixed16_16>(x << 16); }
constexpr fixed16_16 fixed_from_float(f32 x)  { return static_cast<fixed16_16>(x * 65536.0f); }
constexpr i32        fixed_to_int(fixed16_16 x) { return static_cast<i32>(x >> 16); }
constexpr f32        fixed_to_float(fixed16_16 x) { return static_cast<f32>(x) / 65536.0f; }

}  // namespace sotark
