# 本地 FindXKB —— 供 Qt6GuiPrivate 的链接接口使用。
#
# 背景：Qt6GuiPrivate 的 INTERFACE_LINK_LIBRARIES 里有 XKB::XKB，configure 时
# 必须存在该目标。最小无显示环境常常没有 libxkbcommon-dev（只有运行时
# libxkbcommon.so.0），CMake 内置的 FindXKB 会因此失败。
#
# 这里优先用 pkg-config 找到真实的 xkbcommon；找不到时给出一个不链接任何库的
# INTERFACE 目标占位 —— libQt6Gui.so 自身已在 DT_NEEDED 中声明 libxkbcommon.so.0。

if(NOT TARGET XKB::XKB)
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(PC_XKB QUIET xkbcommon)
  endif()
  add_library(XKB::XKB INTERFACE IMPORTED)
  if(PC_XKB_FOUND)
    set_target_properties(XKB::XKB PROPERTIES
      INTERFACE_LINK_LIBRARIES "${PC_XKB_LIBRARIES}"
      INTERFACE_INCLUDE_DIRECTORIES "${PC_XKB_INCLUDE_DIRS}")
  endif()
endif()

set(XKB_FOUND TRUE)
