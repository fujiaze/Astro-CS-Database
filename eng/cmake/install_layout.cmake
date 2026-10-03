# ACSD 模块化安装树布局 — eng/cmake/install_layout.cmake (BLD-003)
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
#     acsd                      # 主 CLI (exe; Windows: acsd.exe)
#     libacsd_runtime.so        # runtime 平台 DLL (loader/registry/... 宿主)
#     libacsd_io.so             # io 平台 DLL (FITS/HiPS/流式 I/O 宿主)
#     modules/acsd_noop.so      # conformance module (ABI-005 填充语义)
#     modules/acsd_catalog_gaia.so      # GAIA XPSD catalog service 模块
#     modules/acsd_p1_{drizzle,calibration,cosmetic,hips_writer}.so
#                                  # 科学模块 DLL (MOD-001 安装面, §8.4;
#                                  # acsd_p1_noise 摘出见 F-CI-002-01 注)
#     providers/acsd_cpu_baseline.so   # baseline backend DSO 技术预览
#     providers/acsd_cpu_avx2.so       # 可选 ISA 变体 DSO (SHARED; R-28)
#     providers/acsd_cpu_avx512.so     # 可选 ISA 变体 DSO (SHARED; R-28)
#     providers/backends.manifest.json    # 变体登记面 (实测 sha256 + required_features_bits)
#     schemas/                     # 安装/产品/manifest schema 只读副本
#     licenses/                    # 许可证收集 (第三方 + 本项目声明)
#     acsd.product.json         # 产品 manifest (模块/DLL/hash 登记; ABI-004 完善)
#
# Windows 正式形态 (03 §4, 由本文件同步 install 规则; WIN-* 验证):
#   acsd.exe, acsd_runtime.dll, acsd_io.dll (根);
#   modules/acsd_noop.dll; modules/acsd_catalog_gaia.dll;
#   modules/acsd_p1_{drizzle,calibration,cosmetic,hips_writer}.dll;
#   providers/acsd_cpu_baseline.dll;
#   providers/acsd_cpu_avx2.dll; providers/acsd_cpu_avx512.dll;
#   providers/backends.manifest.json;
#   pipelines/, schemas/, licenses/, README.txt。
#
# 白名单原则: 本文件是唯一 install() 集合; 其余文件绝不 install。
# 每次新增 install 条目必须同时更新 eng/packaging/install-tree.contract.json
# (机器校验: eng/packaging/verify_install_tree.py)。

# ── RPATH: install 后平台 SHARED 依赖 $ORIGIN 解析 (Linux; 12 §6) ──
set(ACSD_INSTALL_RPATH "$ORIGIN")
set_property(GLOBAL PROPERTY ACSD_PLATFORM_SHARED_TARGETS
  acsd_runtime acsd_io acsd_noop acsd_cpu_baseline
  acsd_cpu_avx2 acsd_cpu_avx512)

function(acsd_apply_install_rpath tgt)
  if(UNIX AND NOT APPLE)
    set_target_properties(${tgt} PROPERTIES
      INSTALL_RPATH "${ACSD_INSTALL_RPATH}"
      BUILD_RPATH_USE_ORIGIN YES)
  endif()
endfunction()

# ── 平台目录 (Windows 与 Linux 统一相对布局) ──
set(ACSD_INSTALL_MODULE_SUBDIR "modules")
set(ACSD_INSTALL_PROVIDER_SUBDIR "providers")
set(ACSD_INSTALL_SCHEMA_SUBDIR "schemas")
set(ACSD_INSTALL_LICENSE_SUBDIR "licenses")

# ── 安装主 CLI ──
install(TARGETS acsd
  RUNTIME DESTINATION . COMPONENT acsd_runtime)

# ── 平台 SHARED 骨架 (BLD-003) ──
# acsd_runtime / acsd_io / acsd_noop 已定义于根 CMakeLists;
# acsd_cpu_baseline 为 Linux 技术预览 DSO (legacy backend ABI; 正式
# provider ABI acsd_provider_query_v1 与 Windows DLL 由 CPU-002 交付)。
if(TARGET acsd_runtime)
  acsd_apply_install_rpath(acsd_runtime)
  install(TARGETS acsd_runtime
    LIBRARY DESTINATION . COMPONENT acsd_runtime
    RUNTIME DESTINATION . COMPONENT acsd_runtime)
endif()
if(TARGET acsd_io)
  acsd_apply_install_rpath(acsd_io)
  install(TARGETS acsd_io
    LIBRARY DESTINATION . COMPONENT acsd_runtime
    RUNTIME DESTINATION . COMPONENT acsd_runtime)
