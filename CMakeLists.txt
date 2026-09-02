# AstroCS 根 CMake — 唯一产品构建入口与唯一产品事实源 (BLD-002)
# ── 契约 (BLD-002 唯一根 CMake 构建图) ──
# 1) 本文件是仓库唯一 project() 与唯一产品事实源: 产品可执行 astrocs 只在此声明
#    (add_executable(astrocs ...)), 显式源列表, 禁 GLOB (QA-002)。
# 2) 子目录 CMakeLists 只声明本模块 target (tests/unit 为测试, 见文末 add_subdirectory)。
# 3) cli/CMakeLists.txt 与 lib/phase2/CMakeLists.txt 已降级为 compatibility 声明
#    (BLD-002): 保留给遗留 tests/cli 独立构建与模块自测/工具按需构建, 不是产品事实源,
#    禁止正式 install; 产品安装/发布构建一律以本文件为入口。
# 4) 正式构建入口: 本文件 (Windows preset win-msvc-17.14.39-x64 / Linux linux-control)。
cmake_minimum_required(VERSION 3.24)
project(astrocs VERSION 0.10.0 LANGUAGES C CXX)

# ── 选项 (03/BLD-001 冻结) ──
option(ASTROCS_ENABLE_ACR "Build dormant ACR tree (default OFF, production exclusion)" OFF)
option(ASTROCS_BUILD_TESTS "Build tests" ON)
# QA-002: sanitizer 选项 (ASan/UBSan/LSan 或 TSan; 生产默认 OFF)
option(ASTROCS_ENABLE_SANITIZERS "Enable ASan/UBSan/LSan (GCC/Clang, production OFF)" OFF)
option(ASTROCS_ENABLE_TSAN "Enable ThreadSanitizer (targeted, mutually exclusive with ASan)" OFF)

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
execute_process(COMMAND git rev-parse HEAD
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  OUTPUT_VARIABLE ASTROCS_GIT_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT ASTROCS_GIT_COMMIT)
  set(ASTROCS_GIT_COMMIT "0000000000000000000000000000000000000000")
endif()
set(ASTROCS_VERSION_STRING "${ASTROCS_BASE_VERSION}+g${ASTROCS_GIT_COMMIT}")

# 版本生成头 (VER-001: 从 VERSION 唯一源)
configure_file(cli/version_generated.h.in ${CMAKE_CURRENT_BINARY_DIR}/version_generated.h @ONLY)
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/version_generated.h
  "#define ASTROCS_COMMIT_SHA \"${ASTROCS_GIT_COMMIT}\"\n")

# vendored cfitsio 显式清单 (BLD-001 禁 GLOB; 由 tools/gen_cfitsio_list.py 生成)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/cfitsio_sources.cmake)

# OpenMP 探测提前: 遗留模块 (calibration/aio/drizzle) 含 omp pragma, 编译需 -fopenmp
# (QA-001: 自有遗留 target target-local 编译, 不污染主程序; 第三方 cfitsio 仍 -w 隔离)
if(UNIX)
  find_package(OpenMP QUIET)
endif()

# ── 显式 target 依赖图 (依赖方向见 ARCH-001 §3) ──

# core/contracts: 公共类型 (DATA/API/MOD)
add_library(astrocs_contracts INTERFACE)
target_include_directories(astrocs_contracts INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/common/include)

