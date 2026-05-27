add_library(sotark_sw)

target_sources(sotark_sw
    PUBLIC FILE_SET CXX_MODULES FILES
        src/sotark.sw.cppm
        src/sotark.sw-framebuffer.cppm
        src/sotark.sw-draw_line.cppm
        src/sotark.sw-draw_tri.cppm
        src/sotark.sw-texture.cppm
        src/sotark.sw-frustum.cppm
        src/sotark.sw-bsp.cppm
        src/sotark.sw-span_buffer.cppm
        src/sotark.sw-rast_scan.cppm
        src/sotark.sw-lightmap.cppm
        src/sotark.sw-scene.cppm
        src/sotark.sw-text.cppm
        src/sotark.sw-ui.cppm)

target_link_libraries(sotark_sw
    PUBLIC  sotark_common
    PRIVATE sotark_warnings)

# ─── Executable: only if SDL2 was found at configure time ────────────
if(SOTARK_HAVE_SDL2)
    add_executable(sw_renderer src/main.cpp)
    target_link_libraries(sw_renderer
        PRIVATE sotark_sw sotark_common sotark_threads sotark_warnings)
    if(TARGET SDL2::SDL2)
        target_link_libraries(sw_renderer PRIVATE SDL2::SDL2)
    elseif(TARGET PkgConfig::SDL2)
        target_link_libraries(sw_renderer PRIVATE PkgConfig::SDL2)
    else()
        target_include_directories(sw_renderer PRIVATE ${SDL2_INCLUDE_DIRS})
        target_link_libraries(sw_renderer PRIVATE ${SDL2_LIBRARIES})
    endif()
endif()
