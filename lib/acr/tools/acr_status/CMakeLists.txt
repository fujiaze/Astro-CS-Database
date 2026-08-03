# lib/acr/tools/acr_status/CMakeLists.txt — acr-status CLI 工具
# 由 ACR_BUILD_TOOLS 选项控制
if(NOT ACR_BUILD_TOOLS)
    return()
endif()

add_executable(acr-status main.cpp)
target_link_libraries(acr-status PRIVATE
    acr_profile
    acr_diagnostics
)
target_include_directories(acr-status PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../profile
)
message(STATUS "ACR tool: acr-status")
