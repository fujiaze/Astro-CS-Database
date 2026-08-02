# lib/acr/scheduler/CMakeLists.txt — Phase F Work-Conserving Dispatcher
# 链接 acr_api（parallel_batch 执行 chunks）+ acr_routing（路由决策可选）
add_library(acr_scheduler STATIC
    partitioner.cpp
    queue_aware.cpp
    reduction_merger.cpp
    fallback.cpp
    mixed_runner.cpp
    dispatcher.cpp
)
target_link_libraries(acr_scheduler PUBLIC
    acr_options
    acr_api
)
target_include_directories(acr_scheduler PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}
)
message(STATUS "ACR scheduler: dispatcher + partitioner + mixed_runner + queue_aware + fallback + reduction_merger")
