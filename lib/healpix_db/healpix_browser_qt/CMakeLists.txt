# CMakeLists.txt - HEALPix 浏览器 Qt 版 (healpix_browser_qt)
# 三层构建: core/ 静态库 + widgets/ 静态库 + app/ demo 可执行
# 依赖: Qt6 (Core/Gui/Widgets/OpenGLWidgets), OpenGL, astro_image_io.dll
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
find_package(OpenMP)

# astro_image_io 路径 (lib/astro_image_io/, 提供 healpix_io 兼容 API)
set(AIO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../astro_image_io)
# 启用 astro_image_io 的 HEALPix I/O 模块 (兼容宏 hiss_read -> aio_hiss_read 等)
set(AIO_DEFINES AIO_ENABLE_HEALPIX)

# ============================================================================
# core 静态库 (无 Qt 依赖, 纯 C++17 + OpenGL 3.3 Core)
# ============================================================================
add_library(healpix_browser_core STATIC
    core/browser_backend.cpp
    core/stf_engine.cpp
    core/healpix_math.cpp
    core/gl_renderer.cpp
    core/hips_browser_backend.cpp
    core/hips_sky_view.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common/healpix/healpix_core.cpp
)
target_include_directories(healpix_browser_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${AIO_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
)
target_compile_definitions(healpix_browser_core PUBLIC ${AIO_DEFINES})
target_link_libraries(healpix_browser_core PUBLIC
    OpenGL::GL
    gdi32
)
if(OpenMP_CXX_FOUND)
    target_link_libraries(healpix_browser_core PUBLIC OpenMP::OpenMP_CXX)
endif()
# astro_image_io.dll 在 ${AIO_DIR}, 链接 .a 导入库
target_link_directories(healpix_browser_core PUBLIC ${AIO_DIR})
target_link_libraries(healpix_browser_core PUBLIC astro_image_io)

# ============================================================================
# widgets 静态库 (依赖 Qt6 + core)
# ============================================================================
add_library(healpix_browser_qt_widgets STATIC
    widgets/abstract_view.cpp
    widgets/sphere_view.cpp
    widgets/hips_view.cpp
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
# CLI 后台调试工具 (不依赖 Qt Widgets, 纯 C++17 + core + psapi)
# 用途: DLL 诊断 + 文件加载测试 + 性能 benchmark + 模拟操作
# ============================================================================
add_executable(browser_cli
    app/browser_cli.cpp
    core/browser_backend.cpp
    core/stf_engine.cpp
    core/healpix_math.cpp
    core/hips_browser_backend.cpp
    core/hips_sky_view.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common/healpix/healpix_core.cpp
)
target_include_directories(browser_cli PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${AIO_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
)
target_compile_definitions(browser_cli PRIVATE ${AIO_DEFINES})
target_link_directories(browser_cli PRIVATE ${AIO_DIR})
target_link_libraries(browser_cli PRIVATE astro_image_io psapi)
if(OpenMP_CXX_FOUND)
    target_link_libraries(browser_cli PRIVATE OpenMP::OpenMP_CXX)
endif()

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
    target_include_directories(test_browser_backend PRIVATE core include ${AIO_DIR}/include)
    target_compile_definitions(test_browser_backend PRIVATE ${AIO_DEFINES})
    target_link_directories(test_browser_backend PRIVATE ${AIO_DIR})
    target_link_libraries(test_browser_backend PRIVATE astro_image_io)
    add_test(NAME browser_backend COMMAND test_browser_backend)

    add_executable(test_hips_browser_backend
        tests/test_hips_browser_backend.cpp
        core/hips_browser_backend.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../../common/healpix/healpix_core.cpp
    )
    target_include_directories(test_hips_browser_backend PRIVATE
        core include ${AIO_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/../../common)
    target_compile_definitions(test_hips_browser_backend PRIVATE ${AIO_DEFINES})
    target_link_directories(test_hips_browser_backend PRIVATE ${AIO_DIR})
    target_link_libraries(test_hips_browser_backend PRIVATE astro_image_io)
    add_test(NAME hips_browser_backend COMMAND test_hips_browser_backend)

    add_executable(test_geometry_truth
        tests/test_geometry_truth.cpp
        core/hips_browser_backend.cpp
        core/hips_sky_view.cpp
        core/stf_engine.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../../common/healpix/healpix_core.cpp
    )
    target_include_directories(test_geometry_truth PRIVATE
        core include ${AIO_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/../../common)
    target_compile_definitions(test_geometry_truth PRIVATE ${AIO_DEFINES})
    target_link_directories(test_geometry_truth PRIVATE ${AIO_DIR})
    target_link_libraries(test_geometry_truth PRIVATE astro_image_io)
    if(OpenMP_CXX_FOUND)
        target_link_libraries(test_geometry_truth PRIVATE OpenMP::OpenMP_CXX)
    endif()
    add_test(NAME geometry_truth COMMAND test_geometry_truth)
endif()
