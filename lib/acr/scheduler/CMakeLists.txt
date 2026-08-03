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
)
target_link_libraries(acr_scheduler PUBLIC
    acr_options
    acr_api
    acr_cost
    acr_utilization
)
target_include_directories(acr_scheduler PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../core
    ${CMAKE_CURRENT_SOURCE_DIR}/../cost
    ${CMAKE_CURRENT_SOURCE_DIR}/../utilization
)
message(STATUS "ACR scheduler: dispatcher + partitioner + mixed_runner + queue_aware + fallback + reduction_merger + current_state + utilization/guided_tail/memory_budget (Phase F3+F4+Commit F)")
