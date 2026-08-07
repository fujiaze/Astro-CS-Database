# lib/acr/tests/fault/CMakeLists.txt — Phase H 故障注入 / sanitizer / 持续路线
# Commit D（20_PHASE_I_AUDIT_ACTION_PLAN.md §6 重命名）：
#   sanitizer_smoke.cpp → lifecycle_smoke.cpp（未开 sanitizer flag，仅功能烟雾测试）
#   新增 ACR_BUILD_SANITIZER option：开启后 sanitizer_actual.cpp 编译为真实 sanitizer 验证
if(NOT ACR_BUILD_TESTS)
    return()
endif()

# ACR_BUILD_SANITIZER option（默认 OFF，MinGW 工具链可能不支持 ASan）
# 开启时给 sanitizer_actual target 加 -fsanitize=address,undefined flag，
# 构建日志必须证明 sanitizer 实际开启（20_PHASE_I_AUDIT_ACTION_PLAN.md §7）。
option(ACR_BUILD_SANITIZER "Enable AddressSanitizer/UBSan for sanitizer_actual tests" OFF)

# fault_injection：故障注入测试
add_executable(acr_test_fault fault_injection.cpp)
target_link_libraries(acr_test_fault PRIVATE
    acr_api
    acr_scheduler
    GTest::gtest
)
target_include_directories(acr_test_fault PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../scheduler
)
gtest_discover_tests(acr_test_fault
    DISCOVERY_MODE PRE_TEST
    PROPERTIES RUN_SERIAL TRUE  # 并行负载下偶发 SEGFAULT（预存共享状态，07 号计划 F）
)

# lifecycle_smoke：生命周期烟雾测试（原 sanitizer_smoke.cpp）
# 不开启 sanitizer flag，仅验证 Buffer/Event/并发生命周期功能正确性。
# 在 sanitizer 开启时也应通过，但本文件不强制 sanitizer。
add_executable(acr_test_sanitizer lifecycle_smoke.cpp)
target_link_libraries(acr_test_sanitizer PRIVATE
    acr_api
    GTest::gtest
)
gtest_discover_tests(acr_test_sanitizer
    DISCOVERY_MODE PRE_TEST
    PROPERTIES RUN_SERIAL TRUE
)

# sanitizer_actual：实际 sanitizer 验证（故意触发 UAF/heap-overflow/UB，验证 sanitizer 开启）
# 仅在 ACR_BUILD_SANITIZER=ON 时触发危险行为；未开启时 SKIP（源文件内编译时检测）。
# 注意：EXPECT_DEATH 需要 threadsafe 模式（源文件 main 中已设置）。
add_executable(acr_test_sanitizer_actual sanitizer_actual.cpp)
target_link_libraries(acr_test_sanitizer_actual PRIVATE
    GTest::gtest
)
if(ACR_BUILD_SANITIZER)
    # 证明 sanitizer 实际开启：构建日志会显示 -fsanitize=address,undefined flag
    target_compile_options(acr_test_sanitizer_actual PRIVATE
        -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(acr_test_sanitizer_actual PRIVATE
        -fsanitize=address,undefined)
    message(STATUS "ACR fault: sanitizer_actual built with -fsanitize=address,undefined (ACR_BUILD_SANITIZER=ON)")
endif()
gtest_discover_tests(acr_test_sanitizer_actual DISCOVERY_MODE PRE_TEST)

# persistence：持续路线测试（默认 5 秒，ACR_PERSIST_DURATION_SEC 可配置）
add_executable(acr_test_persistence persistence.cpp)
target_link_libraries(acr_test_persistence PRIVATE
    acr_api
    acr_profile
    acr_qualification
    GTest::gtest
)
target_include_directories(acr_test_persistence PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../profile
    ${CMAKE_CURRENT_SOURCE_DIR}/../../qualification
)
gtest_discover_tests(acr_test_persistence
    DISCOVERY_MODE PRE_TEST
    PROPERTIES RUN_SERIAL TRUE
)

message(STATUS "ACR fault: fault_injection + lifecycle_smoke + sanitizer_actual(ACR_BUILD_SANITIZER=${ACR_BUILD_SANITIZER}) + persistence")
