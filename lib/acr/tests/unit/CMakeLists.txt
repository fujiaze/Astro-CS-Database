# lib/acr/tests/unit/CMakeLists.txt — 公共 API 单测
add_executable(acr_test_api test_api.cpp)
target_link_libraries(acr_test_api PRIVATE acr_api GTest::gtest_main)

add_executable(acr_test_buffer test_buffer.cpp)
target_link_libraries(acr_test_buffer PRIVATE acr_api GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(acr_test_api)
gtest_discover_tests(acr_test_buffer)
