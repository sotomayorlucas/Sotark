module;

#include <print>
#include <format>
#include <source_location>
#include <string_view>
#include <cstdlib>
#include <utility>

export module sotark.common:log;

export namespace sotark {

template <typename... Args>
void log_info(std::format_string<Args...> fmt, Args&&... args) {
    std::print("[info]  ");
    std::println(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void log_warn(std::format_string<Args...> fmt, Args&&... args) {
    std::print(stderr, "[warn]  ");
    std::println(stderr, fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void log_error(std::format_string<Args...> fmt, Args&&... args) {
    std::print(stderr, "[error] ");
    std::println(stderr, fmt, std::forward<Args>(args)...);
}

// PANIC: prints location + message and aborts. Designed so that the call
// site doesn't need to write the source_location explicitly — it's captured
// at the macro-free call point via a default argument.
struct PanicLocation {
    std::source_location loc;
    consteval PanicLocation(std::source_location l = std::source_location::current())
        : loc(l) {}
};

[[noreturn]] inline void panic_impl(std::string_view msg, std::source_location loc) {
    std::println(stderr, "[PANIC] {}:{}: {}", loc.file_name(), loc.line(), msg);
    std::abort();
}

[[noreturn]] inline void panic(std::string_view msg,
                                PanicLocation pl = {}) {
    panic_impl(msg, pl.loc);
}

template <typename... Args>
[[noreturn]] void panic_fmt(PanicLocation pl, std::format_string<Args...> fmt, Args&&... args) {
    panic_impl(std::format(fmt, std::forward<Args>(args)...), pl.loc);
}

inline void check(bool cond, std::string_view msg,
                  PanicLocation pl = {}) {
    if (!cond) [[unlikely]] panic_impl(msg, pl.loc);
}

}  // namespace sotark
