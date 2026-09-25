# ACSD 根 CMake — 唯一产品构建入口与唯一产品事实源 (BLD-002)
# ── 契约 (BLD-002 唯一根 CMake 构建图) ──
# 1) 本文件是仓库唯一 project() 与唯一产品事实源: 产品可执行 acsd 只在此声明
#    (add_executable(acsd ...)), 显式源列表, 禁 GLOB (QA-002)。
# 2) 子目录 CMakeLists 只声明本模块 target (eng/tests/unit 为测试, 见文末 add_subdirectory)。
# 3) 旧 CLI 兼容子图的 CMakeLists（仓库根遗留, 非产品事实源; 其路径同步归 INT-001）与
#    lib/algorithms/coverage/CMakeLists.txt 已降级为 compatibility 声明 (BLD-002): 保留给
#    遗留 eng/tests/cli 独立构建与模块自测/工具按需构建, 禁止正式 install; 产品安装/发布
#    构建一律以本文件为入口。
# 4) 正式构建入口: 本文件 (Windows preset win-msvc-17.14.39-x64 / Linux linux-control)。
cmake_minimum_required(VERSION 3.24)
# project() VERSION 只接受数字三元组(CMake 语法); alpha 后缀由根 VERSION 唯一承载,
# 生成串 ASTROCS_VERSION_STRING = VERSION + g<commit> (V81-ADOPT-006 统一规则)。
# 同步规则: 此三元组必须 == 根 VERSION 的 major.minor.patch (eng/ci/check_version.py 校验)。
project(acsd VERSION 0.1.0 LANGUAGES C CXX)

# ── 选项 (03/BLD-001 冻结) ──
option(ASTROCS_ENABLE_ACR "Build dormant ACR tree (default OFF, production exclusion)" OFF)
option(ASTROCS_BUILD_TESTS "Build tests" ON)
# QA-002: sanitizer 选项 (ASan/UBSan/LSan 或 TSan; 生产默认 OFF)
option(ASTROCS_ENABLE_SANITIZERS "Enable ASan/UBSan/LSan (GCC/Clang, production OFF)" OFF)
option(ASTROCS_ENABLE_TSAN "Enable ThreadSanitizer (targeted, mutually exclusive with ASan)" OFF)
# RELEASE-02 性能探针 (P1/P2 热点定位)。默认 OFF —— 关闭时探针宏展开为空语句
# ((void)0), 不编译 probe.cpp, 链接产物中无 astrocs::probe::* 符号; 既有构建与
# 测试行为完全不变。ON 时仍需运行期环境变量 ASTROCS_PROBE_LOG=<path> 才产出。
option(ASTROCS_PROBES "Enable low-overhead runtime performance probes (default OFF; runtime gate ASTROCS_PROBE_LOG)" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

# ── QA-002: sanitizer 接线 (GCC/Clang; MSVC 不支持) ──
# ASan+UBSan: -fsanitize=address,undefined; LSan 随 ASan 内建, detect_leaks=1 由运行期验证。
# TSan: -fsanitize=thread; 与 ASan 互斥 (单选)。
if(NOT MSVC)
  if(ASTROCS_ENABLE_TSAN)
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=thread)
  elseif(ASTROCS_ENABLE_SANITIZERS)
    add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address,undefined)
  endif()
else()
  # WIN-001: MSVC 全局源码选项 — 源码 UTF-8 (中文注释/字符串) 必需 /utf-8,
  # 否则 MSVC 按系统 ANSI 代码页误读 → 注释边界错乱 → 级联语法错误 (contracts.h C2144)。
  # NOMINMAX: windows.h 的 min/max 宏会破坏 std::min/std::max (C2589, monitor/recorder)。
  # _USE_MATH_DEFINES: MSVC 需此宏才暴露 M_PI (wcs_tan.cpp 等)。
  add_compile_options(/utf-8 /W4 /EHsc)
  add_compile_definitions(NOMINMAX _USE_MATH_DEFINES)
endif()

# 版本单源: 根 VERSION 文件 (VER-001)
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/VERSION ASTROCS_BASE_VERSION)
string(STRIP "${ASTROCS_BASE_VERSION}" ASTROCS_BASE_VERSION)
# git 面 fail-closed（VER-001 唯一版本源 / ENGINEERING_SPEC §10 / AGENTS.md §6）：
# 生成串的 "+g<sha>" 分量**只能**来自 git HEAD —— 没有 git 就没有诚实的版本串。
# 原实现退化到全零 SHA 继续构建（实测：归档树 "git archive HEAD | tar -x" 内
# cmake rc=0，version_generated.h 得 "...+g0000...0"）⇒ 版本门变**假绿**：看着有
# 版本串，其实是 0000000，零追溯价值，且以环境问题掩盖失败。
# 现口径与 eng/tools/gen_version.py::main 逐字同构（具名 GIT_UNAVAILABLE +
# 拒绝给出结论 + rc=2），先例 eng/tools/traceability/check_traceability_matrix.py:364
# （非 git 工作树 ⇒ GIT_UNAVAILABLE rc=2）。非 git 树（归档树 / 测试镜像树 /
# 无 .git 的检出，形态见 eng/tools/check_warning_suppression.py:30）在此**配置期**
# 即判红，不产出任何可被当作合法版本串消费的串。
execute_process(COMMAND git rev-parse HEAD
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  OUTPUT_VARIABLE ASTROCS_GIT_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_VARIABLE ASTROCS_GIT_ERR RESULT_VARIABLE ASTROCS_GIT_RC)
if(NOT ASTROCS_GIT_RC EQUAL 0 OR ASTROCS_GIT_COMMIT STREQUAL "")
  message(FATAL_ERROR
    "GIT_UNAVAILABLE: git rev-parse HEAD 失败（rc=${ASTROCS_GIT_RC}; ${ASTROCS_GIT_ERR}）"
    " —— 版本串的 +g<sha> 分量只能来自 git HEAD，退化为全零 SHA 会让版本门假绿。"
    " 依据 VER-001 / ENGINEERING_SPEC §10 / AGENTS.md §6；与 eng/tools/gen_version.py"
    " 的 GIT_UNAVAILABLE fail-closed 同口径。请在 git 工作树内配置构建。")
endif()
set(ASTROCS_VERSION_STRING "${ASTROCS_BASE_VERSION}+g${ASTROCS_GIT_COMMIT}")

# 版本生成头 (VER-001: 从 VERSION 唯一源)
configure_file(lib/infrastructure/cli/version_generated.h.in ${CMAKE_CURRENT_BINARY_DIR}/version_generated.h @ONLY)
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/version_generated.h
  "#define ASTROCS_COMMIT_SHA \"${ASTROCS_GIT_COMMIT}\"\n")

# ── 构建期指纹 (RUN-PROVENANCE-01) ───────────────────────────────────────────
# 缺陷（一手证据 run/RUN-PROVENANCE-01/REPORT.md §A）: ASTROCS_COMMIT_SHA 只在
# **configure 期**采样一次；Ninja 的 RERUN_CMAKE 规则只依赖 CMake 输入（实测
# build/build.ninja:19586 的依赖面里没有任何 .cpp/.h）⇒ 改源码不会重跑 configure
# ⇒ 编译进去的是新代码，provenance 记的却是旧 SHA；同一次 configure 下的所有运行
# 共享同一个 source_sha，于是"同 source_sha"被误当成"同一二进制"（事故形态见
# run/RUN-PROVENANCE-01/logs/A2_repro_defect.log）。
# 修法: 另出**构建期**采样的 build_stamp_generated.h（HEAD / dirty / 源集内容摘要），
# 由 eng/tools/gen_build_stamp.py 在构建期生成；依赖面 = git 索引给出的**显式清单**
# （非 GLOB，QA-002），脚本内容不变时不落盘（mtime 不变 ⇒ 不触发下游重编译）。
# 语义权威: docs/VERSIONING.md「构建指纹合同」；消费者规则: 判"同一代码/同一二进制"
# **只看** ASTROCS_BUILD_SOURCE_DIGEST —— source_sha 只表示 configure 时刻的 HEAD。
find_package(Python3 COMPONENTS Interpreter QUIET)
set(ASTROCS_STAMP_PYTHON "python3")
if(Python3_EXECUTABLE)
  set(ASTROCS_STAMP_PYTHON "${Python3_EXECUTABLE}")
endif()
execute_process(COMMAND git ls-files -- lib eng/cmake CMakeLists.txt VERSION CMakePresets.json
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  OUTPUT_VARIABLE ASTROCS_STAMP_FILES OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_VARIABLE ASTROCS_STAMP_ERR RESULT_VARIABLE ASTROCS_STAMP_RC)
if(NOT ASTROCS_STAMP_RC EQUAL 0)
  message(FATAL_ERROR
    "GIT_UNAVAILABLE: git ls-files 失败（rc=${ASTROCS_STAMP_RC}; ${ASTROCS_STAMP_ERR}）"
    " —— 构建期指纹的依赖面来自 git 索引；没有 git 就没有可信指纹，"
    "与 eng/tools/gen_build_stamp.py 的 GIT_UNAVAILABLE fail-closed 同口径。")
endif()
string(REPLACE "\n" ";" ASTROCS_STAMP_FILES "${ASTROCS_STAMP_FILES}")
set(ASTROCS_STAMP_DEPS "")
foreach(_f IN LISTS ASTROCS_STAMP_FILES)
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${_f})
    list(APPEND ASTROCS_STAMP_DEPS ${CMAKE_CURRENT_SOURCE_DIR}/${_f})
  endif()
endforeach()
list(LENGTH ASTROCS_STAMP_DEPS ASTROCS_STAMP_N)
message(STATUS "构建期指纹依赖面: ${ASTROCS_STAMP_N} 个源文件 (RUN-PROVENANCE-01)")
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/build_stamp_generated.h
  COMMAND ${ASTROCS_STAMP_PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/eng/tools/gen_build_stamp.py
          --header ${CMAKE_CURRENT_BINARY_DIR}/build_stamp_generated.h
          --manifest ${CMAKE_CURRENT_BINARY_DIR}/build_stamp_manifest.json
          --configure-head ${ASTROCS_GIT_COMMIT}
  DEPENDS ${ASTROCS_STAMP_DEPS} ${CMAKE_CURRENT_SOURCE_DIR}/eng/tools/gen_build_stamp.py
  COMMENT "生成构建期指纹 build_stamp_generated.h (RUN-PROVENANCE-01)"
  VERBATIM)

