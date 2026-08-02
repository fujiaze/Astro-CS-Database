# lib/acr/tests/fault/CMakeLists.txt — Phase H 故障注入 / sanitizer / 持续路线
if(NOT ACR_BUILD_TESTS)
    return()
endif()

# fault_injection：故障注入测试
add_executable(acr_test_fault fault_injection.cpp)
target_link_libraries(acr_test_fault PRIVATE
    acr_api
    acr_routing
    acr_scheduler
    GTest::gtest
)
target_include_directories(acr_test_fault PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../routing
    ${CMAKE_CURRENT_SOURCE_DIR}/../../scheduler
)
gtest_discover_tests(acr_test_fault DISCOVERY_MODE PRE_TEST)

# sanitizer_smoke：sanitizer 烟雾测试（ASan/UBSan/TSan 下应通过）
add_executable(acr_test_sanitizer sanitizer_smoke.cpp)
target_link_libraries(acr_test_sanitizer PRIVATE
    acr_api
    GTest::gtest
)
gtest_discover_tests(acr_test_sanitizer DISCOVERY_MODE PRE_TEST)

# persistence：持续路线测试（默认 5 秒，ACR_PERSIST_DURATION_SEC 可配置）
add_executable(acr_test_persistence persistence.cpp)
target_link_libraries(acr_test_persistence PRIVATE
    acr_api
    acr_routing
    acr_qualification
    GTest::gtest
)
target_include_directories(acr_test_persistence PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../routing
    ${CMAKE_CURRENT_SOURCE_DIR}/../../qualification
)
gtest_discover_tests(acr_test_persistence DISCOVERY_MODE PRE_TEST)

message(STATUS "ACR fault: fault_injection + sanitizer_smoke + persistence")
