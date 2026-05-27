---
name: cxx26-port-design
date: 2026-05-26
status: approved (Approach A — Idiomatic Maximalist)
---

# Sotark C99 → C++26 Idiomatic Port

## Goal

Port the entire Sotark codebase (4305 LOC of C99 across `common/`, `raytracer/`,
`sw_renderer/`) to **C++26** on branch `cxx26`. Use stdlib C++26 features
maximally; `main` stays as the C99 reference.

## Constraints (decided in brainstorming)

| Eje              | Decisión                                                         |
| ---              | ---                                                              |
| Branch           | `cxx26` in `/home/lkz/Sotark/` (main = C99 reference)            |
| Scope            | Big-bang all 3 sub-projects: `common` + `raytracer` + `sw_renderer` |
| Toolchain        | GCC 16.1.1, `-std=c++26`, real C++20+ modules                    |
| Build            | CMake 4.3 + Ninja (P1689 module dependency scanning)             |
| External deps    | SDL2 (sw_renderer only) + doctest (header-only). Everything else stdlib. |
| Validation       | Functional equivalence + invariants (no bit-exact requirement)   |
| Philosophy       | Idiomatic rewrite, **not** mechanical translation                |

## C99 → C++26 mapping

| Pieza C99                          | Reemplazo C++26                                                            |
| ---                                | ---                                                                        |
| `#include "X.h"` headers           | `import sotark.common:vec;` C++20+ module partitions                       |
| `tag enum + union` (hittable, material) | `std::variant<Sphere, List, Bvh, Quad, Tri>` + `std::visit`           |
| `void*` callbacks / fn pointers    | `concept Hittable`, `concept Material`, `std::function<u32(f32,f32)>`      |
| `pthread` pool (raytracer)         | `std::execution` (P2300 senders/receivers, GCC 16) **— stretch**; fallback `std::jthread + std::barrier` |
| `pthread` pool (sw_renderer)       | `std::jthread + std::barrier + std::stop_token`                            |
| `int return + out param`           | `std::expected<T, std::string>`                                            |
| `float color[H][W]`                | `std::mdspan<float, std::dextents<size_t, 2>>`                             |
| BVH/BSP recursion + callback       | `std::generator<Hit>` coroutines (lazy traversal)                          |
| `printf` / `fprintf`               | `std::print` / `std::println` / `std::format`                              |
| `abort()` PANIC                    | `std::stacktrace` + `std::source_location` + throw / abort                 |
| `malloc/free` / `static T[N]`      | `std::unique_ptr<T[]>`, `std::vector<T>`, `std::pmr::monotonic_buffer_resource` (arenas) |
| Macros `MIN/MAX/CLAMP`             | `std::min`, `std::max`, `std::clamp`                                       |
| Constantes (`3.14159265...`)       | `std::numbers::pi_v<float>`                                                |
| PCG32 RNG                          | Class conforming to `std::uniform_random_bit_generator` concept            |
| Raw loops over pixels              | `std::ranges::views::cartesian_product` + algorithms                       |
| `static int g_sort_axis` (bvh)     | Lambda capture for `std::ranges::sort` projection                          |
| `typedef struct`                   | `struct` (no typedef needed)                                               |
| `f32`, `u32` typedefs              | Keep as aliases in `sotark.common:types` for parity with formulae          |

## Module structure

```
sotark.common (library)
  :types  :util  :log  :rng  :vec  :mat  :aabb  :plane  :color  :image

sotark.rt (library)
  :ray  :hit  :sampling  :hittable  :material  :bvh  :camera  :obj  :scenes

sotark.sw (library, requires SDL2)
  :framebuffer  :draw_line  :draw_tri  :texture  :frustum
  :bsp  :span_buffer  :rast_scan  :lightmap  :scene  :text  :ui
```

Executables:

* `raytracer` — `main.cpp` imports `sotark.common`, `sotark.rt`. Tile-based
  multithread render via `std::jthread + atomic counter` (or `std::execution`).
* `sw_renderer` — `main.cpp` imports `sotark.common`, `sotark.sw`. SDL2 window;
  per-stripe `std::jthread + std::barrier`.

## Build layout

```
Sotark/
  CMakeLists.txt              # top-level, cxx_std 26
  cmake/CMakePresets.json
  third_party/doctest.h       # vendored single-header
  common/
    CMakeLists.txt
    src/                       # .cppm module units
  raytracer/
    CMakeLists.txt
    src/
  sw_renderer/
    CMakeLists.txt
    src/
  tests/
    CMakeLists.txt
    *.cpp                      # doctest
```

## Validation strategy

1. **Build:** `cmake --build build` succeeds for all 3 sub-projects.
2. **Unit tests:** doctest suite for `vec`/`mat`/`aabb`/`plane`/`rng` (PCG32
   determinism — given seed produces known bytes)/`bvh` invariants.
3. **Smoke test raytracer:** render a tiny scene (e.g., `mesh 5 5 42 1` →
   400×225 PPM in <1s), verify the output PPM is parseable (header valid,
   correct pixel count) and not all-black.
4. **Smoke test sw_renderer:** verify it links + reports `SDL_Init` would
   succeed; can't run windowed without display, so accept compile-only as
   pass.

## Out of scope

* Bit-exact reproduction of master C99 (we accept FP reorder for SIMD/parallel).
* Porting tests/CI infrastructure that doesn't exist in master.
* Adding new features beyond C99 parity.
* `std::execution` (P2300) is **stretch** — if GCC 16 stdlib has bugs,
  fall back to `std::jthread + std::atomic<int>` for the raytracer pool.
* Reflection (P2996) and Contracts (P2900) — partial in GCC 16, skip for
  correctness margin.
