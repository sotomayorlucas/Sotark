add_library(sotark_common)

target_sources(sotark_common
    PUBLIC FILE_SET CXX_MODULES FILES
        src/sotark.common.cppm
        src/sotark.common-types.cppm
        src/sotark.common-util.cppm
        src/sotark.common-log.cppm
        src/sotark.common-rng.cppm
        src/sotark.common-vec.cppm
        src/sotark.common-mat.cppm
        src/sotark.common-aabb.cppm
        src/sotark.common-plane.cppm
        src/sotark.common-color.cppm
        src/sotark.common-image.cppm)

target_link_libraries(sotark_common
    PRIVATE sotark_warnings)
