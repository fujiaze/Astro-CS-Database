# lib/acr/tests/integration/CMakeLists.txt
#
# ACR 架构冻结（07 号计划 F）：加权积分合成 Mixed 样例集成测试。
add_executable(acr_test_weighted_integration test_weighted_integration.cpp)
target_link_libraries(acr_test_weighted_integration PRIVATE
    acr_weighted_integration_example
    acr_scheduler
    acr_qualification
    acr_cost
    acr_api
    acr_cuda_bridge_loader
    GTest::gtest_main
)
target_include_directories(acr_test_weighted_integration PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../examples/weighted_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../../scheduler
    ${CMAKE_CURRENT_SOURCE_DIR}/../../qualification
    ${CMAKE_CURRENT_SOURCE_DIR}/../../core
    ${CMAKE_CURRENT_SOURCE_DIR}/../../cost
    ${CMAKE_CURRENT_SOURCE_DIR}/../../backends/cuda
)
gtest_discover_tests(acr_test_weighted_integration
    DISCOVERY_MODE PRE_TEST
    PROPERTIES RUN_SERIAL TRUE  # 真实 GPU：串行避免竞争
)
