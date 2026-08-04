# lib/acr/api/CMakeLists.txt — Event + KernelRegistry 实现
add_library(acr_api STATIC
    event.cpp
    kernel_registry.cpp
)
target_link_libraries(acr_api PUBLIC acr_core)
target_include_directories(acr_api PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
