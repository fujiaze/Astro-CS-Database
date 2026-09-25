# lib/infrastructure/acr/tests/CMakeLists.txt — GoogleTest 单测
enable_testing()
add_subdirectory(unit)
add_subdirectory(integration)

# Phase H: 经典实验 E01-E21 + 故障注入 + sanitizer + 持续路线
add_subdirectory(classic)
add_subdirectory(fault)
# WIRING-W34-01: sanitizer 驱动此前只存在于源文件头注释的手敲 cl 命令行里
# （run_acr_sanitizers.ps1 只运行预编译 exe）⇒ 补机器可校验的构建路径。
add_subdirectory(sanitizer)
