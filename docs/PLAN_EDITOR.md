# Sotark — Plan EDITOR

## Objetivo

Una aplicación interactiva en C99 puro para crear modelos 3D/2D y editarlos
visualmente. Texturas, materiales, luces, geometría — todo manipulable
en tiempo real con UI propia. Path-tracing como modo de "render final".

Diferencia con plan v2: en lugar de seguir mejorando los renderers como
projectos aislados, esto los CONVIERTE en un editor. Los milestones de
v2 (tracks A-D) siguen siendo válidos pero se priorizan por valor al editor.

---

## Arquitectura (decisiones)

1. **Editor = un solo ejecutable `sotark`** (a futuro). Por ahora extendemos
   `sw_renderer` con UX de editor; el `raytracer` queda como "offline
   render" llamado on-demand.

2. **Viewport = sw_renderer extendido**. Real-time rasterizer es el viewport.
   El path tracer se usa para "Render preview" + "Final render" cuando
   el usuario pide quality.

3. **UI library propia** (immediate-mode, estilo dear-imgui pero minimal).
   ~500 líneas. Educativo y bajo overhead. No dependencias externas.

4. **Scene representation = `dynamic`** (heap-allocated, no más `static const`).
   Objetos pueden agregarse/eliminarse en runtime. Cada objeto = transform
   + geometry (mesh/sphere/quad) + material ref.

5. **Persistencia = JSON-ish text format** (legible, diffeable en git).
   Parser propio ~200 líneas.

---

## Fases

### Fase 1 — Viewport interactivo (E1-E5)

| M | Feature | Por qué importa |
|---|---|---|
| **E1** | Cámara orbital con mouse (orbit/pan/zoom) | Sin esto se siente como demo, no app. UX win inmediato. |
| **E2** | Mouse picking (raycast contra BVH) | Click para seleccionar. Indispensable para cualquier edición. |
| **E3** | Selection highlight (outline) | Feedback visual de qué está seleccionado. |
| **E4** | UI library minimal (panel, button, slider, label) | Base para todo lo que sigue. |
| **E5** | HUD overlay (FPS, selected info, scene stats) | Primer UI en pantalla — termina de transformar el viewport en editor. |

### Fase 2 — Scene editing (E6-E8)

| M | Feature | Notas |
|---|---|---|
| **E6** | Dynamic scene (`scene_t` con malloc, add/remove) | Refactor de SCENE_FACES const → estructura mutable. |
| **E7** | Primitive add hotkeys (`1`=cube, `2`=sphere, `3`=plane, `4`=light) | Crear objetos sin recompilar. |
| **E8** | Transform gizmos (move/rotate/scale handles 3D) | Drag handles para transformar objetos. |

### Fase 3 — Material/light editor (E9-E11)

| M | Feature | Notas |
|---|---|---|
| **E9** | Material panel (sliders: albedo R/G/B, roughness, metallic, emission) | Editar parámetros del material seleccionado. |
| **E10** | Color picker widget (HSV + RGB) | Estándar de editor. |
| **E11** | Texture loader (file dialog → assign a slot del material) | Cargar PPMs y asignarlos. |

### Fase 4 — Path-tracing preview (E12-E14)

| M | Feature | Notas |
|---|---|---|
| **E12** | Progressive raytracer (1 spp/frame accumulado) | El raytracer corre en background, suma samples cada frame. Converge ante los ojos. |
| **E13** | Dual viewport: rasterizer (izquierda) + path-trace (derecha) | Editás en el rasterizer, ves el resultado PBR en el otro pane. |
| **E14** | GPU/SIMD acceleration del raytracer | Llegar a ~10 spp/sec interactivos. SIMD ray packets (4-wide) inicialmente; OpenCL si va más allá. |

### Fase 5 — Persistencia (E15-E17)

| M | Feature | Notas |
|---|---|---|
| **E15** | Save/load scene (`.sk` formato propio JSON-ish) | Persistencia básica. |
| **E16** | Export to OBJ + MTL | Compatibilidad con otras tools. |
| **E17** | Render to PNG/JPG (via stb_image_write opcional) | Output usable, no solo PPM. |

### Fase 6 — 2D tools (E18-E20)

| M | Feature | Notas |
|---|---|---|
| **E18** | 2D canvas mode (modo "edit texture") | Pintar directo sobre una textura asignada a un objeto. |
| **E19** | Brush tool con tamaño/dureza configurable | Painting básico. |
| **E20** | UV editing tool (visualizar y mover UVs en un plano) | Pre-req para texture painting útil. |

### Fase 7 — Mesh editing (E21-E25)

| M | Feature | Notas |
|---|---|---|
| **E21** | Edit mode: vertex selection + drag | Mover vertices individuales. |
| **E22** | Edge/face selection mode | Selección de elementos del mesh. |
| **E23** | Extrude operator | El operador más usado en Blender. |
| **E24** | Inset / loop cut | Operadores secundarios. |
| **E25** | Subdivision surface (Catmull-Clark) | Smoothing del mesh. |

---

## MVP (Minimum Viable Editor)

Lo MÍNIMO para sentir que estás usando un editor:

E1 + E2 + E4 + E5 + E6 + E7 + E9 + E10 = **8 milestones**, ~2-3 fines de semana.

Después de esto: tenés un viewer interactivo con scene mutable, panel
de material y color picker. Podés agregar cubos, moverlos (vía teclas
WASD del objeto seleccionado por ahora — gizmos son E8), cambiar sus
materiales en runtime. Es un "Blender mínimo".

---

## Dependencias entre planes

El plan EDITOR consume features del plan v2:

- **E12 progressive raytracer** se beneficia de v2 tracks A4 (Russian
  roulette) y D1 (SIMD BVH).
- **E9 material panel** se beneficia de v2 A2 (GGX microfacet) — más
  parámetros físicos para exponer en el slider.
- **E14 GPU acceleration** = v2 D4.

Sugerencia: hacer milestones del editor en orden, y CUANDO necesitás una
mejora del renderer, atacarla en ese momento.

---

## Recomendación de orden

Para "máximo impacto visual rápido":

1. **E1** (mouse camera) — empezamos AHORA
2. **E2** (picking) — para sentir que tocás objetos
3. **E4** (UI library) — base para todo
4. **E5 + E6** (HUD + dynamic scene) — empezás a editar
5. **E9 + E10** (material panel + color picker) — visual feedback inmediato
6. **E12** (progressive PT) — el momento "wow"

Después: cualquier orden, según ganas.

---

## Notas técnicas

- **UI library**: pattern immediate-mode. Cada frame, `ui_begin()`,
  llamar widgets, `ui_end()`. Estado interno mínimo (hot/active widgets).
- **Mouse picking**: BVH raycast desde camera origin a través del pixel
  bajo el cursor. El raytracer ya tiene `hit_hittable`.
- **Outline selection**: render selected object con un wireframe overlay
  o con stencil-style edge detection en post.
- **Progressive PT**: accumulator float-buffer + sample counter por pixel.
  Mostrar `accum/n_samples` por frame.

---

## Cosas que ya tenemos a favor

- BVH funcionando (M5 raytracer) — base para picking + PT preview.
- Multithread tile-based (M9 raytracer) — base para progressive PT.
- SDL2 framework — eventos mouse/keyboard, fb display.
- Span buffer + lightmaps (M7-M8 sw_renderer) — viewport ya es competente.
- OBJ loader (M11 raytracer) — import básico ya hecho.

El editor reutiliza casi todo. La capa nueva es UI + scene mutable.