endif()
if(TARGET acsd_noop)
  acsd_apply_install_rpath(acsd_noop)
  install(TARGETS acsd_noop
    LIBRARY DESTINATION ${ACSD_INSTALL_MODULE_SUBDIR} COMPONENT acsd_runtime
    RUNTIME DESTINATION ${ACSD_INSTALL_MODULE_SUBDIR} COMPONENT acsd_runtime)
endif()
if(TARGET acsd_cpu_baseline)
  acsd_apply_install_rpath(acsd_cpu_baseline)
  install(TARGETS acsd_cpu_baseline
    LIBRARY DESTINATION ${ACSD_INSTALL_PROVIDER_SUBDIR} COMPONENT acsd_runtime
    RUNTIME DESTINATION ${ACSD_INSTALL_PROVIDER_SUBDIR} COMPONENT acsd_runtime)
endif()

# ── 可选 ISA 变体 provider DSO (docs/engineering/ISA_VARIANTS.md §0/§2 / R-28) ──
# avx2/avx512 变体是**独立动态库**（根 CMakeLists 以 SHARED 声明，编译旗标 target-local），
# 随安装树进 providers/，运行期经 backend_loader 预检→dlopen→self_test 后按
# cpu_profile/能力探测选取；主程序保持基线指令集，缺库或不支持即回退基线。
# 变体**不在** acsd 链接闭包内（check_isa_leak 的 link map 口径不变）。
foreach(_acs_isa_provider acsd_cpu_avx2 acsd_cpu_avx512)
  if(TARGET ${_acs_isa_provider})
    acsd_apply_install_rpath(${_acs_isa_provider})
    install(TARGETS ${_acs_isa_provider}
      LIBRARY DESTINATION ${ACSD_INSTALL_PROVIDER_SUBDIR} COMPONENT acsd_runtime
      RUNTIME DESTINATION ${ACSD_INSTALL_PROVIDER_SUBDIR} COMPONENT acsd_runtime)
  endif()
endforeach()

# providers/backends.manifest.json —— 可选变体的登记面（实测 sha256 + required_features_bits）。
# 与 DSO 同目录安装：backend_loader 的语义是「清单 + 裸文件名同在 backends_dir」，
# 因此运行期/doctor/benchmark 三处的 backends_dir 一律取 <prefix>/providers。
# 清单本身与 DSO 同源生成（根 CMakeLists 的 acsd_backends_manifest 目标）。
if(TARGET acsd_backends_manifest)
  install(FILES ${ACSD_BACKENDS_MANIFEST}
    DESTINATION ${ACSD_INSTALL_PROVIDER_SUBDIR}
    COMPONENT acsd_runtime)
endif()

# ── provider 第二族 DSO + 清单（DYN-740 / C-03 裁决：属 ISA provider 集合 ⇒ 入图并随树分发）──
# 族 = lib/infrastructure/benchmark/cpu/<id>/src/<id>_provider.cpp（+ common/src），入口
# acsd_provider_query_v1（lib/include/acsd/abi/module_api_v1.h 冻结）；消费者 =
# lib/infrastructure/pipeline/module_loader/secure_loader。产物名与 backend 家族不同名
# （acsd_cpuprov_<id> vs acsd_cpu_<id>），两族 ABI 不同故在 providers/ 并列而不覆盖。
# 与 backends.manifest.json 同步安装：清单与 DSO 同目录（清单 + 裸文件名同目录语义）。
foreach(_acs_cpuprov acsd_cpuprov_baseline acsd_cpuprov_avx2 acsd_cpuprov_avx512)
  if(TARGET ${_acs_cpuprov})
    acsd_apply_install_rpath(${_acs_cpuprov})
    install(TARGETS ${_acs_cpuprov}
      LIBRARY DESTINATION ${ACSD_INSTALL_PROVIDER_SUBDIR} COMPONENT acsd_runtime
      RUNTIME DESTINATION ${ACSD_INSTALL_PROVIDER_SUBDIR} COMPONENT acsd_runtime)
  endif()
endforeach()
if(TARGET acsd_providers_manifest)
  install(FILES ${ACSD_PROVIDERS_MANIFEST}
    DESTINATION ${ACSD_INSTALL_PROVIDER_SUBDIR}
    COMPONENT acsd_runtime)
endif()

