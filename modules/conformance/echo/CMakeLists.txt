# modules/conformance/echo — 本模块 target 声明 (ABI-005)
#
# 契约 (BLD-002): 子目录 CMakeLists 只声明本模块 target; 唯一 project()/
# 产品入口在根 CMakeLists.txt; 本文件不作为独立构建入口。
# 本模块为 ABI-005 echo conformance: 跨平台 ABI 契约探针 (全 host callback
# 正/负语义), 独立 SHARED target, 供安全 loader (ABI-003) 动态加载。

# echo SHARED target: Windows 产 astrocs_echo.dll; Linux 产 libastrocs_echo.so
add_library(astrocs_echo SHARED
  src/echo_module.c)
target_include_directories(astrocs_echo PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../include)   # 仓库根 include (astrocs/abi/**)

# 导出可见性 (12 §1: module DLL 只导出批准 C ABI 入口; ABI-006 全查 exports):
#   - Linux: -fvisibility=hidden + 仅 astrocs_module_query_v1 显式 visibility(default)
#     (ASTROCS_EXPORT 在 ASTROCS_ABI_SHARED 下展开为 visibility("default"));
#   - Windows: ASTROCS_ABI_SHARED+ASTROCS_ABI_EXPORTS → dllexport;
#     import 侧 (宿主/测试) 经同一头得 dllimport。
# 本 target 编译时定义 ASTROCS_ABI_EXPORTS (构建 DLL 本体)。
target_compile_definitions(astrocs_echo PRIVATE
  ASTROCS_ABI_SHARED=1
  ASTROCS_ABI_EXPORTS=1)
if(NOT MSVC)
  target_compile_options(astrocs_echo PRIVATE -fvisibility=hidden -Wall -Wextra)
  set_target_properties(astrocs_echo PROPERTIES
    C_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES)
endif()

# 命名对齐 product manifest / install-tree contract (BLD-003 模式):
# Windows 产 astrocs_echo.dll; Linux 产 astrocs_echo.so (PREFIX="" 去掉 lib 前缀)
set_target_properties(astrocs_echo PROPERTIES
  OUTPUT_NAME "astrocs_echo"
  PREFIX "")

# 共址可复用测试 (11 §5): module:<module_id> label + CTest 注册
if(ASTROCS_BUILD_TESTS)
  enable_testing()
  add_executable(echo_host_callback_test tests/unit/echo_host_callback_test.c)
  target_include_directories(echo_host_callback_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include)
  # 直接链接 SHARED (Linux 侧加载验证); Windows 侧 import lib 由同 target 提供
  target_link_libraries(echo_host_callback_test PRIVATE astrocs_echo)
  if(NOT MSVC)
    target_compile_options(echo_host_callback_test PRIVATE -Wall -Wextra)
  endif()
  add_test(NAME module:astrocs.conformance.echo COMMAND echo_host_callback_test)
endif()
