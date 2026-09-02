# modules/conformance/noop — 本模块 target 声明 (BLD-002 唯一根图)
#
# 契约 (BLD-002): 子目录 CMakeLists 只声明本模块 target; 唯一 project()/
# 产品入口在根 CMakeLists.txt; 本文件不作为独立构建入口。
# 本模块为 conformance SKELETON (BLD-003): 独立 SHARED target, 供安全 loader
# (ABI-003) 动态加载; 语义 (ABI-005) 填充前不得伪装完成。

# noop SHARED target: Windows 产 astrocs_noop.dll; Linux 产 libastrocs_noop.so
add_library(astrocs_noop SHARED
  src/noop_module.c)
target_include_directories(astrocs_noop PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../include)   # 仓库根 include (astrocs/abi/**)

# 导出可见性 (12 §1: module DLL 只导出批准 C ABI 入口; ABI-006 全查 exports):
#   - Linux: -fvisibility=hidden + 仅 astrocs_module_query_v1 显式 visibility(default)
#     (ASTROCS_EXPORT 在 ASTROCS_ABI_SHARED 下展开为 visibility("default"));
#   - Windows: ASTROCS_ABI_SHARED+ASTROCS_ABI_EXPORTS → dllexport;
#     import 侧 (宿主/测试) 经同一头得 dllimport。
# 本 target 编译时定义 ASTROCS_ABI_EXPORTS (构建 DLL 本体)。
target_compile_definitions(astrocs_noop PRIVATE
  ASTROCS_ABI_SHARED=1
  ASTROCS_ABI_EXPORTS=1)
if(NOT MSVC)
  target_compile_options(astrocs_noop PRIVATE -fvisibility=hidden -Wall -Wextra)
  set_target_properties(astrocs_noop PROPERTIES
    C_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES)
endif()

# 命名对齐 product manifest / install-tree contract (BLD-003):
# Windows 产 astrocs_noop.dll; Linux 产 astrocs_noop.so (PREFIX="" 去掉 lib
# 前缀; install-tree.contract.json 的 MOD-NOOP install_path =
# modules/astrocs_noop.so)
set_target_properties(astrocs_noop PROPERTIES
  OUTPUT_NAME "astrocs_noop"
  PREFIX "")

# 共址可复用测试 (11 §5): module:<module_id> label + CTest 注册
# ASTROCS_BUILD_TESTS 由根 CMakeLists 声明。注意: 根 enable_testing() 在本
# 子目录 add_subdirectory 之后才执行 (根文件尾部), 故本目录须自行
# enable_testing() 才能把 add_test 落入本目录 CTestTestfile.cmake
# (enable_testing 是目录级属性, 多次调用不冲突)。
if(ASTROCS_BUILD_TESTS)
  enable_testing()
  add_executable(noop_handshake_test tests/unit/noop_handshake_test.c)
  target_include_directories(noop_handshake_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include)
  # 直接链接 SHARED (Linux 侧加载验证); Windows 侧 import lib 由同 target 提供
  target_link_libraries(noop_handshake_test PRIVATE astrocs_noop)
  if(NOT MSVC)
    target_compile_options(noop_handshake_test PRIVATE -Wall -Wextra)
  endif()
  add_test(NAME module:astrocs.conformance.noop COMMAND noop_handshake_test)
endif()
