# 本地 FindWrapOpenGL —— 供 Qt6Gui 的 find_dependency(WrapOpenGL) 使用。
#
# 背景：Qt6Gui 的 CMake 包硬依赖 WrapOpenGL，而 WrapOpenGL 又要求系统装有
# OpenGL 开发包（libGL + GL/gl.h）。在无显示、无 GL 的最小容器（CI、离屏冒烟）
# 里这些包常常缺失，导致 configure 或链接直接失败。
#
# 这里做两件事：
#   1) 系统有 OpenGL 开发包时照常使用系统的库与头；
#   2) 没有时退化为「只提供运行时库」的方案 —— libQt6Gui.so 自身已在 DT_NEEDED
#      中声明 libGL.so.1 / libEGL.so.1，链接期只需要能找到这两个 so，
#      头文件并不需要（ACSD GUI 不使用 OpenGL API）。
#      查找位置按顺序：环境变量 ACSD_GUI_OPENGL_LIB_DIR、~/.cache/wklibs、
#      系统常见库目录。找到的目录会写进 ACSD_GUI_OPENGL_RUNTIME_DIR，
#      由 gui/CMakeLists.txt 设为构建期 rpath，运行时无需额外设置 LD_LIBRARY_PATH。
#
# 覆盖方式：把本目录放在 CMAKE_MODULE_PATH 之前即可（gui/CMakeLists.txt 已这样做）。

if(NOT TARGET OpenGL::GL)
  find_package(OpenGL QUIET)
endif()

if(NOT TARGET OpenGL::GL)
  add_library(OpenGL::GL INTERFACE IMPORTED)
  add_library(OpenGL::GLX INTERFACE IMPORTED)
  add_library(OpenGL::EGL INTERFACE IMPORTED)
  add_library(OpenGL::OpenGL INTERFACE IMPORTED)

  set(_acsd_gl_hints)
  if(DEFINED ENV{ACSD_GUI_OPENGL_LIB_DIR})
    list(APPEND _acsd_gl_hints "$ENV{ACSD_GUI_OPENGL_LIB_DIR}")
  endif()
  if(DEFINED ENV{HOME})
    list(APPEND _acsd_gl_hints "$ENV{HOME}/.cache/wklibs/usr/lib/x86_64-linux-gnu")
  endif()
  list(APPEND _acsd_gl_hints
       /usr/lib/x86_64-linux-gnu /lib/x86_64-linux-gnu /usr/lib64 /usr/lib /usr/local/lib)

  find_library(ACSD_GUI_LIBGL NAMES GL libGL.so.1 PATHS ${_acsd_gl_hints} NO_DEFAULT_PATH)
  find_library(ACSD_GUI_LIBEGL NAMES EGL libEGL.so.1 PATHS ${_acsd_gl_hints} NO_DEFAULT_PATH)

  set(_acsd_gl_libs)
  foreach(_lib ${ACSD_GUI_LIBGL} ${ACSD_GUI_LIBEGL})
    if(_lib)
      list(APPEND _acsd_gl_libs "${_lib}")
      get_filename_component(_dir "${_lib}" DIRECTORY)
      set(ACSD_GUI_OPENGL_RUNTIME_DIR "${_dir}" CACHE PATH "运行期需要的 GL 库目录")
    endif()
  endforeach()
  if(_acsd_gl_libs)
    set_target_properties(OpenGL::GL PROPERTIES INTERFACE_LINK_LIBRARIES "${_acsd_gl_libs}")
  endif()
endif()

# CMake 内置的 FindWrapOpenGL 会额外提供 WrapOpenGL::WrapOpenGL 别名；
# Qt6::Gui 的链接接口直接引用它，缺了会在 generate 阶段报错。
if(NOT TARGET WrapOpenGL::WrapOpenGL)
  add_library(WrapOpenGL::WrapOpenGL INTERFACE IMPORTED)
  if(TARGET OpenGL::OpenGL)
    set_target_properties(WrapOpenGL::WrapOpenGL PROPERTIES
                          INTERFACE_LINK_LIBRARIES OpenGL::OpenGL)
  endif()
endif()

set(WrapOpenGL_FOUND TRUE)