# io adapter (IO-001: 依赖方向 core <- io; 不 include runtime/模块)
# BLD-003: 更名 astrocs_io_adapter —— ARC-001 冻结的 DLL target 计划
# (install_tree 一一对应) 中 "astrocs_io" 是平台 SHARED DLL (astrocs_io.dll/.so,
# FITS/HiPS/原子流式 I/O 宿主服务), 见下 "BLD-003 平台 SHARED 骨架" 段落;
# 本 STATIC 为 legacy 文件适配器 (ArtifactTransaction/FileIoAdapter, IO-001),
# 保留原名会导致 target 重名 (CMake fatal)。
add_library(astrocs_io_adapter STATIC lib/io/src/io_adapter.cpp)
target_include_directories(astrocs_io_adapter PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(astrocs_io_adapter PUBLIC astrocs_core)

# ── BLD-003 平台 SHARED 骨架 (G2: SHARED runtime/io/noop/baseline targets) ──
# 契约 (BLD-003 + 03_TARGET §4.1 + ARC-001):
#   - 唯一 install 规则源 = cmake/install_layout.cmake (文末 include);
#     install 树与 ARC-001 dll_units 计划一一对应; clean install 仅白名单。
#   - astrocs_runtime / astrocs_io: 平台 SHARED DLL 骨架。当前以已冻结、
#     自包含的 C ABI 源 (DATA-001 artifact / IO-001 fits stream) 起步,
#     使 target 真实可编译、可安装、可被 ABI-003 loader 加载验证;
#     loader/registry/DAG/scheduler/ArtifactStore 接线由 ABI-003/004 与
#     RT-002 等后续向本 target 追加源, 本任务不伪装平台实现完成。
#   - astrocs_noop: conformance module SHARED (modules/conformance/noop),
#     仅可加载性骨架 (SKELETON), 语义由 ABI-005 填充; 见其 README。
#   - astrocs_cpu_baseline: baseline backend DSO。Linux 以 legacy backend
#     ABI 源 (baseline_backend.cpp, SA-CPU-09 已有) 编译为 .so 技术预览,
#     仅验证 provider 安装路径/RPATH/加载; 正式 provider ABI
#     (astrocs_provider_query_v1, module_api_v1.h 冻结) 与 Windows DLL 由
#     CPU-002 (providers/cpu/baseline) 交付, 本任务不伪装 provider 完成。
# 导出可见性 (12 §1/ABI-006): module DLL 只导出批准 C ABI 入口; C 源 DLL
# 使用 ASTROCS_ABI_SHARED/EXPORTS + -fvisibility=hidden 控制 (各 target)。

# astrocs_runtime: runtime 平台 DLL 骨架 (artifact C ABI, DATA-001 冻结源)
# 导出: 源为冻结 C ABI (无 ASTROCS_EXPORT 标注, SA-IO-07/DATA-06 头所有
# 顶层函数即 API) → Linux 默认全导出; Windows 经 WINDOWS_EXPORT_ALL_SYMBOLS
# (C 源, 导出全部顶层 API; ABI-006 收紧为精确导出清单, 正式 DLL 由 WIN-* 复核)。
add_library(astrocs_runtime SHARED runtime/artifact_store/artifact_abi_v1.c)
target_include_directories(astrocs_runtime PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(MSVC)
  set_property(TARGET astrocs_runtime PROPERTY WINDOWS_EXPORT_ALL_SYMBOLS ON)
else()
  target_compile_options(astrocs_runtime PRIVATE -Wall -Wextra)
endif()
if(WIN32)
  set_target_properties(astrocs_runtime PROPERTIES OUTPUT_NAME "astrocs_runtime")
endif()

# astrocs_io: io 平台 DLL 骨架 (FITS stream C ABI, IO-001 冻结源)
add_library(astrocs_io SHARED runtime/io/fits_core.c)
target_include_directories(astrocs_io PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/services/io/include)
if(MSVC)
  set_property(TARGET astrocs_io PROPERTY WINDOWS_EXPORT_ALL_SYMBOLS ON)
else()
  target_compile_options(astrocs_io PRIVATE -Wall -Wextra)
endif()
if(WIN32)
  set_target_properties(astrocs_io PROPERTIES OUTPUT_NAME "astrocs_io")
endif()
# fits_core.c 需数学库 (Linux); Windows CRT 自带
if(NOT WIN32)
  target_link_libraries(astrocs_io PRIVATE m)
endif()

# astrocs_noop: conformance module SHARED (模块 CMakeLists 只声明本 target,
# BLD-002: 子目录 CMakeLists 非产品事实源; 见 modules/conformance/noop/README)
add_subdirectory(modules/conformance/noop)

# astrocs_cpu_baseline: baseline backend DSO 技术预览 (Linux; legacy ABI,
# 无导出标注 → 默认全导出)。正式 provider ABI/Windows DLL → CPU-002
# (providers/cpu/baseline)
add_library(astrocs_cpu_baseline SHARED
  lib/backend_host/baseline_backend.cpp)
target_include_directories(astrocs_cpu_baseline PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/backend_host
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/common/crypto
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
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
foreach(tgt astrocs_runtime astrocs_io astrocs_noop astrocs_cpu_baseline)
  if(UNIX AND NOT APPLE)
    set_target_properties(${tgt} PROPERTIES
      INSTALL_RPATH "$ORIGIN" BUILD_RPATH_USE_ORIGIN YES)
  endif()
endforeach()


add_library(astrocs_core STATIC
  lib/core/src/artifact.cpp
  lib/core/src/artifact_store.cpp
  lib/core/src/module.cpp
  lib/core/src/pipeline.cpp
  lib/core/src/context.cpp
  lib/core/src/runtime.cpp
  lib/core/src/scheduler.cpp
  lib/core/src/checkpoint.cpp
  lib/core/src/logging.cpp)
target_include_directories(astrocs_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
target_link_libraries(astrocs_core PUBLIC astrocs_contracts Threads::Threads)
find_package(Threads REQUIRED)

# core/common: sha256/healpix (纯算法)
add_library(astrocs_common STATIC
  lib/common/crypto/sha256.cpp
  lib/common/healpix/healpix_core.cpp)
target_include_directories(astrocs_common PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/common
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/common/include
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/common/crypto
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/common/healpix)

# vendored cfitsio (第三方, SYSTEM 隔离; 不参与模块图)
add_library(astrocs_cfitsio STATIC ${ASTROCS_CFITSIO_SOURCES})
target_include_directories(astrocs_cfitsio PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/astro_image_io/third_party/cfitsio)
if(MSVC)
  # WIN-001: cfitsio 依赖适配 — 源码 `-w` 为 GCC 旗标 (MSVC 忽略并告警 D9002);
  # _REENTRANT 需 pthread.h → 用 cmake/win32_pthread_shim (SRWLOCK 映射, MSVC 专用);
  # zcompress.c 需 zlib (ACS_ZLIB_ROOT, MSVC 编译的 zlib 安装, 见 fatduck C:/Users/fujia/zlib-msvc)。
  # 注意: 不可用 MSYS2 mingw64 include (gcc 头无法被 MSVC 编译)。
  target_compile_options(astrocs_cfitsio PRIVATE /w)
  target_include_directories(astrocs_cfitsio PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/win32_pthread_shim)
  target_link_libraries(astrocs_cfitsio PUBLIC kernel32)  # SRWLock API (shim)
  set(ACS_ZLIB_ROOT "" CACHE PATH "MSVC zlib install root (include/zlib.h + lib/zlib.lib)")
  if(ACS_ZLIB_ROOT)
    target_include_directories(astrocs_cfitsio PRIVATE "${ACS_ZLIB_ROOT}/include")
    target_link_libraries(astrocs_cfitsio PUBLIC "${ACS_ZLIB_ROOT}/lib/libz.lib")
  endif()
else()
  target_compile_options(astrocs_cfitsio PRIVATE -w)
endif()
# IO-003: CFITSIO 显式 reentrant 构建 (fits_is_reentrant() 必须返回 1)
target_compile_definitions(astrocs_cfitsio PRIVATE _REENTRANT)

# io/astro_image_io: 图像/HiPS I/O adapter (aio_pipeline* 保留待 LEG-003 退出)
add_library(astrocs_aio STATIC
  lib/astro_image_io/src/aio_fits.cpp
  lib/astro_image_io/src/aio_api.cpp
  lib/astro_image_io/src/aio_log.cpp
  lib/astro_image_io/src/aio_compressor.cpp
  lib/astro_image_io/src/aio_xisf.cpp
  lib/astro_image_io/src/aio_pipeline.cpp)
target_include_directories(astrocs_aio PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/astro_image_io/include
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/astro_image_io/src
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/astro_image_io/third_party/cfitsio
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/common
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
target_compile_definitions(astrocs_aio PRIVATE AIO_ENABLE_FITS AIO_ENABLE_XISF)
if(MSVC AND ACS_ZLIB_ROOT)
  target_link_libraries(astrocs_aio PUBLIC astrocs_common astrocs_cfitsio "${ACS_ZLIB_ROOT}/lib/libz.lib")
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
  lib/astro_image_io/src/hips/aio_hips_reader.cpp
  lib/astro_image_io/src/hips/aio_hips_writer.cpp
  lib/astro_image_io/src/healpix/aio_healpix_io.cpp
  lib/astro_image_io/src/hiss_codec.cpp
  lib/astro_image_io/src/hiss_common.cpp
  lib/astro_image_io/src/hiss_reader.cpp
  lib/astro_image_io/src/hiss_stream_writer.cpp
  lib/astro_image_io/src/hiss_tile_model.cpp
  lib/astro_image_io/src/hiss_transform.cpp
  lib/astro_image_io/src/hiss_writer.cpp
  lib/astro_image_io/src/aio_upm.cpp)
target_include_directories(astrocs_hips PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/astro_image_io/include
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/astro_image_io/src
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/astro_image_io/third_party/cfitsio
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/common
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/common/healpix)
target_compile_definitions(astrocs_hips PRIVATE AIO_ENABLE_FITS AIO_ENABLE_HEALPIX)
target_link_libraries(astrocs_hips PUBLIC astrocs_aio astrocs_common)

# modules/calibration
add_library(astrocs_calibration STATIC
  lib/calibration/src/calibrator.cpp
  lib/calibration/src/master_generator.cpp
  lib/calibration/src/cosmetic_corrector.cpp
  lib/calibration/src/ac_api.cpp)
target_include_directories(astrocs_calibration PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/calibration/include
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/astro_image_io/include)
target_link_libraries(astrocs_calibration PUBLIC astrocs_aio astrocs_common)
# QA-001: calibrator/cosmetic_corrector/master_generator 含 omp pragma → -fopenmp
if(UNIX AND OpenMP_CXX_FOUND)
  target_compile_options(astrocs_calibration PRIVATE -fopenmp)
  target_link_libraries(astrocs_calibration PRIVATE OpenMP::OpenMP_CXX)
endif()

# modules/phase2 (纯 CPU 生产模块; ACR 源不在此列 — LEG-004)
add_library(astrocs_phase2 STATIC
  lib/phase2/src/upm.cpp
  lib/phase2/src/stage2_common.cpp
  lib/phase2/src/rejection.cpp
  lib/phase2/src/coverage.cpp
  lib/phase2/src/sampler.cpp
  lib/phase2/src/block.cpp
  lib/phase2/src/integrate.cpp
  lib/phase2/src/cuda_bridge_stub.cpp)
target_include_directories(astrocs_phase2 PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/phase2/include
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/acr/backends/cuda/bridge
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/astro_image_io/include
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/astro_image_io/src
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/astro_image_io/third_party/cfitsio
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/common)
target_link_libraries(astrocs_phase2 PUBLIC astrocs_contracts astrocs_common astrocs_aio astrocs_hips)

# modules/healpix_drizzle
add_library(astrocs_drizzle STATIC
  lib/healpix_db/healpix_drizzle/fits_reader.cpp
  lib/healpix_db/healpix_drizzle/wcs_sip.cpp
  lib/healpix_db/healpix_drizzle/poly_clip.cpp
  lib/healpix_db/healpix_drizzle/spherical_overlap.cpp
  lib/healpix_db/healpix_drizzle/drizzle_engine.cpp
  lib/healpix_db/healpix_drizzle/reverse_drizzle.cpp
  lib/healpix_db/healpix_drizzle/hp_drizzle_api.cpp
  lib/healpix_db/healpix_drizzle/astro_sphere_sink.cpp
  lib/healpix_db/healpix_drizzle/snr_evaluator.cpp)
target_include_directories(astrocs_drizzle PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/healpix_db/healpix_drizzle
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/astro_image_io/include
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/astro_image_io/src
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/astro_image_io/third_party/cfitsio
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/common
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/common/healpix)
# QA-001: nanoflann.hpp (BSD-3, vendored 第三方) 经本 target 引号 include 引入;
# 仅对该第三方头做 -isystem 隔离, drizzle 自有 .cpp 仍参与严格 warning 分层.
target_include_directories(astrocs_drizzle SYSTEM PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/healpix_db/healpix_drizzle)
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
  lib/backend_host/cpu_features.cpp
  lib/backend_host/hardware_inspect.cpp
  lib/backend_host/backend_loader.cpp
  lib/backend_host/baseline_backend.cpp
  lib/backend_host/profile_gen.cpp
  lib/backend_host/profile_gen_v2.cpp
  lib/backend_host/cpu_routing.cpp
  lib/backend_host/host_services.cpp
  lib/backend_host/bench_harness.cpp)
target_include_directories(astrocs_cpu PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/backend_host
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/common/crypto
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
target_link_libraries(astrocs_cpu PUBLIC astrocs_contracts astrocs_common)

# CPU-001 (G3): 独立 provider 变体 target — 真实独立编译单元 (05 §2)
# 编译隔离合同: -mavx2/-mfma 等高级旗标仅作用于对应 provider target(PRIVATE),
# 不污染 astrocs_cpu/baseline 与主 CLI; 主 CLI 保持最低 amd64/SSE2 (ARCH-003 §2)。
# 变体与 baseline 共享 baseline_kernels_impl.inc/backend_table.inc 同一源(零复制漂移),
# 仅编译旗标与 ASTROCS_BACKEND_ID 不同 → 每条路径独立编译成不同目标并各自 self-test。
# 注意: 两个变体 target 均不得被 astrocs(主 CLI) 链接 — 由 check_isa_leak 的 link map 验证把关。
add_library(astrocs_cpu_avx2 STATIC lib/backend_host/avx2_backend.cpp)
target_include_directories(astrocs_cpu_avx2 PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/backend_host
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/common/crypto)
target_compile_options(astrocs_cpu_avx2 PRIVATE -mavx2 -mfma)
target_link_libraries(astrocs_cpu_avx2 PUBLIC astrocs_contracts astrocs_common)

add_library(astrocs_cpu_avx512 STATIC lib/backend_host/avx512_backend.cpp)
target_include_directories(astrocs_cpu_avx512 PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/backend_host
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/common/crypto)
# AVX512 实际使用子集: F + DQ + BW + VL (ISA-004, 与 cpu_features.h 检测面一致)
target_compile_options(astrocs_cpu_avx512 PRIVATE
  -mavx512f -mavx512bw -mavx512vl -mavx512dq)
target_link_libraries(astrocs_cpu_avx512 PUBLIC astrocs_contracts astrocs_common)

# phase1 wcs + photometry (P1-004)
add_library(astrocs_phase1_wcs STATIC lib/phase1/wcs/wcs_tan.cpp)
target_include_directories(astrocs_phase1_wcs PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/phase1/wcs ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_library(astrocs_phase1_phot STATIC lib/phase1/photometry/photometer.cpp)
target_include_directories(astrocs_phase1_phot PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/phase1/photometry ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(astrocs_phase1_phot PUBLIC astrocs_contracts)

# phase1 noise (P1-005 SCI-NOISE-001)
add_library(astrocs_phase1_noise STATIC lib/phase1/noise/noise_model.cpp)
target_include_directories(astrocs_phase1_noise PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/phase1/noise ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(astrocs_phase1_noise PUBLIC astrocs_contracts)

# phase1 stars (P1-003 StarDetector)
add_library(astrocs_phase1_stars STATIC lib/phase1/stars/star_detector.cpp)
target_include_directories(astrocs_phase1_stars PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/phase1/stars
  ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(astrocs_phase1_stars PUBLIC astrocs_contracts)

# phases sessions (facade, 后续迁移为 preset)
add_library(astrocs_phase1_session STATIC lib/phase1_session/p1_session.cpp)
target_include_directories(astrocs_phase1_session PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/phase1_session
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/calibration/include)
target_link_libraries(astrocs_phase1_session PUBLIC astrocs_contracts astrocs_calibration astrocs_aio)

add_library(astrocs_phase2_session STATIC lib/phase2_session/p2_session.cpp)
target_include_directories(astrocs_phase2_session PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/phase2_session
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/phase2/include)
target_link_libraries(astrocs_phase2_session PUBLIC astrocs_contracts astrocs_phase2)

add_library(astrocs_phase3_session STATIC
  lib/phase3_session/p3_session.cpp
  lib/phase3_session/p3_wcs.cpp
  lib/phase3_session/hips_properties.cpp
  lib/phase3_session/p3_output.cpp
  lib/phase3_session/p3_resample.cpp)
target_include_directories(astrocs_phase3_session PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/phase3_session
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/common
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/common/crypto
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/common/healpix
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/astro_image_io/include
  ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(astrocs_phase3_session PUBLIC astrocs_contracts astrocs_aio astrocs_common)

# app/astrocs_cli: 薄 CLI (显式链接各库; 无 GLOB; ACR 不链)
# RT-008: main.cpp 仅入口壳; parser/commands/runtime_client 为独立编译单元
add_executable(astrocs
  cli/main.cpp
  cli/parser.cpp
  cli/commands.cpp
  cli/runtime_client.cpp)
target_include_directories(astrocs PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/cli
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/astro_image_io/src
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/astro_image_io/include
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/astro_image_io/third_party/cfitsio
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/healpix_db/healpix_drizzle
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/common
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/common/crypto
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/common/healpix
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/phase2/include)
target_link_libraries(astrocs PRIVATE
  astrocs_contracts astrocs_core astrocs_cli_runtime astrocs_common astrocs_cfitsio astrocs_aio astrocs_hips
  astrocs_calibration astrocs_phase2 astrocs_drizzle astrocs_cpu
  astrocs_phase1_session astrocs_phase2_session astrocs_phase3_session
  ${CMAKE_DL_LIBS})
if(MSVC)
  if(ACS_ZLIB_ROOT)
    target_link_libraries(astrocs PRIVATE "${ACS_ZLIB_ROOT}/lib/libz.lib")
  endif()
else()
  target_link_libraries(astrocs PRIVATE m z)
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
  target_compile_options(astrocs PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()

# RT-005 可执行模块适配器: Phase1/2/3 session → IModule (core 不依赖会话库)
add_library(astrocs_module_adapters STATIC lib/core/src/module_adapters.cpp)
target_include_directories(astrocs_module_adapters PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
target_link_libraries(astrocs_module_adapters PUBLIC
  astrocs_core astrocs_common astrocs_contracts
  astrocs_phase1_session astrocs_phase2_session astrocs_phase3_session
  astrocs_cpu)

# RT-008 CLI Runtime client: CLI 经公开 Runtime API 驱动 pipeline（不 include session/IO 内部头）
add_library(astrocs_cli_runtime STATIC cli/runtime_client.cpp)
target_include_directories(astrocs_cli_runtime PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/cli
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
target_link_libraries(astrocs_cli_runtime PUBLIC
  astrocs_core astrocs_module_adapters)

if(ASTROCS_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests/unit)
endif()

# ── BLD-003 模块化安装骨架: 唯一 install 规则源 (白名单 install 树) ──
# include 于全部 target 声明之后; 本文件只允许在根 CMake 出现 (BLD-002)。
include(cmake/install_layout.cmake)

message(STATUS "AstroCS ${ASTROCS_VERSION_STRING} root target graph ready (ACR=${ASTROCS_ENABLE_ACR})")