# ── 科学模块 DLL (MOD-001: 宪章 §8.4 科学模块独立 DLL/SO 安装面) ──
# 契约: acsd_catalog_gaia (CAT-GAIA-IMPL) 与 acsd_p1_{drizzle,calibration,
# cosmetic,hips_writer} (P1-*迁移面) 均为 SHARED target (各子目录 CMakeLists
# 声明), 唯一导出 acsd_module_query_v1 (ABI-006); MOD-001 起随安装树发布
# 到 modules/ 并登记进 eng/packaging/acsd.product.json +
# install-tree.contract.json (三方面同步; §18.4 只加载签名清单官方模块)。
# F-CI-002-01 (owner 裁决 2026-09-11): acsd_p1_noise 随 lib/algorithms/noise_snr
# V7 残留断链解除一并摘出本安装名单/产品清单 (该子图 CMakeLists 未入库, 根
# CMakeLists add_subdirectory 已解除, acsd_p1_noise target 不在根图);
# V7 残留收编后 target 重新出现时随 if(TARGET) 门卫自动恢复安装。
# RPATH: 全部 $ORIGIN (acsd_catalog_gaia 不在根 CMakeLists 的 legacy RPATH
# foreach, 故在本文件统一 apply, install 语义与既有模块一致)。
foreach(tgt acsd_catalog_gaia acsd_p1_drizzle acsd_p1_calibration
            acsd_p1_cosmetic acsd_p1_hips_writer)
  if(TARGET ${tgt})
    acsd_apply_install_rpath(${tgt})
    install(TARGETS ${tgt}
      LIBRARY DESTINATION ${ACSD_INSTALL_MODULE_SUBDIR} COMPONENT acsd_runtime
      RUNTIME DESTINATION ${ACSD_INSTALL_MODULE_SUBDIR} COMPONENT acsd_runtime)
  endif()
endforeach()

# ── schemas / licenses / product manifest (只读白名单副本) ──
# 白名单闭合 (W5-PKG-001): 逐文件枚举, 禁止 DIRECTORY/FILES_MATCHING 通配 ——
# 通配会让新增 schema 静默进包而不进 eng/packaging/install-tree.contract.json
# (改前实测: 安装树 18 文件 vs 合同 16 单元)。新增/删除 schema 必须同步
# 本清单与合同; 两侧一致性由 eng/packaging/check_packaging_consistency.py C2/C6
# 机器判红。
foreach(_acs_schema
    acsd-product.schema.json
    dependency-lock.schema.json
    install-tree-contract.schema.json
    preset-contract.json)
  install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/eng/packaging/schemas/${_acs_schema}
    DESTINATION ${ACSD_INSTALL_SCHEMA_SUBDIR}
    COMPONENT acsd_runtime)
endforeach()
install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/eng/packaging/licenses/CFITSIO_LICENSE.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/eng/packaging/licenses/LICENSE-INDEX.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/eng/packaging/licenses/nlohmann_json.MIT.txt
  DESTINATION ${ACSD_INSTALL_LICENSE_SUBDIR}
  COMPONENT acsd_runtime)
if(WIN32)
  # WIN-PACKAGE 修复(R10 34179477866 实证): eng/packaging/acsd.product.json 是
  # BLD-003 Linux 技术预览骨架(platform=linux-amd64, rel_path=acsd/
  # libacsd_runtime.so/...)。Windows 安装树无条件装它后, candidate 根的
  # acsd.exe modules list/verify/selftest 读到 Linux rel_path →
  # missing_unit_file → 退出 5(ACR BACKEND)。MSVC configure 期生成 Windows
  # 正式形态 manifest(03 §4)并安装生成物; Linux 分支同样在 configure 期生成
  # 交付副本(注入 VERSION/commit), 只是单元 rel_path 取 Linux 形态。
  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/eng/cmake/acsd.product.windows.json.in
    ${CMAKE_CURRENT_BINARY_DIR}/acsd.product.json
    @ONLY)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/acsd.product.json
    DESTINATION . COMPONENT acsd_runtime)
else()
  # Linux 技术预览: 单元列表唯一源 = eng/packaging/acsd.product.json（仓库静态文件,
  # 供 eng/tools/quality/check_module_map.py 等消费）; 交付副本在 configure 期注入当前
  # VERSION 与 commit, 与 Windows 分支同一约定 —— 改前直装静态文件会把「清单登记
  # 时点」的旧版本/旧 SHA 带进安装树 (W5-PKG-001 实测 product_version=alpha.1,
  # source_commit=9f6b72b5 对 VERSION=alpha.2/HEAD 漂移)。
  file(READ ${CMAKE_CURRENT_SOURCE_DIR}/eng/packaging/acsd.product.json _acs_product_manifest)
  string(REGEX REPLACE "\"product_version\": \"[^\"]*\""
    "\"product_version\": \"${ACSD_BASE_VERSION}\"" _acs_product_manifest "${_acs_product_manifest}")
  string(REGEX REPLACE "\"source_commit\": \"[^\"]*\""
    "\"source_commit\": \"${ACSD_GIT_COMMIT}\"" _acs_product_manifest "${_acs_product_manifest}")
  file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/acsd.product.json "${_acs_product_manifest}")
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/acsd.product.json
    DESTINATION . COMPONENT acsd_runtime)
endif()

message(STATUS "BLD-003 install layout ready (module_dir=${ACSD_INSTALL_MODULE_SUBDIR} provider_dir=${ACSD_INSTALL_PROVIDER_SUBDIR})")
