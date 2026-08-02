# lib/acr/tools/acr_invalidate/CMakeLists.txt — acr-invalidate CLI 工具
if(NOT ACR_BUILD_TOOLS)
    return()
endif()

add_executable(acr-invalidate main.cpp)
target_link_libraries(acr-invalidate PRIVATE
    acr_routing
)
target_include_directories(acr-invalidate PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../routing
)
message(STATUS "ACR tool: acr-invalidate")
