# lib/acr/scheduler/CMakeLists.txt — Phase F Work-Conserving Dispatcher
# 链接 acr_api（parallel_batch 执行 chunks）+ acr_cost（CostEstimator）
add_library(acr_scheduler STATIC
    partitioner.cpp
    queue_aware.cpp
    reduction_merger.cpp
    fallback.cpp
    mixed_runner.cpp
    dispatcher.cpp
    current_state.cpp
    shared_work_pool.cpp
    device_executor.cpp
    mixed_route_planner.cpp
    residency_manager.cpp
)
target_link_libraries(acr_scheduler PUBLIC
    acr_options
    acr_api
    acr_cost
    acr_utilization
    acr_cuda_bridge_loader
    acr_routing
)
# 本库 PUBLIC 链接 acr_cuda_bridge_loader（其强定义 try_append_cuda_bridge_executors
# 提供真实桥接实现）；device_executor.cpp 的 MSVC no-op 仅在无 loader 的独立构建
# （ASan standalone）需要。定义本宏使完整构建跳过 no-op，避免 LNK2005 重复符号。
target_compile_definitions(acr_scheduler PRIVATE ACR_WITH_BRIDGE_LOADER=1)
# F-fix 8: ACR_BUILD_CUDA=ON 时链接 acr_cuda（CudaExecutor 真实 GPU 执行器）
if(ACR_BUILD_CUDA)
    target_link_libraries(acr_scheduler PUBLIC acr_cuda)
endif()
target_include_directories(acr_scheduler PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../core
    ${CMAKE_CURRENT_SOURCE_DIR}/../cost
    ${CMAKE_CURRENT_SOURCE_DIR}/../utilization
    ${CMAKE_CURRENT_SOURCE_DIR}/../qualification
)
message(STATUS "ACR scheduler: dispatcher + partitioner + mixed_runner + queue_aware + fallback + reduction_merger + current_state + utilization/guided_tail/memory_budget (Phase F3+F4+Commit F)")
