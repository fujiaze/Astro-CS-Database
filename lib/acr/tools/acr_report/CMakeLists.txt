# lib/acr/tools/acr_report/CMakeLists.txt — acr-report CLI 工具
if(NOT ACR_BUILD_TOOLS)
    return()
endif()

add_executable(acr-report main.cpp)
target_link_libraries(acr-report PRIVATE
    acr_profile
    acr_diagnostics
)
target_include_directories(acr-report PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../profile
)
message(STATUS "ACR tool: acr-report")
