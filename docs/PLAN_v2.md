# Sotark — Plan v2 (M11+)

## Contexto

El plan v1 (M0..M10/M11) está completo: ambos renderers funcionan de punta a
punta y son técnicamente "completos" para su categoría — el sw_renderer
es feature-comparable a Quake-1 software path; el raytracer corresponde a
Shirley book 1-2 con NEE de book 3.

Plan v2: tres tracks paralelos, NO secuenciales. Pická el que más te tira
en cada sesión. Cada milestone debería ser un fin de semana o menos.

---

## Track A — Raytracer rumbo a PBR

**Estado actual**: BVH, NEE, OBJ loader, image textures lambertian, threading.

| M | Feature | Por qué importa | Costo |
|---|---|---|---|
| **A1** | OBJ `vt` UV + MTL parser | Texturas por triángulo + texturas por material desde archivo (en vez de hardcoded) | bajo |
| **A2** | Microfacet GGX (BRDF Cook-Torrance) | Metales realistas con roughness, fresnel correcto. Pasaje de "looks plastic" a "looks PBR" | medio |
| **A3** | HDR env map sampling | Image-based lighting: cargar un `.hdr` equirectangular y muestrear como sky. Mata el "sky lerp" artificial. | medio |
| **A4** | Russian roulette + adaptive sampling | Convergencia más rápida en regiones complejas; término probabilístico para paths largos. | bajo |
| **A5** | Multiple Importance Sampling (MIS) | Reemplaza el NEE biased actual por la versión unbiased de Veach. Cornell sin firefly pixels. | alto |
| **A6** | Motion blur (ray.time) | Cámara con shutter time + animaciones de spheres. Imagen visualmente única. | bajo |
| **A7** | Volumes (homogeneous + heterogeneous) | Smoke, fog, fire. Cornell box con neblina. | medio |
| **A8** | BVH SAH (Surface Area Heuristic) | Reemplaza median-split por SAH para BVH mejores en escenas no uniformes. ~2× speedup en meshes irregulares. | medio |

**Imagen target de cierre del track**: Stanford dragon (~870k tris) iluminado
por HDR map, materiales mixtos (gold metal, glass, lambertian textured),
con motion blur en una esfera, en una Cornell modificada con humo.

**Recomendación de orden**: A1 → A2 → A3 → A4 → A5 (resto opcional).

---

## Track B — SW renderer rumbo a Quake feature-parity

**Estado actual**: BSP minimal (1 leaf), span buffer, lightmaps estáticos,
multithread, near-plane clipping, perspective-correct texture mapping.

