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

# Phase E：qualification 单测
add_executable(acr_test_qualification test_qualification.cpp)
target_link_libraries(acr_test_qualification PRIVATE
    acr_qualification acr_api GTest::gtest_main)
target_include_directories(acr_test_qualification PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../qualification
)

# Phase F：scheduler 单测

add_executable(acr_test_scheduler test_scheduler.cpp)
target_link_libraries(acr_test_scheduler PRIVATE
    acr_scheduler acr_api GTest::gtest_main)
target_include_directories(acr_test_scheduler PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../scheduler
)

# Phase G：utilization 单测
add_executable(acr_test_utilization test_utilization.cpp)
target_link_libraries(acr_test_utilization PRIVATE
    acr_utilization GTest::gtest_main)
target_include_directories(acr_test_utilization PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../utilization
)

# Phase D：CUDA backend 单测（ACR_BUILD_CUDA=ON 时编译，OFF 时跳过）
# test_cuda.cpp 用 g++ 编译（CXX），链接 acr_cuda（PUBLIC 传递 cuda/cudart + ACR_BUILD_CUDA）
# F-fix 8: 链接 acr_scheduler 以测试真实 CPU+GPU Mixed 执行（dispatch_via_executors）
if(ACR_BUILD_CUDA)
    add_executable(acr_test_cuda test_cuda.cpp)
    target_link_libraries(acr_test_cuda PRIVATE
        acr_cuda
        acr_scheduler
        acr_api
        GTest::gtest_main)
    target_include_directories(acr_test_cuda PRIVATE
        ${CMAKE_CUDA_TOOLKIT_INCLUDE_DIRECTORIES}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../scheduler
        ${CMAKE_CURRENT_SOURCE_DIR}/../../backends/cuda
        ${CMAKE_CURRENT_SOURCE_DIR}/../..  # lib/acr/ 根目录（cuda_executor.hpp 引用 scheduler/）
    )
    gtest_discover_tests(acr_test_cuda DISCOVERY_MODE PRE_TEST)
endif()

include(GoogleTest)
# DISCOVERY_MODE PRE_TEST：ctest 运行时才发现测试，避免 build 时 hwloc.dll 等
# 运行时依赖不在 PATH 导致 discover 失败（MinGW + MSYS2 系统库场景）
gtest_discover_tests(acr_test_api     DISCOVERY_MODE PRE_TEST)
gtest_discover_tests(acr_test_buffer  DISCOVERY_MODE PRE_TEST)
gtest_discover_tests(acr_test_topology DISCOVERY_MODE PRE_TEST)
gtest_discover_tests(acr_test_qualification DISCOVERY_MODE PRE_TEST)
gtest_discover_tests(acr_test_scheduler DISCOVERY_MODE PRE_TEST)
gtest_discover_tests(acr_test_utilization DISCOVERY_MODE PRE_TEST)

# Phase B3：TaskDescriptor 单测（task_traits_valid 在 acr_core）
add_executable(acr_test_task_descriptor test_task_descriptor.cpp)
target_link_libraries(acr_test_task_descriptor PRIVATE
    acr_core acr_api GTest::gtest_main)
target_include_directories(acr_test_task_descriptor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../core
)
gtest_discover_tests(acr_test_task_descriptor DISCOVERY_MODE PRE_TEST)

# Phase E1：HardwareProfile 数据结构单测（header-only，仅需 include 路径）
add_executable(acr_test_hardware_profile test_hardware_profile.cpp)
target_link_libraries(acr_test_hardware_profile PRIVATE
    acr_api GTest::gtest_main)
gtest_discover_tests(acr_test_hardware_profile DISCOVERY_MODE PRE_TEST)

# Phase F1+F2：CostEstimator 单测（CostEstimator 实现在 acr_cost，传递性引入 acr_profile）
add_executable(acr_test_cost test_cost.cpp)
target_link_libraries(acr_test_cost PRIVATE
    acr_cost GTest::gtest_main)
gtest_discover_tests(acr_test_cost DISCOVERY_MODE PRE_TEST)

# Phase C：CPU 画像生成与读取单测（ProfileGenerator + HardwareProfile）
add_executable(acr_test_cpu_profile test_cpu_profile.cpp)
target_link_libraries(acr_test_cpu_profile PRIVATE
    acr_qualification acr_api GTest::gtest_main)
target_include_directories(acr_test_cpu_profile PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../qualification
)
gtest_discover_tests(acr_test_cpu_profile DISCOVERY_MODE PRE_TEST)

# Phase B4：TaskTraits-based 公共 API 集成测试（Commit C）
# 验证 parallel_for/tiles/reduce/batch 新签名接通 CostEstimator + Dispatcher，
# OperationId/traits 不被忽略，CPU fallback 明确。
add_executable(acr_test_api_traits test_api_traits.cpp)
target_link_libraries(acr_test_api_traits PRIVATE
    acr_core acr_cost acr_api GTest::gtest_main)
target_include_directories(acr_test_api_traits PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../core
)
gtest_discover_tests(acr_test_api_traits DISCOVERY_MODE PRE_TEST)

# 23 号计划 §1：KernelRegistry（可加速 ABI 分层）
add_executable(acr_test_kernel_registry test_kernel_registry.cpp)
target_link_libraries(acr_test_kernel_registry PRIVATE
    acr_api GTest::gtest_main)
gtest_discover_tests(acr_test_kernel_registry DISCOVERY_MODE PRE_TEST)

# 23 号计划 §2：共享工作池并发专项（稳定 WorkToken / 正确完成判据）
add_executable(acr_test_work_pool test_work_pool.cpp)
target_link_libraries(acr_test_work_pool PRIVATE
    acr_scheduler acr_api GTest::gtest_main)
target_include_directories(acr_test_work_pool PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../scheduler
)
gtest_discover_tests(acr_test_work_pool DISCOVERY_MODE PRE_TEST)

# 23 号计划 §3：DeviceExecutor（真实执行 + 统计回填）
add_executable(acr_test_device_executor test_device_executor.cpp)
target_link_libraries(acr_test_device_executor PRIVATE
    acr_scheduler acr_api GTest::gtest_main)
target_include_directories(acr_test_device_executor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../scheduler
)
gtest_discover_tests(acr_test_device_executor DISCOVERY_MODE PRE_TEST)

# 23 号计划 §4：CostEstimator 驱动的每设备动态领取（dispatch_invocation）
add_executable(acr_test_invocation_dispatch test_invocation_dispatch.cpp)
target_link_libraries(acr_test_invocation_dispatch PRIVATE
    acr_scheduler acr_cost acr_api GTest::gtest_main)
target_include_directories(acr_test_invocation_dispatch PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../scheduler
    ${CMAKE_CURRENT_SOURCE_DIR}/../../cost
)
gtest_discover_tests(acr_test_invocation_dispatch DISCOVERY_MODE PRE_TEST)
