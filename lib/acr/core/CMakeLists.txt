# lib/acr/core/CMakeLists.txt — ACR runtime 核心（oneTBB 封装在此）
add_library(acr_core STATIC runtime.cpp)
target_link_libraries(acr_core PUBLIC acr_options TBB::tbb)
target_include_directories(acr_core PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
