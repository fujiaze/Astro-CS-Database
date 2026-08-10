# lib/acr/core/CMakeLists.txt — ACR runtime 核心（oneTBB 封装在此）
# runtime.cpp 使用 Event::Event(shared_ptr<EventImpl>) 构造（定义在 acr_api/event.cpp），
# 形成 acr_api <-> acr_core 循环依赖。CMake 通过在链接行重复库解析循环依赖。
# Phase B4：runtime.cpp 接通 CostEstimator + Dispatcher，链接 acr_scheduler（传递性引入 acr_cost + acr_profile）。
add_library(acr_core STATIC runtime.cpp task_descriptor.cpp)
target_link_libraries(acr_core PUBLIC acr_options TBB::tbb acr_api acr_scheduler)
target_include_directories(acr_core PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../cost
    ${CMAKE_CURRENT_SOURCE_DIR}/../scheduler
    ${CMAKE_CURRENT_SOURCE_DIR}/../profile
)
