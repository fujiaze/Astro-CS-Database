# CMakeLists.txt - HEALPix 浏览器 Qt 版 (healpix_browser_qt)
# 三层构建: core/ 静态库 + widgets/ 静态库 + app/ demo 可执行
# 依赖: Qt6 (Core/Gui/Widgets/OpenGLWidgets), OpenGL, healpix_io.dll
# 编译: cmake -B build -DCMAKE_PREFIX_PATH=C:/msys64/mingw64
#        cmake --build build
# 设计文档: docs/superpowers/specs/2026-07-13-cpp-qt-browser-ui-design.md §7

cmake_minimum_required(VERSION 3.16)
project(healpix_browser_qt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ============================================================================
# 依赖查找
# ============================================================================
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets OpenGLWidgets)
find_package(OpenGL REQUIRED)

# healpix_io 路径 (本模块同级目录)
set(HIO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../healpix_io)

# ============================================================================
# core 静态库 (无 Qt 依赖, 纯 C++17 + OpenGL 3.3 Core)
# ============================================================================
add_library(healpix_browser_core STATIC
    core/browser_backend.cpp
    core/stf_engine.cpp
    core/healpix_math.cpp
    core/gl_renderer.cpp
)
target_include_directories(healpix_browser_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${HIO_DIR}/include
)
target_link_libraries(healpix_browser_core PUBLIC
    OpenGL::GL
    gdi32
)
# healpix_io.dll 在 ${HIO_DIR}, 链接 .a 导入库
target_link_directories(healpix_browser_core PUBLIC ${HIO_DIR})
target_link_libraries(healpix_browser_core PUBLIC healpix_io)

# ============================================================================
# widgets 静态库 (依赖 Qt6 + core)
# ============================================================================
add_library(healpix_browser_qt_widgets STATIC
    widgets/abstract_view.cpp
    widgets/sphere_view.cpp
)
target_include_directories(healpix_browser_qt_widgets PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/widgets
)
target_link_libraries(healpix_browser_qt_widgets PUBLIC
    healpix_browser_core
    Qt6::Core
    Qt6::Gui
    Qt6::OpenGLWidgets
)

# ============================================================================
# demo exe (依赖 widgets + core + Qt6::Widgets)
# ============================================================================
add_executable(healpix_browser_qt WIN32
    app/main.cpp
    app/main_window.cpp
    app/stf_panel.cpp
)
target_link_libraries(healpix_browser_qt PRIVATE
    healpix_browser_qt_widgets
    healpix_browser_core
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

# ============================================================================
# 单元测试 (可选, 通过 -DBUILD_TESTS=ON 启用)
# ============================================================================
option(BUILD_TESTS "Build unit tests" OFF)
if(BUILD_TESTS)
    enable_testing()

    add_executable(test_healpix_math tests/test_healpix_math.cpp core/healpix_math.cpp)
    target_include_directories(test_healpix_math PRIVATE core include)
    target_link_libraries(test_healpix_math PRIVATE)
    add_test(NAME healpix_math COMMAND test_healpix_math)

    add_executable(test_stf_engine tests/test_stf_engine.cpp core/stf_engine.cpp)
    target_include_directories(test_stf_engine PRIVATE core include)
    add_test(NAME stf_engine COMMAND test_stf_engine)

    add_executable(test_browser_backend
        tests/test_browser_backend.cpp
        core/browser_backend.cpp
        core/healpix_math.cpp
    )
    target_include_directories(test_browser_backend PRIVATE core include ${HIO_DIR}/include)
    target_link_directories(test_browser_backend PRIVATE ${HIO_DIR})
    target_link_libraries(test_browser_backend PRIVATE healpix_io)
    add_test(NAME browser_backend COMMAND test_browser_backend)
endif()
