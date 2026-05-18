# Sotark

Two software renderers from scratch in C99 — building up the graphics canon
the way Abrash, Carmack, and Shirley did: line by line, no GPU, no shaders.

- **`sw_renderer/`** — Quake-era pipeline: BSP scenes, perspective-correct
  texture mapping with Abrash's subspan FDIV trick, static lightmaps,
  span buffer for zero overdraw, multithreaded scanline rendering.
  SDL2 only for opening the window + blitting a CPU framebuffer.

- **`raytracer/`** — Peter Shirley's *Ray Tracing in One Weekend* and *The
  Next Week* in C99: BVH, lambertian/metal/dielectric materials, NEE
  importance sampling for Cornell box, triangle meshes via OBJ loader,
  image textures with spherical UV, pthread tile-based parallelism.

- **`common/`** — shared math (`vec3`, `mat4`, `plane`, `aabb`), PCG32 RNG,
  PPM image I/O.

Pure C99, hand-written `Makefile`s, zero runtime dependencies beyond libc
and (for the SW renderer) SDL2. No CMake, no helper libraries.

## Build

```sh
sudo dnf install SDL2-devel      # Fedora; on Debian/Ubuntu: libsdl2-dev
make
```

Targets: `make common`, `make sw_renderer`, `make raytracer`, `make clean`.

## Run

```sh
# SW renderer: real-time, WASD + arrows. Argument = thread count (default auto).
./sw_renderer/sw_renderer
./sw_renderer/sw_renderer 8

# Raytracer:
./raytracer/raytracer out.ppm <scene> [spp] [depth] [seed] [threads] [aux_path]
./raytracer/raytracer cover.ppm   cover   100 50 42 16
./raytracer/raytracer cornell.ppm cornell 500 50 42 16   # NEE makes this fast
./raytracer/raytracer mesh.ppm    mesh    50  50 42 16
./raytracer/raytracer obj.ppm     obj     50  50 42 16   # model.obj icosphere
./raytracer/raytracer earth.ppm   earth   100 50 42 16   # image-textured sphere
```

Open the resulting `.ppm` with any image viewer (e.g. `xdg-open`, `feh`).

## What's in the box

Eleven milestones built incrementally, each fully working before moving on:

| M | sw_renderer | raytracer |
|---|---|---|
| 0  | SDL window + animated framebuffer       | PPM gradient (Shirley §2)              |
| 1  | wireframe cube (Bresenham)              | sphere with normal-as-color            |
| 2  | solid triangles + z-buffer              | hittable list + AA + gamma             |
| 3  | affine texture mapping (swimming demo)  | lambertian / metal / dielectric        |
| 4  | perspective-correct texture mapping     | defocus blur + RTIOW cover scene       |
| 5  | frustum + backface culling              | BVH (12× speedup)                      |
| 6  | BSP scene + WASD camera + textures      | HIT_QUAD + Cornell box                 |
| 7  | span buffer (zero overdraw)             | NEE importance sampling                |
| 8  | static baked lightmaps                  | HIT_TRI + procedural icosphere mesh    |
| 9  | subspan FDIV (Abrash's trick)           | pthread tile-based rendering           |
| 10 | pthread per-stripe + near-plane clip    | (covered by M9)                        |
| 11 | —                                       | OBJ loader + image textures (PPM)      |

References: Michael Abrash *Graphics Programming Black Book* (chapters 59-70),
Peter Shirley *Ray Tracing in One Weekend* / *The Next Week* / *The Rest of
Your Life*, the Quake-1 source.
