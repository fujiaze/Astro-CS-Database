# lib/acr/utilization/CMakeLists.txt — Phase G 95% 软占用控制器
add_library(acr_utilization STATIC
    cpu_controller.cpp
    gpu_controller.cpp
    memory_budget.cpp
    io_budget.cpp
    config_hot_read.cpp
    actual_tracker.cpp
)
target_link_libraries(acr_utilization PUBLIC
    acr_options
)
target_include_directories(acr_utilization PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}
)
message(STATUS "ACR utilization: cpu/gpu controller + memory/io budget + config_hot_read + actual_tracker")
