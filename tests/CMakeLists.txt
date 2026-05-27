add_executable(sotark_tests
    test_vec.cpp
    test_mat.cpp
    test_rng.cpp
    test_aabb.cpp
    test_plane.cpp
    test_image.cpp
    test_bvh.cpp
    test_main.cpp)

target_include_directories(sotark_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party)

target_link_libraries(sotark_tests
    PRIVATE sotark_common sotark_rt sotark_warnings)

add_test(NAME sotark_tests COMMAND sotark_tests)
