add_library(sotark_rt)

target_sources(sotark_rt
    PUBLIC FILE_SET CXX_MODULES FILES
        src/sotark.rt.cppm
        src/sotark.rt-ray.cppm
        src/sotark.rt-hit.cppm
        src/sotark.rt-sampling.cppm
        src/sotark.rt-hittable.cppm
        src/sotark.rt-material.cppm
        src/sotark.rt-bvh.cppm
        src/sotark.rt-camera.cppm
        src/sotark.rt-obj.cppm)

target_link_libraries(sotark_rt
    PUBLIC  sotark_common
    PRIVATE sotark_warnings)

add_executable(raytracer src/main.cpp)
target_link_libraries(raytracer
    PRIVATE sotark_rt sotark_common sotark_threads sotark_warnings)
