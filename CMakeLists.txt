cmake_minimum_required(VERSION 3.28)
project(Sotark
    VERSION 1.0.0
    DESCRIPTION "Sotark — C++26 port of the C99 software renderers"
    LANGUAGES CXX)

# ─── C++26, modules, strict warnings ─────────────────────────────────
set(CMAKE_CXX_STANDARD          26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default Release if user didn't pick.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# Common compile options for all targets in this project.
add_library(sotark_warnings INTERFACE)
target_compile_options(sotark_warnings INTERFACE
    -Wall -Wextra -Wpedantic
    -Wno-unused-parameter   # legacy code has out-params we don't use yet
    -fdiagnostics-color=always)

add_library(sotark_threads INTERFACE)
find_package(Threads REQUIRED)
target_link_libraries(sotark_threads INTERFACE Threads::Threads)

# ─── Optional SDL2 detection ─────────────────────────────────────────
# sotark_sw library (rendering primitives) does NOT need SDL2.
# Only the `sw_renderer` executable does.
find_package(SDL2 QUIET)
if(NOT SDL2_FOUND)
    # Fallback via pkg-config (Fedora's sdl2-compat-devel installs both).
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(SDL2 IMPORTED_TARGET sdl2)
    endif()
endif()

if(SDL2_FOUND OR (SDL2_LIBRARIES))
    set(SOTARK_HAVE_SDL2 ON)
    message(STATUS "Sotark: SDL2 found — sw_renderer executable will be built.")
else()
    set(SOTARK_HAVE_SDL2 OFF)
    message(STATUS "Sotark: SDL2 NOT found — sw_renderer LIBRARY will still build, "
                   "but the executable will be skipped. "
                   "Install with: sudo dnf install sdl2-compat-devel")
endif()

# ─── Subdirectories ──────────────────────────────────────────────────
add_subdirectory(common)
add_subdirectory(raytracer)
add_subdirectory(sw_renderer)

enable_testing()
add_subdirectory(tests)
