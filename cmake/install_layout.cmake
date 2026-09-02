# AstroCS 模块化安装树布局 — cmake/install_layout.cmake (BLD-003)
#
# 契约 (BLD-003 + 03_TARGET_PRODUCT_AND_ARCHITECTURE.md §4 / ARC-001):
#   - 唯一 install 规则源: 根 CMakeLists.txt include 本文件 (BLD-002:
#     子目录 CMakeLists 禁止 install; 产品安装/发布构建一律以根 CMake 为入口);
#   - install 树与 ARC-001 dll_units 计划一一对应 (clean install 仅白名单);
#   - RPATH: 平台 SHARED 之间 install 后依赖以 $ORIGIN 解析 (Windows 同目录
#     查找 DLL); 禁 LD_LIBRARY_PATH/PATH 作为正式发现语义 (12 §6);
#   - Linux .so 为同源技术预览; Windows DLL 布局本文件同步声明, 实际编译/
#     加载验证由 WIN-* 系列在 Fatduck 执行 (10 §5 PLATFORM_SCOPE)。
#
# 安装树 (prefix 可重定位; Linux 技术预览形态):
#   <prefix>/
#     astrocs                      # 主 CLI (exe; Windows: astrocs.exe)
#     libastrocs_runtime.so        # runtime 平台 DLL (loader/registry/... 宿主)
#     libastrocs_io.so             # io 平台 DLL (FITS/HiPS/流式 I/O 宿主)
#     modules/astrocs_noop.so      # conformance module (ABI-005 填充语义)
#     providers/astrocs_cpu_baseline.so   # baseline backend DSO 技术预览
#     schemas/                     # 安装/产品/manifest schema 只读副本
#     licenses/                    # 许可证收集 (第三方 + 本项目声明)
#     astrocs.product.json         # 产品 manifest (模块/DLL/hash 登记; ABI-004 完善)
#
# Windows 正式形态 (03 §4, 由本文件同步 install 规则; WIN-* 验证):
#   astrocs.exe, astrocs_runtime.dll, astrocs_io.dll (根);
#   modules/astrocs_noop.dll; providers/astrocs_cpu_baseline.dll;
#   pipelines/, schemas/, licenses/, README.txt。
#
# 白名单原则: 本文件是唯一 install() 集合; 其余文件绝不 install。
# 每次新增 install 条目必须同时更新 packaging/install-tree.contract.json
# (机器校验: packaging/verify_install_tree.py)。

# ── RPATH: install 后平台 SHARED 依赖 $ORIGIN 解析 (Linux; 12 §6) ──
set(ASTROCS_INSTALL_RPATH "$ORIGIN")
set_property(GLOBAL PROPERTY ASTROCS_PLATFORM_SHARED_TARGETS
  astrocs_runtime astrocs_io astrocs_noop astrocs_cpu_baseline)

function(astrocs_apply_install_rpath tgt)
  if(UNIX AND NOT APPLE)
    set_target_properties(${tgt} PROPERTIES
      INSTALL_RPATH "${ASTROCS_INSTALL_RPATH}"
      BUILD_RPATH_USE_ORIGIN YES)
  endif()
endfunction()

# ── 平台目录 (Windows 与 Linux 统一相对布局) ──
set(ASTROCS_INSTALL_MODULE_SUBDIR "modules")
set(ASTROCS_INSTALL_PROVIDER_SUBDIR "providers")
set(ASTROCS_INSTALL_SCHEMA_SUBDIR "schemas")
set(ASTROCS_INSTALL_LICENSE_SUBDIR "licenses")

# ── 安装主 CLI ──
install(TARGETS astrocs
  RUNTIME DESTINATION . COMPONENT astrocs_runtime)

# ── 平台 SHARED 骨架 (BLD-003) ──
# astrocs_runtime / astrocs_io / astrocs_noop 已定义于根 CMakeLists;
# astrocs_cpu_baseline 为 Linux 技术预览 DSO (legacy backend ABI; 正式
# provider ABI astrocs_provider_query_v1 与 Windows DLL 由 CPU-002 交付)。
if(TARGET astrocs_runtime)
  astrocs_apply_install_rpath(astrocs_runtime)
  install(TARGETS astrocs_runtime
    LIBRARY DESTINATION . COMPONENT astrocs_runtime
    RUNTIME DESTINATION . COMPONENT astrocs_runtime)
endif()
if(TARGET astrocs_io)
  astrocs_apply_install_rpath(astrocs_io)
  install(TARGETS astrocs_io
    LIBRARY DESTINATION . COMPONENT astrocs_runtime
    RUNTIME DESTINATION . COMPONENT astrocs_runtime)
endif()
if(TARGET astrocs_noop)
  astrocs_apply_install_rpath(astrocs_noop)
  install(TARGETS astrocs_noop
    LIBRARY DESTINATION ${ASTROCS_INSTALL_MODULE_SUBDIR} COMPONENT astrocs_runtime
    RUNTIME DESTINATION ${ASTROCS_INSTALL_MODULE_SUBDIR} COMPONENT astrocs_runtime)
endif()
if(TARGET astrocs_cpu_baseline)
  astrocs_apply_install_rpath(astrocs_cpu_baseline)
  install(TARGETS astrocs_cpu_baseline
    LIBRARY DESTINATION ${ASTROCS_INSTALL_PROVIDER_SUBDIR} COMPONENT astrocs_runtime
    RUNTIME DESTINATION ${ASTROCS_INSTALL_PROVIDER_SUBDIR} COMPONENT astrocs_runtime)
endif()

# ── schemas / licenses / product manifest (只读白名单副本) ──
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/packaging/schemas/
  DESTINATION ${ASTROCS_INSTALL_SCHEMA_SUBDIR}
  COMPONENT astrocs_runtime
  FILES_MATCHING PATTERN "*.json" PATTERN "*.schema.json")
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/packaging/licenses/
  DESTINATION ${ASTROCS_INSTALL_LICENSE_SUBDIR}
  COMPONENT astrocs_runtime)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/packaging/astrocs.product.json
  DESTINATION . COMPONENT astrocs_runtime)

message(STATUS "BLD-003 install layout ready (module_dir=${ASTROCS_INSTALL_MODULE_SUBDIR} provider_dir=${ASTROCS_INSTALL_PROVIDER_SUBDIR})")