# ── G-RES-01 数值唯一源 (GATE-FIX-RES / R-4 D-11) ────────────────────────────
# 阈值不得在 C++ 里复制字面量: 由 eng/contracts/resource_gate_v1.json 生成
# resource_gate_thresholds_generated.h (CMake >= 3.19 string(JSON), 无额外依赖)。
# 判据语义权威: docs/plugins/infrastructure/21_observability.md §8。
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/eng/contracts/resource_gate_v1.json ASTROCS_RESOURCE_GATE_JSON)
string(JSON ASTROCS_RG_APPLICABILITY GET "${ASTROCS_RESOURCE_GATE_JSON}" applicability)
string(JSON ASTROCS_RG_COMPUTE GET "${ASTROCS_RESOURCE_GATE_JSON}" compute)
string(JSON ASTROCS_RG_MEMORY GET "${ASTROCS_RESOURCE_GATE_JSON}" memory)
string(JSON ASTROCS_RG_MIN_EFFECTIVE_CPUS GET "${ASTROCS_RG_APPLICABILITY}" min_effective_cpus)
string(JSON ASTROCS_RG_MIN_ACTIVE_WINDOW_SECONDS_EXCLUSIVE GET "${ASTROCS_RG_APPLICABILITY}" min_active_window_seconds_exclusive)
string(JSON ASTROCS_RG_MIN_ACTIVE_THREADS GET "${ASTROCS_RG_COMPUTE}" min_active_compute_threads)
string(JSON ASTROCS_RG_MEAN_UTIL_PERCENT GET "${ASTROCS_RG_COMPUTE}" mean_utilization_min_percent)
string(JSON ASTROCS_RG_P50_UTIL_PERCENT GET "${ASTROCS_RG_COMPUTE}" p50_utilization_min_percent)
string(JSON ASTROCS_RG_PER_SAMPLE_UTIL_PERCENT GET "${ASTROCS_RG_COMPUTE}" per_sample_utilization_min_percent)
string(JSON ASTROCS_RG_PER_SAMPLE_PASS_FRACTION GET "${ASTROCS_RG_COMPUTE}" per_sample_pass_fraction_min)
string(JSON ASTROCS_RG_QUEUE_LOW_UTIL_PERCENT GET "${ASTROCS_RG_COMPUTE}" queue_low_utilization_percent)
string(JSON ASTROCS_RG_QUEUE_LOW_WINDOW_SECONDS GET "${ASTROCS_RG_COMPUTE}" queue_low_window_seconds_min)
string(JSON ASTROCS_RG_MEM_GROWTH_MIB_PER_S GET "${ASTROCS_RG_MEMORY}" growth_limit_mib_per_s)
string(JSON ASTROCS_RG_ALLOC_RECLAIM_MIN_FRACTION GET "${ASTROCS_RG_MEMORY}" allocation_reclaim_min_fraction)
string(JSON ASTROCS_RG_ALLOC_RECLAIM_MIN_WALL_SECONDS GET "${ASTROCS_RG_MEMORY}" allocation_reclaim_min_wall_seconds)
string(JSON ASTROCS_RG_RECLAIM_RESIDUAL_TOL_BYTES GET "${ASTROCS_RG_MEMORY}" reclaim_residual_tolerance_bytes)
string(JSON ASTROCS_RG_WORKLOAD_FLOOR_CORE_SECONDS GET "${ASTROCS_RESOURCE_GATE_JSON}" workload_floor_core_seconds)
configure_file(lib/infrastructure/cli/resource_gate_thresholds_generated.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/resource_gate_thresholds_generated.h @ONLY)

# ── 运行时资源预算比例的唯一数值源 ──────────────────────────────────────────
# 依据: ASTROCS_DESIGN.md §8.3/§9（静态预算 = 调度准入输入；内存不设资源门）
#       + SCHEDULER_CONTRACT §3（内存上限由配置决定，禁硬编码）。
# 与上面 G-RES-01 同款机制: 数值只写一次（eng/packaging/config/runtime_resources.json），
# C++ 侧经生成头消费 ⇒ 实现侧零字面量（不得硬编码比例）。
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/eng/packaging/config/runtime_resources.json ASTROCS_RUNTIME_RESOURCES_JSON)
string(JSON ASTROCS_RR_MEMORY_BUDGET_PERCENT GET "${ASTROCS_RUNTIME_RESOURCES_JSON}" memory_budget_percent)
# 内存压力治理的水位/去抖/节流（ASTROCS_DESIGN.md §8.3:609-615 编排策略）——
# 与预算比例同一个唯一数值源，不新增第二份配置面。
string(JSON ASTROCS_RR_MEMORY_PRESSURE_ENABLED GET "${ASTROCS_RUNTIME_RESOURCES_JSON}" memory_pressure_enabled)
string(JSON ASTROCS_RR_MEMORY_PRESSURE_HIGH_PERCENT GET "${ASTROCS_RUNTIME_RESOURCES_JSON}" memory_pressure_high_percent)
string(JSON ASTROCS_RR_MEMORY_PRESSURE_LOW_PERCENT GET "${ASTROCS_RUNTIME_RESOURCES_JSON}" memory_pressure_low_percent)
string(JSON ASTROCS_RR_MEMORY_PRESSURE_HIGH_DWELL GET "${ASTROCS_RUNTIME_RESOURCES_JSON}" memory_pressure_high_dwell_samples)
string(JSON ASTROCS_RR_MEMORY_PRESSURE_LOW_DWELL GET "${ASTROCS_RUNTIME_RESOURCES_JSON}" memory_pressure_low_dwell_samples)
string(JSON ASTROCS_RR_MEMORY_PRESSURE_EVICT_AFTER GET "${ASTROCS_RUNTIME_RESOURCES_JSON}" memory_pressure_evict_after_samples)
string(JSON ASTROCS_RR_MEMORY_PRESSURE_SAMPLE_INTERVAL_MS GET "${ASTROCS_RUNTIME_RESOURCES_JSON}" memory_pressure_sample_interval_ms)
configure_file(eng/packaging/config/runtime_resources_generated.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/runtime_resources_generated.h @ONLY)

# vendored cfitsio 显式清单 (BLD-001 禁 GLOB; 由 eng/tools/gen_cfitsio_list.py 生成)
include(${CMAKE_CURRENT_SOURCE_DIR}/eng/cmake/cfitsio_sources.cmake)

# OpenMP 探测提前: 遗留模块 (calibration/aio/drizzle) 含 omp pragma, 编译需 -fopenmp
# (QA-001: 自有遗留 target target-local 编译, 不污染主程序; 第三方 cfitsio 仍 -w 隔离)
if(UNIX)
  find_package(OpenMP QUIET)
endif()

# ── QA-006 覆盖插桩作用域 (F8-b 修复, V8-CI-012 R6.5): 必须位于首个 add_library/
# add_subdirectory 之前。目录级 add_compile_options/add_link_options 只影响其后声明的
# target; 原位置在 lib/tests/unit 之后导致 *_test 可执行未插桩链接 (无 __llvm_profile
# runtime) → 测试进程零 profraw。前移后 lib/cli/tests/unit 全部插桩编译链接,
# *_test 落 profraw, llvm-cov 经 *_test objects 报 lib/cli 覆盖。
if(NOT WIN32 AND NOT MSVC)
  option(ASTROCS_BUILD_COVERAGE "Build instrumented for coverage + ccov target (deep CI)" OFF)
  if(ASTROCS_BUILD_COVERAGE)
    find_program(ASTROCS_LLVM_PROFDATA llvm-profdata)
    find_program(ASTROCS_LLVM_COV llvm-cov)
    if(NOT ASTROCS_LLVM_PROFDATA OR NOT ASTROCS_LLVM_COV)
      message(FATAL_ERROR "ASTROCS_BUILD_COVERAGE=ON 需要 llvm-profdata/llvm-cov (linux-deep CI host 提供)")
    endif()
    add_compile_options(-fprofile-instr-generate -fcoverage-mapping)
    add_link_options(-fprofile-instr-generate)
  endif()
endif()

# ── 显式 target 依赖图 (依赖方向见 ARCH-001 §3) ──

# core/contracts: 公共类型 (DATA/API/MOD)
add_library(astrocs_contracts INTERFACE)
target_include_directories(astrocs_contracts INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/include
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/shared/include)

# io adapter (IO-001: 依赖方向 core <- io; 不 include 平台 runtime 模块)
# BLD-003: 更名 acsd_io_adapter —— ARC-001 冻结的 DLL target 计划
# (install_tree 一一对应) 中 "acsd_io" 是平台 SHARED DLL (acsd_io.dll/.so,
# FITS/HiPS/原子流式 I/O 宿主服务), 见下 "BLD-003 平台 SHARED 骨架" 段落;
# 本 STATIC 为 legacy 文件适配器 (ArtifactTransaction/FileIoAdapter, IO-001),
# 保留原名会导致 target 重名 (CMake fatal)。
add_library(acsd_io_adapter STATIC lib/infrastructure/aio/io/src/io_adapter.cpp)
target_include_directories(acsd_io_adapter PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/lib/include)
target_link_libraries(acsd_io_adapter PUBLIC astrocs_core)

# ── BLD-003 平台 SHARED 骨架 (G2: SHARED acsd_runtime/io/noop/baseline targets) ──
# 契约 (BLD-003 + 03_TARGET §4.1 + ARC-001):
#   - 唯一 install 规则源 = eng/cmake/install_layout.cmake (文末 include);
#     install 树与 ARC-001 dll_units 计划一一对应; clean install 仅白名单。
#   - acsd_runtime / acsd_io: 平台 SHARED DLL 骨架。当前以已冻结、
#     自包含的 C ABI 源 (DATA-001 artifact / IO-001 fits stream) 起步,
#     使 target 真实可编译、可安装、可被 ABI-003 loader 加载验证;
#     loader/registry/DAG/scheduler/ArtifactStore 接线由 ABI-003/004 与
#     RT-002 等后续向本 target 追加源, 本任务不伪装平台实现完成。
#   - astrocs_noop: conformance module SHARED (eng/tests/conformance/noop),
#     仅可加载性骨架 (SKELETON), 语义由 ABI-005 填充; 见其 README。
#   - astrocs_cpu_baseline: baseline backend DSO。Linux 以 legacy backend
#     ABI 源 (baseline_backend.cpp, SA-CPU-09 已有) 编译为 .so 技术预览,
#     仅验证 provider 安装路径/RPATH/加载; 正式 provider ABI
#     (astrocs_provider_query_v1, module_api_v1.h 冻结) 与 Windows DLL 由
#     CPU-002 (lib/infrastructure/benchmark/cpu/baseline) 交付, 本任务不伪装 provider 完成。
# 导出可见性 (12 §1/ABI-006): module DLL 只导出批准 C ABI 入口; C 源 DLL
# 使用 ASTROCS_ABI_SHARED/EXPORTS + -fvisibility=hidden 控制 (各 target)。

# acsd_runtime: runtime 平台 DLL 骨架 (artifact C ABI, DATA-001 冻结源)
# 导出: 源为冻结 C ABI (无 ASTROCS_EXPORT 标注, SA-IO-07/DATA-06 头所有
# 顶层函数即 API) → Linux 默认全导出; Windows 经 WINDOWS_EXPORT_ALL_SYMBOLS
# (C 源, 导出全部顶层 API; ABI-006 收紧为精确导出清单, 正式 DLL 由 WIN-* 复核)。
add_library(acsd_runtime SHARED lib/infrastructure/aio/runtime/artifact_store/artifact_abi_v1.c)
target_include_directories(acsd_runtime PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/include)
if(MSVC)
  set_property(TARGET acsd_runtime PROPERTY WINDOWS_EXPORT_ALL_SYMBOLS ON)
else()
  target_compile_options(acsd_runtime PRIVATE -Wall -Wextra)
endif()
if(WIN32)
  set_target_properties(acsd_runtime PROPERTIES OUTPUT_NAME "acsd_runtime")
endif()

# acsd_io: io 平台 DLL 骨架 (FITS stream C ABI, IO-001 冻结源)
add_library(acsd_io SHARED lib/infrastructure/aio/io/fits_core.c)
target_include_directories(acsd_io PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/include
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/infrastructure/aio/io/include)
if(MSVC)
  set_property(TARGET acsd_io PROPERTY WINDOWS_EXPORT_ALL_SYMBOLS ON)
  # HOSTFIX (WIN-BUILD C1189): fits_core.c 用 C11 <stdatomic.h> (tmp 名计数器),
  # MSVC C11 atomics 是实验特性, 默认关闭 → vcruntime_c11_stdatomic.h 抛
  # "C atomic support is not enabled" (C1189), acsd_io.dll 构建失败并连带
  # io_* 测试。/experimental:c11atomics 需 MSVC 19.35+ (VS 17.5+); 只作用于
  # 本 target 的 C 编译单元, 不影响其他 C/C++ target。
  target_compile_options(acsd_io PRIVATE
    $<$<AND:$<COMPILE_LANGUAGE:C>,$<C_COMPILER_ID:MSVC>,$<VERSION_GREATER_EQUAL:$<C_COMPILER_VERSION>,19.35>>:/experimental:c11atomics>)
