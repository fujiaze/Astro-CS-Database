# lib/acr/core/CMakeLists.txt — ACR runtime 核心（oneTBB 封装在此）
# runtime.cpp 使用 Event::Event(shared_ptr<EventImpl>) 构造（定义在 acr_api/event.cpp），
# 形成 acr_api <-> acr_core 循环依赖。CMake 通过在链接行重复库解析循环依赖。
add_library(acr_core STATIC runtime.cpp)
target_link_libraries(acr_core PUBLIC acr_options TBB::tbb acr_api)
target_include_directories(acr_core PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
