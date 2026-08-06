# lib/acr/tests/CMakeLists.txt — GoogleTest 单测
enable_testing()
add_subdirectory(unit)
add_subdirectory(integration)

# Phase H: 经典实验 E01-E21 + 故障注入 + sanitizer + 持续路线
add_subdirectory(classic)
add_subdirectory(fault)