else()
  target_compile_options(acsd_io PRIVATE -Wall -Wextra)
endif()
if(WIN32)
  set_target_properties(acsd_io PROPERTIES OUTPUT_NAME "acsd_io")
endif()
# fits_core.c 需数学库 (Linux); Windows CRT 自带
if(NOT WIN32)
  target_link_libraries(acsd_io PRIVATE m)
endif()

# astrocs_noop: conformance module SHARED (模块 CMakeLists 只声明本 target,
# BLD-002: 子目录 CMakeLists 非产品事实源; 见 eng/tests/conformance/noop/README)
add_subdirectory(eng/tests/conformance/noop)

# astrocs_catalog_gaia: GAIA XPSD catalog service module DLL (CAT-GAIA-IMPL;
# 矩阵行 astrocs.catalog.gaia / astrocs_catalog_gaia.dll)。唯一导出
# astrocs_module_query_v1; legacy 符号经 GAIA_EXPORT= 本地化 (模块 CMakeLists)。
add_subdirectory(lib/infrastructure/gaia_xpsd_client)

# astrocs_p1_drizzle: drizzle 迁移模块 DLL (P1-DRZ-IMPL; 矩阵行
# astrocs.p1.drizzle / astrocs_p1_drizzle.dll)。九操作 C ABI v1 adapter
# 包装 legacy healpix_drizzle 生产源 (API-DRZ-001 冻结, scientific_change=
# false 零改动重编译); 唯一导出 astrocs_module_query_v1, legacy 六符号经
# version-script 降 local (模块 CMakeLists)。
add_subdirectory(lib/algorithms/drizzle)

# astrocs_p1_calibration: calibration 迁移模块 DLL (P1-CAL-IMPL; 矩阵行
# astrocs.p1.calibration / astrocs_p1_calibration.dll)。九操作 C ABI v1
# adapter 包装 legacy calibration 生产 4 TU (API-P1-001 冻结, scientific_
# change=false 零改动重编译); 唯一导出 astrocs_module_query_v1, legacy 12
# 符号经 AC_API= 本地化 + version-script 双保险 (模块 CMakeLists)。
add_subdirectory(lib/algorithms/calibration)

# astrocs_p1_cosmetic: cosmetic 迁移模块 DLL (P1-COS-IMPL; 矩阵行
# astrocs.p1.cosmetic / astrocs_p1_cosmetic.dll)。九操作 C ABI v1
# adapter 包装 legacy cosmetic 生产闭包 2 TU (API-COS-001 冻结,
# scientific_change=false 零改动重编译); 唯一导出
# astrocs_module_query_v1, legacy AC_API 符号经 AC_API= 本地化 +
# version-script 双保险 (模块 CMakeLists)。
add_subdirectory(lib/algorithms/cosmetic)

# astrocs_p1_hips_writer: HiPS writer 迁移模块 DLL (P1-HIPS-IMPL; 矩阵行
# astrocs.p1.hips_writer / astrocs_p1_hips_writer.dll)。九操作 C ABI v1
# adapter 包装 legacy HiPS writer 生产闭包 2 TU + cfitsio 源 (API-HIPS-001
# 冻结, scientific_change=false 零改动重编译; 依赖闭包实证 2026-09-07:
# writer TU 仅依赖 astrocs::healpix 两符号 + cfitsio); 唯一导出
# astrocs_module_query_v1, legacy aio_hips_* 九符号经 version-script /
# DEF 降 local (模块 CMakeLists)。
add_subdirectory(lib/algorithms/drizzle/hips)

# F-CI-002-01(CI-002 owner 裁决 2026-09-11): add_subdirectory(lib/algorithms/noise_snr)
# 断链引用解除——该子图 CMakeLists.txt 为 V7 残留面, 从未入库(P1-001 9e09941a
# 引用 untracked 文件, hosted/干净树 configure 必败, run 34601822084 实证);
# 目录保持 untracked 锁死不收编, astrocs_p1_noise target 随之不在根图
# (RPATH foreach 同步摘除; install_layout/tests/unit 均有 if(TARGET)/if(EXISTS)
# 门卫, 收编后自动重新激活)。
#
# ⚠ 2026-09-20 订正（V5 分片 5 / R-1）: 上段旧文「该子图 CMakeLists.txt 为 V7 残留面,
#   从未入库 / 目录保持 untracked 锁死不收编」**已作废**（依据: git ls-files 实测
#   + GAP_AUDIT §9.67 定案 3）。
#   ① 该子图**已在册**: `git ls-files lib/algorithms/noise_snr` = 39 文件（2026-09-20 实测;
#      A 分片报告 §4 R-1 记 40, 以实测 39 为准）, 含 CMakeLists.txt / src/module_entry.cpp;
#   ② 本 add_subdirectory 仍**保持注释**（未恢复）⇒ `astrocs_p1_noise`(SHARED, 子图 :35)
#      仍不在根图; RPATH foreach / install_layout / tests 门卫现状不变, 该段其余陈述继续有效;
#   ③ 「根图注册待 A-01 裁决后恢复」已被**更晚裁决取代**: §9.67 定案 3（2026-09-20「噪声模型 →
#      选对的那一套」）已定案 —— 公式级比对（`run/RELEASE-02/consolidate/RULING3-noise-model.md`
#      §0）判定 A = `cpp/src/noise_model.cpp` 科学正确、B = `wrapper_phase1/noise_model.cpp`
#      退役; 落地取 §9.66-B 选项 1 = **源文件直接编入 `astrocs_phase1_noise`**（见本文件
#      :620-633 NOISE-MODEL-CANON-001）, 不是恢复本子图注册。F-CI-002-01 的处置理由
#      （「引用 untracked 文件必败」）随之失据; 本子图 SHARED target 的去留归 P1-NOISE-INT 收口。

# astrocs_cpu_baseline: baseline backend DSO 技术预览 (Linux; legacy ABI,
# 无导出标注 → 默认全导出)。正式 provider ABI/Windows DLL → CPU-002
# (lib/infrastructure/benchmark/cpu/baseline)
add_library(astrocs_cpu_baseline SHARED
  lib/infrastructure/benchmark/backend_host/baseline_backend.cpp)
target_include_directories(astrocs_cpu_baseline PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/include
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/infrastructure/benchmark/backend_host
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/shared/crypto
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/third_party)
if(MSVC)
  set_property(TARGET astrocs_cpu_baseline PROPERTY WINDOWS_EXPORT_ALL_SYMBOLS ON)
else()
  target_compile_options(astrocs_cpu_baseline PRIVATE -Wall -Wextra)
endif()
# 命名对齐 install-tree contract (BLD-003): providers/astrocs_cpu_baseline.so
# (PREFIX="" 去 lib 前缀; Windows 同 astrocs_cpu_baseline.dll)
set_target_properties(astrocs_cpu_baseline PROPERTIES
  OUTPUT_NAME "astrocs_cpu_baseline"
  PREFIX "")

# RPATH (12 §6): install 后 SHARED 依赖以 $ORIGIN 解析; 禁 LD_LIBRARY_PATH 语义
# (F-CI-002-01: astrocs_p1_noise 随 snr_estimator 断链解除摘出——target 未入库)
foreach(tgt acsd_runtime acsd_io astrocs_noop astrocs_cpu_baseline astrocs_p1_drizzle astrocs_p1_calibration astrocs_p1_cosmetic astrocs_p1_hips_writer)
  if(UNIX AND NOT APPLE)
    set_target_properties(${tgt} PROPERTIES
      INSTALL_RPATH "$ORIGIN" BUILD_RPATH_USE_ORIGIN YES)
  endif()
endforeach()


add_library(astrocs_core STATIC
  lib/infrastructure/scheduler/src/artifact.cpp
  lib/infrastructure/scheduler/src/artifact_store.cpp
  lib/infrastructure/scheduler/src/module.cpp
  lib/infrastructure/scheduler/src/pipeline.cpp
  # ARCH-501: 阶段内命名块与块生命周期（CONTRACT-501 PIPELINE_BLOCK_CONTRACT）
  lib/infrastructure/scheduler/src/block_frame.cpp
  # ARCH-502: normalize 异步工作流调度器（CONTRACT-501 SCHEDULER_CONTRACT）
  lib/infrastructure/scheduler/src/normalize_workflow.cpp
  # ARCH-503: mosaic 天球窗口并行调度器（CONTRACT-501 SCHEDULER_CONTRACT）
  lib/infrastructure/scheduler/src/mosaic_window.cpp
  # ARCH-504: export 子块流式调度器（CONTRACT-501 SCHEDULER_CONTRACT）
  lib/infrastructure/scheduler/src/export_stream.cpp
  # ARCH-505: 阶段块流执行器（生产节点命名块装配）
  lib/infrastructure/scheduler/src/block_flow.cpp
  lib/infrastructure/scheduler/src/context.cpp
  lib/infrastructure/scheduler/src/runtime.cpp
  lib/infrastructure/scheduler/src/scheduler.cpp
  lib/infrastructure/scheduler/src/checkpoint.cpp
  lib/infrastructure/scheduler/src/logging.cpp
  # MEM-WIRE-01 (ARCH-AUDIT-01 B-3 = ARCH-AUDIT-02 F-04): §8.3「静态预算」的唯一实现
  # lib/infrastructure/scheduler/src/plan_estimator.cpp 此前**不在任何构建 target**
  # （根 CMake 全文无该符号）⇒ 内存回压无估算输入。本行把它编入 astrocs_core，
  # 使 Runtime::load_pipeline 能按节点 config 静态估算峰值工作集并喂给 Scheduler。
  # 依赖面：该 TU 只 include 自己的公开头 + <algorithm>/<cmath>/<limits>/<sstream>，
  # 不引入新依赖环（eng/tests/runtime/test_rt005_plan_estimator.py 静态断言同款）。
  lib/infrastructure/scheduler/src/plan_estimator.cpp
  # MEM-WIRE-01: 内存预算**来源解析**（纯函数；可用内存 × 可配置比例，默认 95）。
  # 唯一数值源 = eng/packaging/config/runtime_resources.json（经生成头消费，零字面量）。
  lib/infrastructure/scheduler/src/memory_budget.cpp
  # MEMGOV-01: 内存压力治理（ASTROCS_DESIGN.md §8.3:609-615 编排策略里
  # 「探针校正 :612 / 异步并行 :613 / 可丢弃重跑 :615」的共同实现面）。
  # 依赖面：只 include 自己的公开头 + memory_budget.h(公开头) + 生成头
  # runtime_resources_generated.h + aio_atomic_file.h(header-only 追加写原语)
  # ⇒ 不新增链接依赖，core 依赖图仍不含 cfitsio / 产品 IO 库。
  # 压力分子（进程树 RSS）经函数指针**注入**，调用方（module_adapters，已链 aio）
  # 传 aio_process_tree_rss_bytes —— core 不因此链接 astrocs_aio。
  lib/infrastructure/scheduler/src/memory_pressure.cpp
  # DET-001: 规范产品哈希（canonical product hash）—— 与
  # eng/tools/canonical_product_hash.py 逐字节同构的 C++ 实现; 进 core 使 CLI run
  # manifest 与 P3 writer/verify 节点共用同一实现（无第二份口径）。
  lib/infrastructure/scheduler/src/canonical_hash.cpp
  # RUN-PROVENANCE-01: 构建期指纹的**唯一**消费 TU。生成头显式声明为构建输入
  # （不依赖"碰巧已生成"），且只有本 TU include 它 —— 源集一变，指纹头就变，
  # 但重编译面固定为这一个小 TU + 重归档，不会波及 module_adapters/commands 等大 TU。
  ${CMAKE_CURRENT_BINARY_DIR}/build_stamp_generated.h
  lib/infrastructure/scheduler/src/build_stamp.cpp)