| M | Feature | Por qué importa | Costo |
|---|---|---|---|
| **B1** | Real BSP tree (programmatic multi-leaf) | El "BSP" actual es 1 leaf trivial. Construir un BSP real con planos de split y múltiples leaves muestra el algoritmo posta. | medio |
| **B2** | PVS (Potentially Visible Set) | Por cada leaf, calcular qué leafs son visibles. Cull masivo en runtime — base del rendimiento de Quake. | alto |
| **B3** | Quake `.bsp` v29 loader | Parsear el formato binario real. Cargar mapas de Quake-1 (`e1m1.bsp` etc). Requiere id1/ pak files o un .bsp standalone. | alto |
| **B4** | Surface caching (Abrash's secret) | Quake bakea base_tex × lightmap a una sub-textura ad-hoc por surface, cache LRU. Hace texturizado un memcpy en lugar de modulado por pixel. ~2× FPS. | medio |
| **B5** | Mipmaps con LOD selection | Por surface, elegir mip level según área proyectada. Anti-aliasing del texturizado lejano. | medio |
| **B6** | MD3 / MDL animated models | Modelos animados (frame-blended) estilo Quake-1/2. Entities en la escena. | medio |
| **B7** | Particle system | Sparks, smoke, dust. Billboards en screen space con span buffer y blending. | bajo |
| **B8** | SIMD inner loop (AVX2) | Vectorizar `draw_span_perspective` a 8 pixels a la vez. Texture sampler especializado por textura. | alto |
| **B9** | Dynamic lights (lightstyles) | Light flicker + adding/subtracting dynamic contribution sobre el lightmap base. | medio |
| **B10** | Sky cube / dome | Cielo no-trivial. Skybox 6-faces o cilíndrico tipo Quake-2. | bajo |

**Imagen target**: cargar `e1m1.bsp` de Quake-1 y caminar por adentro a
60 FPS con lightmaps, surface caching, mipmaps y un MD3 walking around.

**Recomendación de orden**: B4 (surface caching) primero — beneficio gigante
para el costo. Después B1+B2 si querés meterse en el rendering de mapas
reales. B3 (loader) puede esperar — el formato es complejo y los mapas
necesitan id1/ pak files (legal grey area).

---

## Track C — Proyectos nuevos (greenfield)

Si Sotark te aburre, estos son starting points para algo nuevo, aprovechando
el `common/` que ya tenemos:

### C1 — SDF Raymarcher (Inigo Quilez-style)

Renderear escenas definidas por **signed distance functions** en lugar de
geometría explícita. Sphere tracing en cada pixel.

- Primitives: sphere, box, torus, cilindro, plano (cada uno = función f(p))
- Operadores: union (min), intersection (max), subtract (max(a, -b))
- Smooth-min para blending suave
- Repetition: `f(mod(p, period) - period/2)` para escenas infinitas
- Soft shadows + AO via accumulated distance
- Materiales por SDF (lambertian, mirror)

**Resultado**: imágenes "Inigo Quilez en C99". Cero meshes, geometría matemática
pura. Excelente intro a CSG.

**Costo**: bajo-medio. Un archivo ~500 líneas. shadertoy.com tiene mucho material.

### C2 — Voxel DDA raymarcher

Render de un mundo voxelizado tipo Minecraft via **DDA traversal** (Amanatides
& Woo). Cada rayo camina la grilla de voxels por celda hasta hit.

- Mundo: `u8 voxels[N×N×N]` (e.g., 256³ = 16MB)
- Generación: simplex noise para terreno, gen. árboles, cuevas con erosión
- DDA: stepping con tDeltaX/Y/Z y tMaxX/Y/Z, elegir min, avanzar
- Material: per-voxel color o textura (tiled)
- Cube faces texturizados (grass top, dirt sides)

**Resultado**: caminar por Minecraft renderizado en CPU. Con multithreading
+ chunks, 30 FPS+ en 800×600 es factible.

**Costo**: medio. DDA es sutil de implementar bien.

### C3 — Software OpenGL ES 1.0 subset

Implementar un SUBSET de OpenGL ES 1.x como library (`libsoftgl.a`) que
expone `glVertex3f`, `glColor4f`, `glDrawArrays`, etc., compatible binario
con OpenGL ES 1.0. Backend: el sw_renderer.

- API: GL_TRIANGLES, GL_LINES, ortho/perspective matrices
- State: matrix stack, current color, current texture
- Pipeline: vertex transform → clip → raster (reuse rast_scan)
- Texturas: bound texture object, glTexImage2D

**Resultado**: un .so/.a que código existente OpenGL puede linkear contra
y "correr en CPU". Drop-in para sistemas sin GPU.

**Costo**: alto. OpenGL es complejo aún en subset.

### C4 — 2D vector rasterizer (SVG-like)

Stroke + fill de paths de Bézier en CPU. Anti-aliasing via supersampling
o analytical coverage.

- Path: secuencia de moveTo/lineTo/quadTo/cubicTo
- Fill: nonzero or even-odd winding
- Stroke: outline a width via parallel curves + caps/joins
- AA: 4x4 supersample por pixel, o un proper analytical algorithm

**Resultado**: SVG renderer en C99. Útil como base para un UI minimalista.

**Costo**: medio.

---

## Track D — Eternal optimization (cualquier momento)

- **D1**: SIMD ray-vs-AABB en el raytracer BVH (4-wide test). Speedup 2-3× en BVH-bound scenes.
- **D2**: Cache-aware BVH layout (depth-first linear, no random pointers). Speedup 1.5-2×.
- **D3**: Wide BVH (4 children por nodo en lugar de 2). Mejor SIMD utilization.
- **D4**: GPU compute via OpenCL — port del raytracer a kernel OpenCL. Speedup 50-100×.
- **D5**: Profile-guided refactor — `perf record` ambos renderers, optimizar hot path.

---

## Cosas que descarté del plan original (con razón)

- **OBJ MTL** parser fue diferido en M11 — ahora es A1.
- **`stb_image.h`** para PNG/JPG textures — opcionalmente lo podés agregar
  cuando A1 esté listo. Si no querés depender de stb, PPM como hicimos es
  suficiente para el demo.
- **GPU rendering** — totalmente fuera de scope del plan, pero un Vulkan
  port del raytracer es un proyecto razonable después de M-completar.

---

## Recomendación personal de orden

Si querés "máximo impacto visual por unidad de tiempo":

1. **A1** (OBJ MTL + triangle UV) — destrabar real Stanford bunny con texturas.
2. **A3** (HDR env map) — IBL hace que CUALQUIER scene se vea bien instantáneamente.
3. **A2** (GGX microfacet) — completar la sensación PBR.
4. **B4** (surface caching) — duplica el FPS del sw_renderer.
5. **C1** (SDF raymarcher) — proyecto nuevo, cambio de aire.

Si querés "más fidelidad histórica a Quake":

1. **B1** (real BSP) → **B2** (PVS) → **B3** (.bsp loader) → caminar e1m1.

Si querés "más fidelidad a path tracing puro":

1. **A5** (MIS) → **A8** (BVH SAH) → **A7** (volumes) → Cornell con humo.

---

## Referencias adicionales

Para el track A:
- Heitz 2014 — "Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs" (GGX masking)
- Karis 2013 — "Real Shading in Unreal Engine 4" (PBR formulas resumidas)
- Veach 1997 thesis — capítulo 9 sobre MIS

Para el track B:
- Carmack's `.plan` files (1996-1999) — Quake engine evolution
- Abrash Black Book cap. 67-70 — surface caching, dynamic light, particles
- Fabien Sanglard "Quake Engine Code Review" — fabiensanglard.net/quakeSource

Para el track C1:
- iquilezles.org/articles — Inigo Quilez's writeups, definitive resource for SDF
- ShaderToy — busca por "raymarch" para inspiración + algorithms

Para el track C2:
- Amanatides & Woo 1987 — "A Fast Voxel Traversal Algorithm for Ray Tracing"
- John Lin's "DDA Voxel Traversal" tutorial

Para el track D:
- Agner Fog's optimization manuals (agner.org/optimize)
- "Cache-Oblivious Algorithms" — Frigo et al. para layout BVH
