# lib/acr/tests/unit/CMakeLists.txt — 公共 API 单测
# 08 号计划 §7：聚焦 Mixed 能力与性能验收（真实 GPU）
add_executable(acr_test_focused_mixed test_focused_mixed.cpp)
target_link_libraries(acr_test_focused_mixed PRIVATE
    acr_scheduler acr_qualification acr_cost acr_api GTest::gtest_main)
target_include_directories(acr_test_focused_mixed PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../scheduler
    ${CMAKE_CURRENT_SOURCE_DIR}/../../qualification
)
gtest_discover_tests(acr_test_focused_mixed
    DISCOVERY_MODE PRE_TEST
    PROPERTIES RUN_SERIAL TRUE  # 性能敏感：避免与 E10Mandelbrot 等重负载并行触发负载守卫
)

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

# 23 号计划 §5：可恢复时间窗资源闭环
add_executable(acr_test_resource_control test_resource_control.cpp)
target_link_libraries(acr_test_resource_control PRIVATE
    acr_scheduler acr_utilization acr_cost acr_api GTest::gtest_main)
target_include_directories(acr_test_resource_control PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../scheduler
    ${CMAKE_CURRENT_SOURCE_DIR}/../../utilization
)
gtest_discover_tests(acr_test_resource_control DISCOVERY_MODE PRE_TEST)

# 24 号计划 §1：统一 Benchmark→Profile 管线 holdout 预测验证
add_executable(acr_test_profile_holdout test_profile_holdout.cpp)
target_link_libraries(acr_test_profile_holdout PRIVATE
    acr_qualification acr_cost acr_api GTest::gtest_main)
target_include_directories(acr_test_profile_holdout PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../qualification
)
gtest_discover_tests(acr_test_profile_holdout DISCOVERY_MODE PRE_TEST)

# 08 号计划 §3/§4：聚焦目标合成 Operation 与 OperationProfile
add_executable(acr_test_focused_operation test_focused_operation.cpp)
target_link_libraries(acr_test_focused_operation PRIVATE
    acr_qualification acr_api GTest::gtest_main)
target_include_directories(acr_test_focused_operation PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../qualification
)
gtest_discover_tests(acr_test_focused_operation DISCOVERY_MODE PRE_TEST)

# Benchmark 驱动路由（08 计划 D/F）：RouteEstimator 单测
add_executable(acr_test_route_estimator test_route_estimator.cpp)
target_link_libraries(acr_test_route_estimator PRIVATE
    acr_routing
    GTest::gtest_main)
target_include_directories(acr_test_route_estimator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../routing
)
gtest_discover_tests(acr_test_route_estimator DISCOVERY_MODE PRE_TEST)

# Dispatcher Finalization：RouteProfileV2 标定纯函数（Probe/Final 模型语义）
add_executable(acr_test_route_calibration
    test_route_calibration.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../examples/weighted_integration/route_profile_calibration.cpp
)
target_link_libraries(acr_test_route_calibration PRIVATE
    acr_weighted_integration_example
    acr_scheduler
    acr_qualification
    acr_cost
    acr_routing
    acr_api
    acr_cuda_bridge_loader
    GTest::gtest_main)
target_include_directories(acr_test_route_calibration PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../examples/weighted_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../scheduler
    ${CMAKE_CURRENT_SOURCE_DIR}/../../qualification
    ${CMAKE_CURRENT_SOURCE_DIR}/../../core
    ${CMAKE_CURRENT_SOURCE_DIR}/../../cost
    ${CMAKE_CURRENT_SOURCE_DIR}/../../backends/cuda
)
gtest_discover_tests(acr_test_route_calibration DISCOVERY_MODE PRE_TEST)

# Dispatcher Finalization：BDR 三路闭环（07 测试 D；真实 GPU 时全路径执行）
add_executable(acr_test_dispatcher_bdr test_dispatcher_bdr.cpp)
target_link_libraries(acr_test_dispatcher_bdr PRIVATE
    acr_weighted_integration_example
    acr_scheduler
    acr_qualification
    acr_cost
    acr_routing
    acr_api
    acr_cuda_bridge_loader
    GTest::gtest_main)
target_include_directories(acr_test_dispatcher_bdr PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../examples/weighted_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../scheduler
    ${CMAKE_CURRENT_SOURCE_DIR}/../../qualification
    ${CMAKE_CURRENT_SOURCE_DIR}/../../core
    ${CMAKE_CURRENT_SOURCE_DIR}/../../cost
    ${CMAKE_CURRENT_SOURCE_DIR}/../../backends/cuda
)
gtest_discover_tests(acr_test_dispatcher_bdr DISCOVERY_MODE PRE_TEST
    PROPERTIES RUN_SERIAL TRUE  # GPU 性能敏感：避免并发负载守卫
)

# 08 号计划 §5：MixedRoutePlanner 与 RouteMode
add_executable(acr_test_mixed_route test_mixed_route.cpp)
target_link_libraries(acr_test_mixed_route PRIVATE
    acr_scheduler acr_qualification acr_cost acr_api GTest::gtest_main)
target_include_directories(acr_test_mixed_route PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../scheduler
    ${CMAKE_CURRENT_SOURCE_DIR}/../../qualification
)
gtest_discover_tests(acr_test_mixed_route DISCOVERY_MODE PRE_TEST)

# 08 号计划 §6：数据驻留与 pinned 内存预算
add_executable(acr_test_residency test_residency.cpp)
target_link_libraries(acr_test_residency PRIVATE
    acr_scheduler acr_utilization GTest::gtest_main)
target_include_directories(acr_test_residency PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../scheduler
    ${CMAKE_CURRENT_SOURCE_DIR}/../../utilization
)
gtest_discover_tests(acr_test_residency DISCOVERY_MODE PRE_TEST)

# 23 号计划 §3/§6：CUDA 桥接真实 GPU（无 GPU/桥接时 GTEST_SKIP）
add_executable(acr_test_cuda_bridge test_cuda_bridge.cpp)
target_link_libraries(acr_test_cuda_bridge PRIVATE
    acr_scheduler acr_classic_kernels acr_api GTest::gtest_main)
target_include_directories(acr_test_cuda_bridge PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../scheduler
    ${CMAKE_CURRENT_SOURCE_DIR}/../../backends/classic
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)
gtest_discover_tests(acr_test_cuda_bridge DISCOVERY_MODE PRE_TEST)

# 25 号计划 §2：CUDA 分块卷积与独立缓冲容量（无 GPU 时 GTEST_SKIP）
add_executable(acr_test_cuda_bridge_chunk test_cuda_bridge_chunk.cpp)
target_link_libraries(acr_test_cuda_bridge_chunk PRIVATE
    acr_cuda_bridge_loader acr_api GTest::gtest_main)
target_include_directories(acr_test_cuda_bridge_chunk PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../backends/cuda
    ${CMAKE_CURRENT_SOURCE_DIR}/../../backends/cuda/bridge
)
gtest_discover_tests(acr_test_cuda_bridge_chunk DISCOVERY_MODE PRE_TEST)

# 26 号计划 §2：CPU/GPU 50/80/95/100% 持续负载利用率测试已删除
# （用户撤销精确利用率控制；acr_test_resource_sustained.cpp 已移除）。