# 生成头在构建根目录；astrocs_core 的 PUBLIC include 面保持 lib/include 不变，
# 生成头只对本 target 可见（消费者走 astrocs/core/build_stamp.h 的访问器）。
target_include_directories(astrocs_core PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(astrocs_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/include
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/third_party)
# CLEAN-403: canonical_hash.cpp 的文件读取/存在性/头部探测/流式读取经 aio 机制
# 原语 (aio_atomic_file.h / aio_file_io.h, header-only)。仅 PRIVATE include 面:
# 不新增链接依赖, core 依赖图仍不含 cfitsio / 产品 IO 库 (原注释语义不变)。
target_include_directories(astrocs_core PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/infrastructure/aio/src
  # MEM-WIRE-01: memory_budget.cpp 消费生成头 runtime_resources_generated.h
  # （比例默认值的唯一数值源；与 resource_gate_thresholds_generated.h 同款）。
  ${CMAKE_CURRENT_BINARY_DIR})
# astrocs_common 提供 SHA-256（lib/algorithms/shared/crypto/sha256.cpp）; 无环
#（astrocs_common 不依赖 core）。core 仍不依赖 cfitsio: FITS 结构由
# canonical_hash.cpp 内的最小头解析处理, 保持 core 依赖图不含产品 IO 库。
target_link_libraries(astrocs_core PUBLIC astrocs_contracts astrocs_common Threads::Threads)
find_package(Threads REQUIRED)

# ── RELEASE-02 性能探针框架接线 ─────────────────────────────────────────────
# 单开关 ASTROCS_PROBES (默认 OFF)。OFF 时只提供头文件 include 面 (宏为空语句),
# 不编译实现、不产生符号; ON 时编译 astrocs_probes 静态库并把 ASTROCS_PROBES=1
# 沿链接闭包传给被埋点 target。头文件: astrocs/probe.h。
if(ASTROCS_PROBES)
  add_library(astrocs_probes STATIC
    lib/infrastructure/observability/probes/src/probe.cpp)
  target_include_directories(astrocs_probes PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/infrastructure/observability/probes/include
    # CLEAN-403: probe.cpp 的 sink 追加写经 aio 机制原语 (aio_atomic_file.h)。
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/infrastructure/aio/src)
  target_compile_definitions(astrocs_probes PUBLIC ASTROCS_PROBES=1)
  target_link_libraries(astrocs_probes PUBLIC Threads::Threads)
  if(NOT MSVC)
    target_compile_options(astrocs_probes PRIVATE -Wall -Wextra -Wpedantic)
  endif()
else()
  # INTERFACE: 仅登记 include 面; 源文件里的 #include "astrocs/probe.h" 在
  # 未定义 ASTROCS_PROBES 时展开为空语句, 零开销。
  add_library(astrocs_probes INTERFACE)
  target_include_directories(astrocs_probes INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/infrastructure/observability/probes/include)
endif()

# core/common: sha256/healpix (纯算法)
add_library(astrocs_common STATIC
  lib/algorithms/shared/crypto/sha256.cpp
  lib/algorithms/shared/healpix/healpix_core.cpp)
target_include_directories(astrocs_common PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/shared
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/shared/include
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/shared/crypto
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/shared/healpix)

# ── ORCH-001 批次 1: 编排层进构建图（结构性孤岛收口）─────────────────────
# 事实（改前）: 根 CMake 对 orchestrator 命中 0，全仓只有 run/** 下的历史副本提到
# 它 ⇒ lib/infrastructure/pipeline/orchestrator/cpp/** 从未进任何构建图，
# SAT-001（claim SC-008 饱和电平解析）与 MASK-002（claim SC-007 逐星掩膜输入）
# 在 orchestrator.cpp 的改动从未被编译过。
# 归属: ASTROCS_DESIGN §7.1 逐字「scheduler/ 注册、资源预算、执行、取消、checkpoint」
# 与本目录职责面逐条对应 ⇒ 职责家 = scheduler/**；物理位 = ARCH-001 迁移清单 #15
# （登记在 pipeline/），位置与职责冲突登记 ORCH-HOME-01 交前台裁决。
# target: astrocs_infra_orchestrator（MODULE_MAP conventions: astrocs_infra_<id>）;
# 依赖闭包见 lib/infrastructure/pipeline/orchestrator/cpp/CMakeLists.txt。
# 唯一静态依赖 = astrocs_common（sha256 + healpix_core），故置于其后。
# 注: 共址测试子目录在文末 ASTROCS_BUILD_TESTS 段（enable_testing() 之后）注册。
add_subdirectory(lib/infrastructure/pipeline/orchestrator/cpp)

# vendored cfitsio (第三方, SYSTEM 隔离; 不参与模块图)
add_library(astrocs_cfitsio STATIC ${ASTROCS_CFITSIO_SOURCES})
target_include_directories(astrocs_cfitsio PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/infrastructure/aio/third_party/cfitsio)
if(MSVC)
  # WIN-001: cfitsio 依赖适配 — 源码 `-w` 为 GCC 旗标 (MSVC 忽略并告警 D9002);
  # _REENTRANT 需 pthread.h → 用 eng/cmake/win32_pthread_shim (SRWLOCK 映射, MSVC 专用);
  # zcompress.c 需 zlib (ACS_ZLIB_ROOT, MSVC 编译的 zlib 安装, 见 fatduck C:/Users/fujia/zlib-msvc)。
  # 注意: 不可用 MSYS2 mingw64 include (gcc 头无法被 MSVC 编译)。
  target_compile_options(astrocs_cfitsio PRIVATE /w)
  target_include_directories(astrocs_cfitsio PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/eng/cmake/win32_pthread_shim)
  target_link_libraries(astrocs_cfitsio PUBLIC kernel32)  # SRWLock API (shim)
  set(ACS_ZLIB_ROOT "$ENV{ACS_ZLIB_ROOT}" CACHE PATH "MSVC zlib install root (lib/include/zlib.h + lib/<zlib>.lib)；hosted runner 经 vcpkg 注入 env")
  if(ACS_ZLIB_ROOT)
    # HOSTFIX-23④ (run 34039050194): env 层确定性优先 —— hosted runner 的
    # "Provide zlib (vcpkg)" 步骤列出 installed/x64-windows-static-md/lib 下
    # 首个 zlib 匹配 *.lib 并以 GITHUB_ENV ACS_ZLIB_LIB 注入文件全名，
    # 直接拼路径命中，find_library 仅作回退探测。
    if(ENV{ACS_ZLIB_LIB})
      set(ACS_ZLIB_LIBRARY "${ACS_ZLIB_ROOT}/lib/$ENV{ACS_ZLIB_LIB}")
    endif()
    # 库文件名跨来源不同：fatduck 本机 zlib-msvc 布局为 libz.lib；
    # vcpkg zlib 1.3.2 x64-windows-static-md (上游 madler/zlib v1.3.2
    # CMakeLists: zlibstatic OUTPUT_NAME z${zlib_static_suffix}, WIN32 下
    # suffix="s") 实际产物为 zs.lib —— NAMES 覆盖 z/libz/zlib/zlibstatic/zs。
    find_library(ACS_ZLIB_LIBRARY NAMES z libz zlib zlibstatic zs
      HINTS "${ACS_ZLIB_ROOT}/lib" NO_DEFAULT_PATH)
    if(NOT ACS_ZLIB_LIBRARY)
      message(FATAL_ERROR "ACS_ZLIB_ROOT=${ACS_ZLIB_ROOT} 下未找到 zlib 链接库 (z libz zlib zlibstatic zs; 或经 ACS_ZLIB_LIB 注入确切文件名)")
    endif()
  endif()
  if(ACS_ZLIB_ROOT)
    target_include_directories(astrocs_cfitsio PRIVATE "${ACS_ZLIB_ROOT}/lib/include")
    target_link_libraries(astrocs_cfitsio PUBLIC "${ACS_ZLIB_LIBRARY}")
  endif()
else()
  target_compile_options(astrocs_cfitsio PRIVATE -w)
endif()
# IO-003: CFITSIO 显式 reentrant 构建 (fits_is_reentrant() 必须返回 1)
target_compile_definitions(astrocs_cfitsio PRIVATE _REENTRANT)

# io/astro_image_io: 图像/HiPS I/O adapter (aio_pipeline* 保留待 LEG-003 退出)
# MOD-001 (F-AIO-003 生产接线): aio_abi.cpp (AIO-001 交付的唯一 AIO C ABI v1
# 实现, aio_abi_query_v1/aio_content_hash_*_v1) 自本任务起编入生产 target
# astrocs_aio (AIO 正统归属; 此前仅测试面直编, 生产接线缺口登记于 HANDOVER
# F-AIO-003 → MOD-001)。仓库根 lib/include/ 以 PRIVATE 引入 (仅本 target 编译
# aio_abi.cpp 需要 astrocs/io/aio_abi_v1.h, 不扩大下游 include 面)。
add_library(astrocs_aio STATIC
  lib/infrastructure/aio/src/aio_fits.cpp
  lib/infrastructure/aio/src/aio_api.cpp
  lib/infrastructure/aio/src/aio_log.cpp
  lib/infrastructure/aio/src/aio_compressor.cpp
  lib/infrastructure/aio/src/aio_xisf.cpp
  lib/infrastructure/aio/src/aio_pipeline.cpp
  # AIO-SYSINFO-01: 可用内存探测收进 aio 边界 (§10「aio 是文件级唯一 I/O 边界」
  # + §9.73 裁决 U5); 原 scheduler 内直读 /proc/meminfo 的 fopen 已删除。
  lib/infrastructure/aio/src/aio_sysinfo.cpp
  lib/infrastructure/aio/src/aio_abi.cpp)
target_include_directories(astrocs_aio PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/include)
target_include_directories(astrocs_aio PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/infrastructure/aio/include
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/infrastructure/aio/src
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/infrastructure/aio/third_party/cfitsio
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/shared
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/third_party)
target_compile_definitions(astrocs_aio PRIVATE AIO_ENABLE_FITS AIO_ENABLE_XISF)
if(MSVC AND ACS_ZLIB_LIBRARY)
  target_link_libraries(astrocs_aio PUBLIC astrocs_common astrocs_cfitsio "${ACS_ZLIB_LIBRARY}")
else()
  target_link_libraries(astrocs_aio PUBLIC astrocs_common astrocs_cfitsio z)
endif()
# QA-001: aio_fits.cpp 含 omp pragma → -fopenmp 编译消除 unknown-pragmas
if(UNIX AND OpenMP_CXX_FOUND)
  target_compile_options(astrocs_aio PRIVATE -fopenmp)
  target_link_libraries(astrocs_aio PRIVATE OpenMP::OpenMP_CXX)
endif()

# io/hips: HiPS reader/writer + hiss (无科学积分)
add_library(astrocs_hips STATIC
  lib/infrastructure/aio/src/hips/aio_hips_reader.cpp
  lib/infrastructure/aio/src/hips/aio_hips_writer.cpp
  lib/infrastructure/aio/src/healpix/aio_healpix_io.cpp
  lib/infrastructure/aio/src/hiss_codec.cpp
  lib/infrastructure/aio/src/hiss_common.cpp
  lib/infrastructure/aio/src/hiss_reader.cpp
  lib/infrastructure/aio/src/hiss_stream_writer.cpp
  lib/infrastructure/aio/src/hiss_tile_model.cpp
  lib/infrastructure/aio/src/hiss_transform.cpp
  lib/infrastructure/aio/src/hiss_writer.cpp
  lib/infrastructure/aio/src/aio_upm.cpp)
target_include_directories(astrocs_hips PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/infrastructure/aio/include
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/infrastructure/aio/src
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/infrastructure/aio/third_party/cfitsio
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/shared
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/shared/healpix)
target_compile_definitions(astrocs_hips PRIVATE AIO_ENABLE_FITS AIO_ENABLE_HEALPIX)
target_link_libraries(astrocs_hips PUBLIC astrocs_aio astrocs_common)

# lib/algorithms/calibration
add_library(astrocs_calibration STATIC
  lib/algorithms/calibration/src/calibrator.cpp
  lib/algorithms/calibration/src/master_generator.cpp
  lib/algorithms/calibration/src/cosmetic_corrector.cpp
  # RELEASE-02 FIX-P1 (P1-1): apply_photometry（I_photo = k_photo·I_cal,
  # 02_FROZEN_STAGE1_HISS_SPEC §7 历史锚 / docs/algorithms/CALIBRATION_ALGORITHMS.md §3.6）
  # 此前只在测试 target 编译、生产零调用者 ⇒ Phase1 测光归一化从未应用到像素。
  # 编入生产库后由 Phase1 photometry 节点调用（p1_op_photometry）。
  lib/algorithms/calibration/src/photometry_apply.cpp
  lib/algorithms/calibration/src/ac_api.cpp)
target_include_directories(astrocs_calibration PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/calibration/include
  # FIX-P1: photometry_apply.h 供 Phase1 节点直接 include（扁平头名, 无冲突）
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/calibration/src
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/infrastructure/aio/include)
target_link_libraries(astrocs_calibration PUBLIC astrocs_aio astrocs_common)
# QA-001: calibrator/cosmetic_corrector/master_generator 含 omp pragma → -fopenmp
if(UNIX AND OpenMP_CXX_FOUND)
  target_compile_options(astrocs_calibration PRIVATE -fopenmp)
  target_link_libraries(astrocs_calibration PRIVATE OpenMP::OpenMP_CXX)
endif()

# PHOT-MXY-01: 仓内**唯一**「可辨识性/病态」判据 (identifiability.cpp) 抽成独立
# 小库, 供 astrocs_phase2 (天光面/UPM) 与 astrocs_phase1_photcal (Phase1 低阶空间
# 增益 m(x,y) 的基函数秩检验) **共用同一份编译产物** —— 不复制源码、不另立阈值。
# 依据: docs/science/PHASE2_UPM.md §7a:196-198（判据按矩阵谱自身定）;
#       lib/algorithms/coverage/include/astro/phase2/identifiability.h 文件头;
#       CODE_STANDARD §MUST「禁止重复 production science implementation」。
add_library(astrocs_identifiability STATIC
  lib/algorithms/coverage/src/identifiability.cpp)
target_include_directories(astrocs_identifiability PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/coverage/include)
target_link_libraries(astrocs_identifiability PUBLIC astrocs_contracts astrocs_common)

# lib/algorithms/coverage (phase2 生产模块; ACR 源不在此列 — LEG-004)
add_library(astrocs_phase2 STATIC
  lib/algorithms/coverage/src/upm.cpp
  lib/algorithms/coverage/src/sky_plane.cpp
  lib/algorithms/coverage/src/stage2_common.cpp
  lib/algorithms/coverage/src/rejection.cpp
  lib/algorithms/coverage/src/coverage.cpp
  lib/algorithms/coverage/src/sampler.cpp
  lib/algorithms/coverage/src/block.cpp
  lib/algorithms/coverage/src/integrate.cpp
  lib/algorithms/coverage/src/cuda_bridge_stub.cpp)
target_include_directories(astrocs_phase2 PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/coverage/include
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/infrastructure/acr/backends/cuda/bridge
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/infrastructure/aio/include
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/infrastructure/aio/src
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/infrastructure/aio/third_party/cfitsio
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/shared)
target_link_libraries(astrocs_phase2 PUBLIC astrocs_contracts astrocs_common astrocs_aio astrocs_hips
  # PHOT-MXY-01: 唯一可辨识性判据实现 (identifiability.cpp, 见 astrocs_identifiability)
  astrocs_identifiability
  # RELEASE-02 探针 (OFF 时仅 include 面, 零开销)
  astrocs_probes)

# lib/algorithms/coverage · Phase3 HIPS properties 解析器 (W8 / W4-A9 批次 4)
# 原 lib/phase3_session/hips_properties.{h,cpp} (ASTROCS_DESIGN §7.1 无独立 hips 行;
# coverage 承接 HIPS 产品结构面 —— 前台 2026-09-17 裁决)。独立成库以解
# 「astrocs_p3_rsmp ⇄ astrocs_phase3_session」链接环: 该解析器不依赖任何 phase3
# 目标 (头仅 <cstddef>/<string>), 由 resample 模块与会话库同时依赖。
# 根声明 (非 coverage/CMakeLists.txt) 与 astrocs_phase2 同例, 遵 BLD-002。
if(NOT TARGET astrocs_hips_properties)
  add_library(astrocs_hips_properties STATIC
    lib/algorithms/coverage/hips_properties.cpp)
  target_include_directories(astrocs_hips_properties PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/coverage
    # CLEAN-403: hips_properties.cpp 的目录枚举/整文件读取经 aio 机制原语
    # (aio_atomic_file.h / aio_file_io.h, header-only) ⇒ 需要 aio/src include 面。
    # 仅 include 面, 不新增链接依赖 (aio_file_io.h 的 sha256 来自 astrocs_common)。
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/infrastructure/aio/src)
  target_link_libraries(astrocs_hips_properties PUBLIC astrocs_common)
  set_target_properties(astrocs_hips_properties PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON)
endif()

# lib/algorithms/drizzle/healpix_drizzle
add_library(astrocs_drizzle STATIC
  lib/algorithms/drizzle/healpix_drizzle/fits_reader.cpp
  lib/algorithms/drizzle/healpix_drizzle/wcs_sip.cpp
  lib/algorithms/drizzle/healpix_drizzle/poly_clip.cpp
  lib/algorithms/drizzle/healpix_drizzle/spherical_overlap.cpp
  lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp
  lib/algorithms/drizzle/healpix_drizzle/reverse_drizzle.cpp
  lib/algorithms/drizzle/healpix_drizzle/hp_drizzle_api.cpp
  lib/algorithms/drizzle/healpix_drizzle/hp_drizzle_hips_api.cpp
  lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp
  lib/algorithms/drizzle/healpix_drizzle/snr_evaluator.cpp)
target_include_directories(astrocs_drizzle PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/drizzle/healpix_drizzle
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/infrastructure/aio/include
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/infrastructure/aio/src
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/infrastructure/aio/third_party/cfitsio
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/shared
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/shared/healpix)
# QA-001: nanoflann.hpp (BSD-3, vendored 第三方) 经本 target 引号 include 引入;
# 仅对该第三方头做 -isystem 隔离, drizzle 自有 .cpp 仍参与严格 warning 分层.
target_include_directories(astrocs_drizzle SYSTEM PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/drizzle/healpix_drizzle)
target_compile_definitions(astrocs_drizzle PRIVATE AIO_ENABLE_FITS AIO_ENABLE_HEALPIX)
target_link_libraries(astrocs_drizzle PUBLIC astrocs_contracts astrocs_common astrocs_aio astrocs_hips)
# QA-001: drizzle_engine/snr_evaluator 含 omp pragma → -fopenmp 编译消除 unknown-pragmas
# (omp 并行仅为遗留 drizzle 内部; 生产调度走 Runtime lease, 见 MON/CHK 系列)
if(UNIX AND OpenMP_CXX_FOUND)
  target_compile_options(astrocs_drizzle PRIVATE -fopenmp)
  target_link_libraries(astrocs_drizzle PRIVATE OpenMP::OpenMP_CXX)
endif()

# backends/cpu: provider ABI + baseline (AVX2/512 见 CPU-003/004)
add_library(astrocs_cpu STATIC
  lib/infrastructure/benchmark/backend_host/cpu_features.cpp
  lib/infrastructure/benchmark/backend_host/hardware_inspect.cpp
  lib/infrastructure/benchmark/backend_host/backend_loader.cpp
  lib/infrastructure/benchmark/backend_host/baseline_backend.cpp
  lib/infrastructure/benchmark/backend_host/profile_gen.cpp
  lib/infrastructure/benchmark/backend_host/profile_gen_v2.cpp
  lib/infrastructure/benchmark/backend_host/cpu_routing.cpp
  lib/infrastructure/benchmark/backend_host/host_services.cpp
  lib/infrastructure/benchmark/backend_host/bench_harness.cpp
  lib/infrastructure/benchmark/backend_host/bench_report.cpp
  lib/infrastructure/benchmark/backend_host/profile_store.cpp
  lib/infrastructure/benchmark/backend_host/worker_advisor.cpp)
target_include_directories(astrocs_cpu PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/infrastructure/benchmark/backend_host
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/shared/crypto
  # CLEAN-403: backend_host 各 TU 的文件读写经 aio 机制原语 (header-only)。
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/infrastructure/aio/src
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/shared
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/third_party)
target_link_libraries(astrocs_cpu PUBLIC astrocs_contracts astrocs_common)

# CPU-001 (G3): 独立 provider 变体 target — 真实独立编译单元 (05 §2)
# 编译隔离合同: -mavx2/-mfma 等高级旗标仅作用于对应 provider target(PRIVATE),
# 不污染 astrocs_cpu/baseline 与主 CLI; 主 CLI 保持最低 amd64/SSE2 (ARCH-003 §2)。
# 变体与 baseline 共享 baseline_kernels_impl.inc/backend_table.inc 同一源(零复制漂移),
# 仅编译旗标与 ASTROCS_BACKEND_ID 不同 → 每条路径独立编译成不同目标并各自 self-test。
# 注意: acsd(主 CLI) 的链接闭包只含基线段 — 两个变体 target 由 check_isa_leak 的 link map 验证把关。
add_library(astrocs_cpu_avx2 STATIC lib/infrastructure/benchmark/backend_host/avx2_backend.cpp)
target_include_directories(astrocs_cpu_avx2 PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/infrastructure/benchmark/backend_host
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/shared/crypto)
target_compile_options(astrocs_cpu_avx2 PRIVATE -mavx2 -mfma)
target_link_libraries(astrocs_cpu_avx2 PUBLIC astrocs_contracts astrocs_common)

add_library(astrocs_cpu_avx512 STATIC lib/infrastructure/benchmark/backend_host/avx512_backend.cpp)
target_include_directories(astrocs_cpu_avx512 PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/infrastructure/benchmark/backend_host
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/shared/crypto)
# AVX512 实际使用子集: F + DQ + BW + VL (ISA-004, 与 cpu_features.h 检测面一致)
target_compile_options(astrocs_cpu_avx512 PRIVATE
  -mavx512f -mavx512bw -mavx512vl -mavx512dq)
target_link_libraries(astrocs_cpu_avx512 PUBLIC astrocs_contracts astrocs_common)

# phase1 wcs + photometry (P1-004)
add_library(astrocs_phase1_wcs STATIC lib/algorithms/platesolve/wrapper_phase1/wcs_tan.cpp)
target_include_directories(astrocs_phase1_wcs PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/platesolve/wrapper_phase1 ${CMAKE_CURRENT_SOURCE_DIR}/lib/include)
add_library(astrocs_phase1_phot STATIC lib/algorithms/photometry/wrapper_phase1/photometer.cpp)
target_include_directories(astrocs_phase1_phot PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/photometry/wrapper_phase1 ${CMAKE_CURRENT_SOURCE_DIR}/lib/include)
target_link_libraries(astrocs_phase1_phot PUBLIC astrocs_contracts)

# RELEASE-02 FIX-P1 (P1-1): Phase1 测光定标生产库 astrocs_phase1_photcal
# ----------------------------------------------------------------------------
# 缺陷: 会做 I_photo=k_photo·I_cal 的 pc_calibrate_simple* 只在测试 target
# (lib/algorithms/photometry/tests/p1phot/CMakeLists.txt:60-68) 编译; 生产
# photometric_calib.dll 未编入根构建 ⇒ Phase1 节点取不到 star_matcher 的
# Tukey-IRLS 标量。本目标把 5 个生产 TU 零 diff 编入主图（与 p1phot 测试
# 同一组源），供 p1_op_photometry 经 frame_photometry_fit 调用。
# gaia_client 符号由 astrocs_p1_ipv（已含 gaia_client.c）提供。
add_library(astrocs_phase1_photcal STATIC
  lib/algorithms/photometry/cpp/src/pc_api.cpp
  lib/algorithms/photometry/cpp/src/star_matcher.cpp
  lib/algorithms/photometry/cpp/src/wcs_transform.cpp
  lib/algorithms/photometry/cpp/src/image_corrector.cpp
  lib/algorithms/photometry/cpp/src/spectrum_integrator.cpp
  lib/algorithms/photometry/cpp/src/frame_photometry_fit.cpp
  # PHOT-MXY-01: 低阶乘性空间增益 m(x,y) 的拟合 (SCI-PHOT-001 §16.1 ④)
  lib/algorithms/photometry/cpp/src/spatial_gain.cpp)
target_include_directories(astrocs_phase1_photcal PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/photometry/cpp/src
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/photometry/cpp/include
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/infrastructure/gaia_xpsd_client/src
  # CLEAN-403: frame_photometry_fit.cpp 的 filters.json 读取经 aio 机制原语
  # (aio_file_io.h, header-only) ⇒ 需要 aio/src 与 shared include 面。
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/infrastructure/aio/src
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/shared
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/include)
# PHOT-MXY-01 追加依赖:
#   · astrocs_calibration —— 只为 photometry_apply.h 的**唯一**空间场求值实现
#     (基函数 + m(x,y) 内联求值) 与 apply_photometry_spatial; 无反向依赖。
#   · astrocs_identifiability —— 空间基函数的可辨识性判据 (唯一实现)。
target_link_libraries(astrocs_phase1_photcal PUBLIC astrocs_contracts astrocs_p1_ipv
  astrocs_calibration astrocs_identifiability)
if(UNIX AND OpenMP_CXX_FOUND)
  target_link_libraries(astrocs_phase1_photcal PUBLIC OpenMP::OpenMP_CXX)
endif()

# phase1 noise (P1-005 SCI-NOISE-001)
# P8-SNR-LINUX (2026-09-14): Linux 生产路径 SNR 接线 —— 把 P5-SNR 唯一权威科学实现
# lib/algorithms/noise_snr/cpp/src/snr_science.cpp 编入本目标。帧级聚合在
# lib/algorithms/noise_snr/wrapper_phase1/snr_frame_science.cpp (零公式副本, 只调 snr_science C ABI)。
#
# NOISE-MODEL-CANON-001 (2026-09-20, 负责人 §9.67 定案 3「选对的」):
#   `cpp/src/noise_model.cpp` (A) 是冻结 SCI-NOISE-001 §5/§5a 的唯一实现
#   (docs/science/NOISE_MODEL.md:71/165 锚定; ALG §13.1:120「生产符号唯一源」)。
#   原 `wrapper_phase1/noise_model.cpp` (B) 是 A 的**退化子集**
#   (ALG §13.5:257-262「退化子集」= 无掩膜/无裁剪/无饱和/无网格/无平面场/无预算门的整帧兜底),
#   其公式在自述域(空白背景像素集)内正确, 但生产调用点曾把**整帧**喂入
#   ⇒ 有星时整帧 MAD 偏差 +2.6%..+21.6% (超 SCI §7 的 2% 与 §11 的 5% oracle),
#   而 A 全算法 <=0.35% (独立复现, 3 seed)。B 已退役, 不再编入。
#   逐像素 variance/ivar 产品面也只有 A 能给 (hp_drizzle_api.cpp:1018-1047 的 variance 块合同)。
#   符号冲突核查: 全仓 snr_noise_model_v1* 定义仅 A 一处 ⇒ 无重复定义。
add_library(astrocs_phase1_noise STATIC
  lib/algorithms/noise_snr/cpp/src/noise_model.cpp
  lib/algorithms/noise_snr/wrapper_phase1/snr_frame_science.cpp
  lib/algorithms/noise_snr/cpp/src/snr_science.cpp)
target_include_directories(astrocs_phase1_noise PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/noise_snr/wrapper_phase1
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/noise_snr/cpp/include
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/noise_snr/include
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/include)
target_link_libraries(astrocs_phase1_noise PUBLIC astrocs_contracts)
# P10-UTIL2-007: 噪声统计 (median/MAD 次序统计量选择) 与逐源 SNR 聚合使用 OpenMP。
# 无 OpenMP 时 #pragma omp 被忽略 -> 串行回退且结果逐位不变; 线程数不在本库
# 硬编码 (宪章 §10.4), 由 Runtime 注入的 OpenMP 环境决定。与
# astrocs_phase1_stars / astrocs_p1_sdet / astrocs_p1_dpsf 同款接线。
if(UNIX AND OpenMP_CXX_FOUND)
  target_link_libraries(astrocs_phase1_noise PUBLIC OpenMP::OpenMP_CXX)
endif()

# phase1 stars (P1-003 StarDetector)
add_library(astrocs_phase1_stars STATIC lib/algorithms/star_detection/wrapper_phase1/star_detector.cpp)
target_include_directories(astrocs_phase1_stars PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/star_detection/wrapper_phase1
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/include)
target_link_libraries(astrocs_phase1_stars PUBLIC astrocs_contracts)
# P2 (PSF-DET-001/PSF-BG-001): 检测器扫描/逐像素转换用 OpenMP。无 OpenMP 时
# #pragma omp 被忽略 → 串行回退且结果逐位不变; 线程数不在本库硬编码
# (宪章 §10.4), 由 Runtime 注入的 OpenMP 环境决定。与 astrocs_p1_sdet:548 同款。
if(UNIX AND OpenMP_CXX_FOUND)
  target_link_libraries(astrocs_phase1_stars PUBLIC OpenMP::OpenMP_CXX)
endif()

# P1-001 (attempt 2) + RESCUE-FD-05: B 线真实求解器/拟合器接线——
# 生产源零 diff 只读编译进 STATIC（ipv 13 TU + gaia_client.c(C) + dpsf 3 TU）；
# 两平台均为真实求解: Windows 经 LoadLibraryA 解析 DLL, 非 Windows (Linux amd64
# 正式入口) 经 ipv_select.cpp 直接绑定同一组静态链接 C API。源内已无平台 stub。
set(P1_IPV_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/platesolve/cpp/ipv)
add_library(astrocs_p1_ipv STATIC
  ${P1_IPV_DIR}/src/ipv_entry.cpp ${P1_IPV_DIR}/src/ipv_solver.cpp
  ${P1_IPV_DIR}/src/ipv_select.cpp ${P1_IPV_DIR}/src/ipv_angle.cpp
  ${P1_IPV_DIR}/src/ipv_distortion.cpp ${P1_IPV_DIR}/src/ipv_itertrans.cpp
  ${P1_IPV_DIR}/src/ipv_kvector.cpp ${P1_IPV_DIR}/src/ipv_polygon.cpp
  ${P1_IPV_DIR}/src/ipv_ransac.cpp ${P1_IPV_DIR}/src/ipv_robust_refine.cpp
  ${P1_IPV_DIR}/src/ipv_sip.cpp ${P1_IPV_DIR}/src/ipv_triangle.cpp
  ${P1_IPV_DIR}/src/ipv_wcs.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/infrastructure/gaia_xpsd_client/src/gaia_client.c)
target_include_directories(astrocs_p1_ipv PUBLIC
  ${P1_IPV_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/infrastructure/gaia_xpsd_client/src
  # RESCUE-FD-05: 非 Windows 直连 C API 需要生产头面 (与 DLL 导入等价的 ABI 面)
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/star_detection/include
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/infrastructure/aio/include
  # CLEAN-403: ipv_log.h 的日志追加写经 aio 机制原语 (aio_atomic_file.h)。
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/infrastructure/aio/src
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/include)
target_link_libraries(astrocs_p1_ipv PUBLIC z Threads::Threads)
set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/lib/infrastructure/gaia_xpsd_client/src/gaia_client.c
  PROPERTY C_STANDARD 11)

add_library(astrocs_p1_dpsf STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/psf/src/dpsf_psf.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/psf/src/dpsf_image.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/psf/src/dpsf_log.cpp)
target_include_directories(astrocs_p1_dpsf PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/psf/include
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/psf/src
  # CLEAN-403: dpsf_log.cpp 的日志追加写经 aio 机制原语 (aio_atomic_file.h)。
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/infrastructure/aio/src)
target_link_libraries(astrocs_p1_dpsf PUBLIC OpenMP::OpenMP_CXX)

# P1-001: sdet 生产实现（lib/algorithms/star_detection 9 TU, ipv detector_handle;
# 规范权威源 = docs/algorithms/STAR_DETECTION_ALGORITHMS.md:8「唯一权威生产源」,
# 生产检测器切换属 D-04 裁决面）
# ⚠ 2026-09-20 订正（V5 分片 5 / R-8）: 上段旧文「star-psf 检测的真实来源; …
#   wrapper_phase1 仅为薄包装, 真实检测在此」**已作废/失实**（依据 GAP_AUDIT §9.48②
#   `:1404-1414` 同帧实测, M42 4096² 同参数）—— 两者是**两套独立盲检测算法**,
#   不是包装关系:
#   · wrapper_phase1 = `astrocs::phase1::StarDetector`（P1-003 桥接类; 现生产 star-psf
#     节点实际调用, 见 lib/infrastructure/scheduler/src/module_adapters.cpp:5-7, :2102）
#     ⇒ 345,960 条 / Gaia 外部纯度 0.212%（中位距离 56.1 px）/ 完备性 SNR 5–10 = 93.7%;
#   · sdet（本 target）= 1,473 条 / 纯度 45.553%（中位 1.1 px）/ 完备性 44.4%,
#     当前被 wcs-platesolve 节点使用（同文件 :2864 sdet_create, sp.maxStars=2000）。
#   ⇒ 交付源 99.67%（344,808 条）无 sdet 对应; 下游已污染（`p1_snr.json`
#   n_snr_input=345,960, 参考通量中位建立在非星结构上）。本 target 的编译面**不变**
#   （下方 add_library 原样）; 「生产检测器权威源」择一归 D-04
#   （GAP_AUDIT §9.67 前台须执行 #3 登记为已裁决, 落地另行）。
add_library(astrocs_p1_sdet STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/star_detection/src/sdet_api.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/star_detection/src/sdet_detector.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/star_detection/src/sdet_image.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/star_detection/src/sdet_log.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/star_detection/src/sdet_background.cpp
  # GSL-REPLACE-01: 自研信赖域 LM 求解器（替代外部 GSL gsl_multifit_nlinear）
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/star_detection/src/nls_lm.cpp)
target_include_directories(astrocs_p1_sdet PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/star_detection/include
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/star_detection/src
  # CLEAN-403: sdet_log.cpp 的日志追加写经 aio 机制原语 (aio_atomic_file.h)。
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/infrastructure/aio/src)
target_link_libraries(astrocs_p1_sdet PUBLIC OpenMP::OpenMP_CXX)
# GSL-REPLACE-01: PSF 拟合的 trust-region LM 求解器改为仓内自研实现
# （lib/algorithms/star_detection/src/nls_lm.cpp, 无外部依赖）。
# **禁止**重新引入 GSL/gslcblas: Windows 无来源, 且其为 GPL 族并经
# astrocs_p1_sdet → astrocs_module_adapters → astrocs 进入产品 exe 动态链。
target_link_libraries(astrocs_p1_sdet PUBLIC m)

# phases sessions (facade, 后续迁移为 preset)
add_library(astrocs_phase1_session STATIC lib/phase1_session/p1_session.cpp)
target_include_directories(astrocs_phase1_session PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/phase1_session
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/calibration/include)
# P1-001 (attempt 2): 原 phase1 科学实现源(只读委托, 源零修改; ARCH-001 迁移后位于
# lib/algorithms/{noise_snr,photometry,star_detection,platesolve}/wrapper_phase1) 经本 PUBLIC
# 链接闭包传递给 astrocs_module_adapters(B 线真实节点化构建接线; ARCH-P0-001)。
target_link_libraries(astrocs_phase1_session PUBLIC astrocs_contracts astrocs_calibration astrocs_aio
  astrocs_phase1_wcs astrocs_phase1_phot astrocs_phase1_noise astrocs_phase1_stars
  # RELEASE-02 探针 (OFF 时仅 include 面, 零开销)
  astrocs_probes)

add_library(astrocs_phase2_session STATIC lib/phase2_session/p2_session.cpp)
target_include_directories(astrocs_phase2_session PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/phase2_session
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/coverage/include)
target_link_libraries(astrocs_phase2_session PUBLIC astrocs_contracts astrocs_phase2
  # RELEASE-02 探针 (OFF 时仅 include 面, 零开销)
  astrocs_probes)

add_library(astrocs_phase3_session STATIC
  lib/phase3_session/p3_session.cpp
  # W4-A9 批次 1: p3_wcs.cpp 已迁 lib/algorithms/projection/ (ASTROCS_DESIGN §7.1
  # 「projection」行; 由 add_subdirectory(lib/algorithms/projection) 的
  # astrocs_p3_projection_wcs 声明, 经本库 target_link_libraries 进闭包)。
  # W4-A9 批次 3: p3_output.cpp 已迁 lib/algorithms/fits_output/ (同 §7.1「fits_output」
  # 行; astrocs_p3_fits_output 声明, 经下方链接闭包进产品图)。
  # W8 (批次 4): p3_resample.cpp 编译单元归位 astrocs_p3_rsmp (resample 模块, 见
  # lib/algorithms/resample/CMakeLists.txt); hips_properties.cpp 迁
  # lib/algorithms/coverage/ 独立成库 astrocs_hips_properties (解环, 见上)。
  )
target_include_directories(astrocs_phase3_session PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/phase3_session
  # W4-A9 批次 1: p3_wcs.h 迁 projection; 会话层 p3_session.cpp / p3_output.h 仍以
  # 扁平 "p3_wcs.h" 引用 ⇒ 登记新址 include 面 (会话层退场时随批次 3 收敛)。
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/projection
  # W4-A9 批次 2: p3_resample.h 迁 resample; 会话层与既有测试仍以扁平
  # "p3_resample.h" 引用 ⇒ 登记新址 include 面 (随批次 4 收口)。
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/resample
  # W4-A9 批次 3: p3_output.h 迁 fits_output; 会话层与既有测试仍以扁平
  # "p3_output.h" 引用 ⇒ 登记新址 include 面 (会话层退场时收敛)。
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/fits_output
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/shared
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/shared/crypto
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/shared/healpix
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/infrastructure/aio/include
  ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(astrocs_phase3_session PUBLIC astrocs_contracts astrocs_aio astrocs_common
  astrocs_p3_projection_wcs astrocs_p3_fits_output astrocs_p3_rsmp astrocs_hips_properties)

# lib/infrastructure/cli: 子命令模块注册 (INT-001 W4-A9 接线步 1/3)。
# CLI-001 交付的 header-only 命令编排层 (command_tree/session_commands/subcommand +
# normalize|mosaic|export 三子目录) 在此登记为 INTERFACE target
# (astrocs_cli_subcommands 及三个子命令 target); 声明见
# lib/infrastructure/cli/CMakeLists.txt (BLD-002: 子目录文件非产品事实源,
# 唯一 project()/唯一 add_executable(acsd) 仍在本文件)。
# 置于 acsd target 之前: INTERFACE target 无源编译, 仅提供 include 面。
add_subdirectory(lib/infrastructure/cli)

# app/astrocs_cli: 薄 CLI (显式链接各库; 无 GLOB; ACR 不链)
# RT-008: main.cpp 仅入口壳; parser/commands/runtime_client 为独立编译单元
# B8-P1: lib/infrastructure/cli/process.cpp — 跨平台子进程封装(argv 传参, 替代 std::system 拼接族),
#   同时以 astrocs_cli_process 静态库形式供 eng/tests/unit 共址单测链接。
add_library(astrocs_cli_process STATIC lib/infrastructure/cli/process.cpp)
target_include_directories(astrocs_cli_process PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/infrastructure/cli
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/third_party)

add_executable(acsd
  # G-RES-01 生成头显式声明为构建输入（不依赖"碰巧已生成"）。
  ${CMAKE_CURRENT_BINARY_DIR}/resource_gate_thresholds_generated.h
  lib/infrastructure/cli/main.cpp
  lib/infrastructure/cli/parser.cpp
  lib/infrastructure/cli/commands.cpp
  lib/infrastructure/cli/process.cpp
  lib/infrastructure/cli/runtime_client.cpp)
target_include_directories(acsd PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/infrastructure/cli
  # INT-001 W4-A9 接线步 2/3: CLI 子命令模块四个 include 目录。前三个供
  # commands.cpp 的 normalize/normalize.h 等引号包含解析, 第 4 个为其上级目录;
  # astrocs_cli_subcommands 已携带同一组 INTERFACE include (此处显式登记,
  # 使 include 面在 target_link_libraries 之外亦可独立审计)。
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/infrastructure/cli/normalize
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/infrastructure/cli/mosaic
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/infrastructure/cli/export
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/infrastructure/aio/src
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/infrastructure/aio/include
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/infrastructure/aio/third_party/cfitsio
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/drizzle/healpix_drizzle
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/shared
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/shared/crypto
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/shared/healpix
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/coverage/include
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/projection)
target_link_libraries(acsd PRIVATE
  astrocs_contracts astrocs_core astrocs_cli_runtime astrocs_common astrocs_cfitsio astrocs_aio astrocs_hips
  astrocs_p3_projection_wcs
  astrocs_calibration astrocs_phase2 astrocs_drizzle astrocs_cpu
  astrocs_phase1_session astrocs_phase2_session astrocs_phase3_session
  # INT-001 W4-A9 接线步 3/3: CLI 子命令模块 target (INTERFACE, 仅 include 面;
  # normalize|mosaic|export 三子目录的扁平头名经此解析)。
  astrocs_cli_subcommands
  ${CMAKE_DL_LIBS})
if(MSVC)
  if(ACS_ZLIB_LIBRARY)
    target_link_libraries(acsd PRIVATE "${ACS_ZLIB_LIBRARY}")
  endif()
else()
  target_link_libraries(acsd PRIVATE m z)
endif()

# ── QA-001: 自有生产 targets 分层严格 warning (GCC/Clang; MSVC 用 /W4) ──
# 第三方 (cfitsio) 与遗留 (aio/hips/drizzle/calibration) 已 target-local 隔离:
#  - cfitsio: -w (第三方)
#  - aio/hips/drizzle/calibration: -fopenmp 编译 omp pragma (遗留, 无 -Wconversion 惩罚)
# 自有生产: core/common/phase2/phase3/cpu/CLI 全量 -Wall -Wextra -Wpedantic -Wconversion
if(NOT MSVC)
  foreach(tgt astrocs_core astrocs_common astrocs_phase2 astrocs_phase3_session
              astrocs_cpu astrocs_phase1_wcs astrocs_phase1_phot astrocs_phase1_noise
              astrocs_phase1_stars astrocs_phase1_session astrocs_phase2_session
              astrocs_module_adapters astrocs_cli_runtime astrocs_cpu_avx2 astrocs_cpu_avx512)
    if(TARGET ${tgt})
      target_compile_options(${tgt} PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
    endif()
  endforeach()
  target_compile_options(acsd PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()

# RT-005 可执行模块适配器: Phase1/2/3 session → IModule (core 不依赖会话库)
add_library(astrocs_module_adapters STATIC
  lib/infrastructure/scheduler/src/module_adapters.cpp
  # RELEASE-02 权重链（weight-chain-report §6）: HiPS 头帧级 SNR → 逆方差权重
  # （w = SNR²/F_ref²; 纯 FP64 + std, 无第三方依赖）。直接编入本 target ——
  # 不链 astrocs_v6_phase2_integrate: 后者重复编译 upm/rejection/sampler/
  # coverage, 与本 target 已链的 astrocs_phase2 产生重复符号。
  lib/algorithms/integration/v6/src/weight_chain.cpp
  # RELEASE-02 P2b: 残差制造者 PΣPᵀ 方差传播（纯 FP64 + std，无第三方依赖）。
  lib/algorithms/integration/v6/src/variance_propagation.cpp)
# B2-A10(宪章 §4.3): 节点 manifest 自报 module build ID = module_id@软件版本;
# 版本单源 = 构建期 version_generated.h (与 CLI 共用同一生成头)。
target_include_directories(astrocs_module_adapters PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/include
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/third_party
  ${CMAKE_CURRENT_BINARY_DIR}
  # RELEASE-02 权重链公共头 (astrocs/v6/weight_chain.h)
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/integration/v6/include)
# P1-001 (attempt 2): drizzle 节点唯一真实入口 hp_drizzle_run_phase1_hips(astrocs_drizzle 静态库
# 直链) + 口径更新: star-psf→dpsf_fit_batch_f64(astrocs_p1_dpsf) /
# wcs→ipv 求解器链(astrocs_p1_ipv, 含 gaia_client; 生产源零 diff 只读编译)。
# ⚠ 2026-09-20 订正（V5 分片 5 / R-3）: 旧文入口名 `hp_drizzle_run` 已作废, 实为
#   `hp_drizzle_run_phase1_hips` —— 生产调用点
#   lib/infrastructure/scheduler/src/module_adapters.cpp:4745
#   （2026-09-20 grep 复核; A 分片报告记 :4562, 行号已漂移）;
#   `hp_drizzle_run` 全仓仅剩定义本身、零生产调用者（GAP_AUDIT A-04）。
target_link_libraries(astrocs_module_adapters PUBLIC
  astrocs_core astrocs_common astrocs_contracts
  astrocs_phase1_session astrocs_phase2_session astrocs_phase3_session
  astrocs_cpu astrocs_drizzle astrocs_p1_dpsf astrocs_p1_ipv astrocs_p1_sdet
  # RELEASE-02 FIX-P1 (P1-1): Phase1 测光定标生产库 (star_matcher Tukey-IRLS
  # → k_photo); 由 p1_op_photometry 经 frame_photometry_fit 调用。
  astrocs_phase1_photcal
  # RELEASE-02 探针 (OFF 时仅 include 面, 零开销)
  astrocs_probes)
# P1-001: 生产头 include 面（dynamic_psf/ipv/star_detector/aio_hips 直读）
target_include_directories(astrocs_module_adapters PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/psf/include
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/platesolve/cpp/ipv/include
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithms/star_detection/include
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/infrastructure/aio/include)
# aio_healpix_io.h(HISS 读面) 由 AIO_ENABLE_HEALPIX 门卫（drizzle 同款语义）
target_compile_definitions(astrocs_module_adapters PUBLIC AIO_ENABLE_HEALPIX=1)

# RT-008 CLI Runtime client: CLI 经公开 Runtime API 驱动 pipeline（不 include session/IO 内部头）
add_library(astrocs_cli_runtime STATIC lib/infrastructure/cli/runtime_client.cpp)
target_include_directories(astrocs_cli_runtime PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/infrastructure/cli
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/include
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/third_party)
target_link_libraries(astrocs_cli_runtime PUBLIC
  astrocs_core astrocs_module_adapters)

# W4-A9 批次 1: projection 算法模块注册 (p3_wcs TAN 投影/WCS 内核,
# 原 lib/phase3_session/p3_wcs.cpp; ASTROCS_DESIGN §7.1「projection」)。
# 声明见 lib/algorithms/projection/CMakeLists.txt (if(NOT TARGET) 守卫)。
# 由 astrocs_phase3_session / acsd / astrocs_module_adapters 经
# target_link_libraries 引用 astrocs_p3_projection_wcs (同名 target 前向引用)。
add_subdirectory(lib/algorithms/projection)

# ── V6 (AR-033 / C-004.4): V6 生产库统一注册 ─────────────────────────────
# Wave 5/7/8 交付的 V6 库（产品 IO、Phase3 重采样传播、Phase1 产品装配、
# Phase2 三模式集成）。各子目录自带 if(NOT TARGET ...) 守卫，既支持独立
# cmake -S <dir> 自包含构建，也可被根构建复用；此处为唯一注册点。
add_subdirectory(lib/infrastructure/aio/v6)
# W4-A9 批次 2: resample 模块 (含迁入的 p3_resample.{h,cpp}; 声明见
# lib/algorithms/resample/CMakeLists.txt 的 astrocs_p3_rsmp)。
# 注意: 同一源目录只允许 add_subdirectory 一次 (重复即 CMake 二进制目录冲突)。
add_subdirectory(lib/algorithms/resample)
# W4-A9 批次 3: fits_output 模块注册 (承载迁入的 p3_output.{h,cpp}; 声明见
# lib/algorithms/fits_output/CMakeLists.txt 的 astrocs_p3_fits_output; 由
# astrocs_phase3_session 的 target_link_libraries 引用 —— 同名 target 属前向
# 引用, 由 CMake 生成期解析, 注册点与本文件内位置先后无关)。
add_subdirectory(lib/algorithms/fits_output)
add_subdirectory(lib/algorithms/integration/v6_phase1)
add_subdirectory(lib/algorithms/integration/v6)

if(ASTROCS_BUILD_TESTS)
  enable_testing()
  # W4-A9 批次 1: projection 共址测试 (p3_wcs_test / ctest 名 p3_wcs) 必须先于
  # eng/tests/unit 注册 —— eng/tests/unit/p1wcs/CMakeLists.txt 的 p1wcs_std_f1_bridge_cross
  # 以 $<TARGET_FILE:p3_wcs_test> 引用该目标 (STD-F1 导出边界第三方交叉)。
  # 目标名与 ctest 名相对迁移前不变, 外部引用零改动。
  add_subdirectory(lib/algorithms/projection/tests/p3wcs)
  # ORCH-001 批次 1: 编排层共址测试注册（改前 ctest -N | grep -ci orchestrator = 0）。
  # 必须在 enable_testing() 之后: 目标库本体已在 add_subdirectory(ORCH 模块) 声明。
  add_subdirectory(lib/infrastructure/pipeline/orchestrator/cpp/tests)
  add_subdirectory(eng/tests/unit)
  # V3 B3-A4 (RESCUE-P1-13/P1-16): Phase2 主科学门进全量 ctest。
  # lib/algorithms/coverage/tests/* 的六个 gtest 目标 (synthetic_gate / ivar_wiring /
  # execution_options / routing / sampler_parallel / async_io) 此前从不进根
  # 图 —— ctest -N | grep -i synthetic 只剩 eng/tests/unit 的 p2_upm_synthetic
  # 单点。子目录自带 find_package(GTest QUIET); GTest 缺失时这些目标零注册,
  # 故 CI 供应 libgtest-dev 以保证有真实执行面 (不得静默零测试)。
  add_subdirectory(lib/algorithms/coverage)

  # ── V6 (AR-033 / C-004.4): V6 测试目标统一注册 ────────────────────────
  # eng/tests/unit/v6_*: Wave 5 共址单元测试（9 个）；eng/tests/integration/v6_{p1,p2,p3}:
  # Wave 7/8 产品链集成；eng/tests/system/v6_runtime: Wave 9 运行面。
  # 均只依赖上方已定义的 V6 库目标与 astrocs_phase2，故置于 lib/algorithms/coverage 之后。
  add_subdirectory(eng/tests/unit/v6_aio v6_aio)
  add_subdirectory(eng/tests/unit/v6_p1_cal v6_p1_cal)
  add_subdirectory(eng/tests/unit/v6_p1_drz v6_p1_drz)
  add_subdirectory(eng/tests/unit/v6_p1_psfw v6_p1_psfw)
  add_subdirectory(eng/tests/unit/v6_p2_rej v6_p2_rej)
  add_subdirectory(eng/tests/unit/v6_p2_samp v6_p2_samp)
  add_subdirectory(eng/tests/unit/v6_p2_upm v6_p2_upm)
  add_subdirectory(eng/tests/unit/v6_p2_sky v6_p2_sky)
  # SCI-502 FIX-3：天光面 κ 可观测 + 粗糙度自适应红/绿双向
  add_subdirectory(eng/tests/unit/v6_p2_sky_kappa v6_p2_sky_kappa)
  # UPM-KAPPA-UNIFY-01：唯一可辨识性判据（正/负例 + 尺度不变 + Oracle）
  add_subdirectory(eng/tests/unit/v6_p2_identifiability v6_p2_identifiability)
  # UPM-KAPPA-UNIFY-01 证据探针：真实 M42 上天光面侧的判据读数 / λ 扫描 / 解差异
  # （只链 astrocs_phase2，不碰编排面；按需构建，不进默认目标）。
  add_executable(m42_criterion_probe
    run/UPM-KAPPA-UNIFY-01/tools/m42_criterion_probe.cpp)
  target_link_libraries(m42_criterion_probe PRIVATE astrocs_phase2)
  set_property(TARGET m42_criterion_probe PROPERTY CXX_STANDARD 17)
  set_target_properties(m42_criterion_probe PROPERTIES EXCLUDE_FROM_ALL TRUE)
  add_subdirectory(eng/tests/unit/v6_p3_proj v6_p3_proj)
  add_subdirectory(eng/tests/unit/v6_p3_rsmp v6_p3_rsmp)
  add_subdirectory(eng/tests/integration/v6_p1 v6_p1_integration)
  add_subdirectory(eng/tests/integration/v6_p2 v6_p2_integration)
  add_subdirectory(eng/tests/integration/v6_p3 v6_p3_integration)
  add_subdirectory(eng/tests/system/v6_runtime v6_runtime)
endif()

# ── QA-006: linux-deep 附加质量目标 (V8-CI-005; 默认 OFF, 仅 linux-deep CI 开) ──
# 与 QA-002 sanitizer 选项同模式: 生产默认 OFF, 不进默认构建图。
#   ASTROCS_SANITIZE_RUNTIME: qa-sanitize 目标 (clang + ASan/UBSan/LSan, 复用 QA-002 接线)
#   ASTROCS_SANITIZE_THREAD : qa-sanitize-tsan 目标 (clang + TSan; 与上一个互斥, 单选)
#   ASTROCS_BUILD_COVERAGE  : 插桩作用域已前移至 target 声明前 (见上文 F8-b 注释);
#                             此处仅声明 ccov 自定义目标 (构建+ctest+报告合并)。
if(NOT WIN32 AND NOT MSVC)
  option(ASTROCS_SANITIZE_RUNTIME "Add qa-sanitize custom target (clang ASan+UBSan+LSan)" OFF)
  option(ASTROCS_SANITIZE_THREAD "Add qa-sanitize-tsan custom target (clang TSan)" OFF)
  if(ASTROCS_SANITIZE_THREAD AND ASTROCS_SANITIZE_RUNTIME)
    message(FATAL_ERROR "qa-sanitize 与 qa-sanitize-tsan 互斥 (QA-006): 单选")
  endif()
  if(ASTROCS_BUILD_COVERAGE)
    # 报告合并逻辑在 eng/cmake/qa_coverage_report.sh（多目标 -object 列表展开）
    add_custom_target(ccov
      COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} -j2
      COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/coverage
      COMMAND ${CMAKE_COMMAND} -E env
              LLVM_PROFILE_FILE=${CMAKE_BINARY_DIR}/coverage/%p-%m.profraw
              ${CMAKE_CTEST_COMMAND} --test-dir ${CMAKE_BINARY_DIR} --output-on-failure
      COMMAND /bin/sh ${CMAKE_SOURCE_DIR}/eng/cmake/qa_coverage_report.sh
              ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR}
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      COMMENT "QA-006 覆盖基线测量 (lib+cli, LLVM source-based; ctest 全量)"
      VERBATIM)
  endif()
  if(ASTROCS_SANITIZE_RUNTIME OR ASTROCS_SANITIZE_THREAD)
    find_program(ASTROCS_QA_CLANG clang)
    if(NOT ASTROCS_QA_CLANG)
      message(FATAL_ERROR "qa-sanitize/qa-sanitize-tsan 需要 clang 工具链 (linux-deep CI host 提供)")
    endif()
    if(ASTROCS_SANITIZE_THREAD)
      set(_QA_SAN_OPT -DASTROCS_ENABLE_TSAN=ON)
      set(_QA_SAN_TGT qa-sanitize-tsan)
      # TSan suppression: vendored cfitsio 4.6.4 内部诊断状态写竞争
      # (Fitsio_Pthread_Status, fitsio2.h FFLOCK/FFUNLOCK 宏, 上游固有模式)
      # — 仓库既有先例 eng/tools/quality/tsan/cfitsio.supp (QA-002, 仅限第三方
      # 符号)。全量 ctest 环境统一挂载 (与下方 ASan 分支 ASAN_OPTIONS 同
      # 模式); 路径取 qa-sanitize 子 build 树副本 (eng/tests/unit configure 时
      # 生成, 不含空格) — sanitizer flag 解析按空白切 token, 源树路径含
      # 空格会破坏 TSAN_OPTIONS。非 TSan 构建不消费该环境变量, 无副作用。
      set(_QA_SAN_ENV ${CMAKE_COMMAND} -E env
        TSAN_OPTIONS=suppressions=${CMAKE_BINARY_DIR}/qa-sanitize/tsan/cfitsio.supp)
    else()
      set(_QA_SAN_OPT -DASTROCS_ENABLE_SANITIZERS=ON)
      set(_QA_SAN_TGT qa-sanitize)
      set(_QA_SAN_ENV ${CMAKE_COMMAND} -E env ASAN_OPTIONS=detect_leaks=1:abort_on_error=1)
    endif()
    # 独立 build dir 用 clang 重新配置, 复用本文件 QA-002 sanitizer 接线 (不重复实现)
    add_custom_target(${_QA_SAN_TGT}
      COMMAND ${CMAKE_COMMAND} -S ${CMAKE_SOURCE_DIR} -B ${CMAKE_BINARY_DIR}/qa-sanitize
              -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
              ${_QA_SAN_OPT}
      COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}/qa-sanitize -j2
      COMMAND ${_QA_SAN_ENV}
              ${CMAKE_CTEST_COMMAND} --test-dir ${CMAKE_BINARY_DIR}/qa-sanitize
              --output-on-failure -j2
      COMMENT "QA-006 clang sanitizer 全量单元 (CTest; 违例=非零退出=检查 FAIL)"
      VERBATIM)
  endif()
endif()

# ── BLD-003 模块化安装骨架: 唯一 install 规则源 (白名单 install 树) ──
# include 于全部 target 声明之后; 本文件只允许在根 CMake 出现 (BLD-002)。
include(eng/cmake/install_layout.cmake)

message(STATUS "ACSD ${ASTROCS_VERSION_STRING} root target graph ready (ACR=${ASTROCS_ENABLE_ACR})")
