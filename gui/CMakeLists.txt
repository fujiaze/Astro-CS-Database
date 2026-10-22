# ACSD v2 GUI —— 独立 CMake 工程
#
# 约束（来自 docs/engineering/rebuild/API.md 与 SYSTEM.md）：
#   * 不加入根 CMakeLists.txt，不链接科学库，不 include src/ 或 include/acsd 的内部头；
#   * 只消费 CLI 配置（acsd.config/2）、JSONL 事件（acsd.events/2）、产品目录，
#     以及取消控制协议（acsd.control/2）。
#
# 构建：
#   cmake -S gui -B build/gui -G Ninja -DCMAKE_PREFIX_PATH=$HOME/qt6/6.7.3/gcc_64
#   cmake --build build/gui
cmake_minimum_required(VERSION 3.25)
project(acsd-gui VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(MSVC)
  # 源码含中文，MSVC 必须显式按 UTF-8 编译。
  add_compile_options(/utf-8)
endif()

# 本地 FindWrapOpenGL：Qt6Gui 硬依赖 OpenGL；最小无显示环境里没有 GL 开发包时，
# 退化为只提供运行时库（libGL.so.1 / libEGL.so.1），头文件并不需要。
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

# FindWrapOpenGL 找到的运行时库目录写进构建期 rpath，离屏运行不必再手工
# 设置 LD_LIBRARY_PATH。
if(ACSD_GUI_OPENGL_RUNTIME_DIR)
  set(CMAKE_BUILD_RPATH "${ACSD_GUI_OPENGL_RUNTIME_DIR}")
  message(STATUS "ACSD GUI: 构建期 rpath 追加 ${ACSD_GUI_OPENGL_RUNTIME_DIR}")
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # 用 DT_RPATH 而不是 DT_RUNPATH：DT_RPATH 会被传递给依赖的依赖查找，
    # 这样 libQt6Gui.so 自己的 libGL.so.1 也能被找到。
    add_link_options(-Wl,--disable-new-dtags)
  endif()
endif()

# ---------------------------------------------------------------------------
# acsd_gui_core —— 只依赖 Qt6 Core 的协议/配置/运行/产品层。
# 不链接任何科学库；协议冒烟工具也复用这一层，保证测的就是 GUI 用的代码。
# ---------------------------------------------------------------------------
add_library(acsd_gui_core STATIC
  src/protocol.h
  src/protocol.cpp
  src/config.h
  src/config.cpp
  src/runner.h
  src/runner.cpp
  src/product.h
  src/product.cpp)
target_include_directories(acsd_gui_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(acsd_gui_core PUBLIC Qt6::Core)
target_compile_features(acsd_gui_core PUBLIC cxx_std_20)

# ---------------------------------------------------------------------------
# acsd-gui —— Qt6 Widgets 图形界面
# ---------------------------------------------------------------------------
add_executable(acsd-gui
  src/main.cpp
  src/mainwindow.h
  src/mainwindow.cpp
  src/smoke.h
  src/smoke.cpp)
target_link_libraries(acsd-gui PRIVATE acsd_gui_core Qt6::Widgets)

# ---------------------------------------------------------------------------
# acsd-protocol-check —— 无窗口的协议检查器（复用 GUI 同一份协议/运行代码）
# ---------------------------------------------------------------------------
add_executable(acsd-protocol-check
  tests/protocol_check.cpp)
target_link_libraries(acsd-protocol-check PRIVATE acsd_gui_core)

install(TARGETS acsd-gui RUNTIME DESTINATION bin)

# ---------------------------------------------------------------------------
# 协议级冒烟（不依赖 Qt）
# ---------------------------------------------------------------------------
enable_testing()
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  add_test(NAME protocol_smoke
           COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/protocol_smoke.py
                   --check $<TARGET_FILE:acsd-protocol-check>)
  set_tests_properties(protocol_smoke PROPERTIES
                       ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
endif()
