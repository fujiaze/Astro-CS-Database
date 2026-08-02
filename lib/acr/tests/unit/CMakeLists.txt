# lib/acr/tests/unit/CMakeLists.txt — 公共 API 单测
add_executable(acr_test_api test_api.cpp)
target_link_libraries(acr_test_api PRIVATE acr_api GTest::gtest_main)

add_executable(acr_test_buffer test_buffer.cpp)
target_link_libraries(acr_test_buffer PRIVATE acr_api GTest::gtest_main)

# Phase C：硬件发现/ISA 单测
add_executable(acr_test_topology test_topology.cpp)
target_link_libraries(acr_test_topology PRIVATE
    acr_topology acr_cpu_isa acr_diagnostics GTest::gtest_main)
target_include_directories(acr_test_topology PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../backends/cpu
)

include(GoogleTest)
# DISCOVERY_MODE PRE_TEST：ctest 运行时才发现测试，避免 build 时 hwloc.dll 等
# 运行时依赖不在 PATH 导致 discover 失败（MinGW + MSYS2 系统库场景）
gtest_discover_tests(acr_test_api     DISCOVERY_MODE PRE_TEST)
gtest_discover_tests(acr_test_buffer  DISCOVERY_MODE PRE_TEST)
gtest_discover_tests(acr_test_topology DISCOVERY_MODE PRE_TEST)
