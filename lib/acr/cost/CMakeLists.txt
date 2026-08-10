# lib/acr/cost/CMakeLists.txt — Phase F1+F2 CostEstimator
# CostEstimator 根据 TaskDescriptor + HardwareProfile 推算成本与块大小。
# 链接 acr_api（公共类型）+ acr_profile（HardwareProfileReader）。
add_library(acr_cost STATIC
    cost_estimator.cpp
)
target_link_libraries(acr_cost PUBLIC
    acr_options
    acr_api
    acr_profile
)
target_include_directories(acr_cost PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../core
    ${CMAKE_CURRENT_SOURCE_DIR}/../profile
)
message(STATUS "ACR cost: cost_estimator (Phase F1+F2)")
