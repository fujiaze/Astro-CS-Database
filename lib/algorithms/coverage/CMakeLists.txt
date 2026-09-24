# lib/algorithms/coverage/CMakeLists.txt — COMPATIBILITY 声明 (BLD-002, 非产品事实源)
#
# 唯一产品事实源 = 根 CMakeLists.txt (唯一 project / 唯一 add_executable(astrocs))。
# 本文件仅保留给 lib/algorithms/coverage 模块自测与工具按需构建 (compatibility target):
#   - phase2 STATIC 库的正式产品源集由根 CMakeLists.txt 的 astrocs_phase2 声明
#     (lib/algorithms/coverage/src/*.cpp 显式列表, 无 GLOB); 本文件不再作为生产 target 事实源;
#   - astrocs-stage2 / calibrated_pair_diag / rejection_cli 为按需工具
#     (EXCLUDE_FROM_ALL, 非正式 ALL target), 不进入默认构建与任何 install;
#   - 禁止正式 install (BLD-002): 安装/发布规则只允许存在于根 CMakeLists.txt;
#   - 本文件保留 enable_testing()/GTest 分支仅供模块独立自测 (eng/tests/cli 不依赖)。
cmake_minimum_required(VERSION 3.24)
project(astro_phase2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT MSVC)
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()
if(NOT CMAKE_GENERATOR AND CMAKE_HOST_WIN32)
    set(CMAKE_GENERATOR "MinGW Makefiles" CACHE STRING "" FORCE)
endif()

# Hotfix: OpenMP 硬禁用（Fatduck: 即使存在 libgomp 也串行；避免 0xC0000005）
# 如需实验并行，改此 OFF→ON 后重建（默认 OFF 才能过 32→1+4/4 门禁）
option(P2_ENABLE_OPENMP "Enable OpenMP parallel sampler (default OFF, hotfix revert)" OFF)
if(P2_ENABLE_OPENMP)
    find_package(OpenMP)
    if(OpenMP_CXX_FOUND)
        message(STATUS "Phase2: OpenMP enabled (P2_ENABLE_OPENMP=ON)")
    else()
        message(STATUS "Phase2: P2_ENABLE_OPENMP=ON but OpenMP NOT found, fallback single thread")
    endif()
else()
    # 关键：显式禁掉上游/缓存中残留的 OpenMP_CXX_FOUND，避免误链 libgomp
    set(OpenMP_CXX_FOUND FALSE)
    message(STATUS "Phase2: OpenMP disabled (hotfix default, serial sampler) - libgomp NOT linked")
endif()

add_library(phase2 STATIC
    src/upm.cpp
    src/sky_plane.cpp
    src/identifiability.cpp
    src/stage2_common.cpp
    src/rejection.cpp
    src/coverage.cpp
    src/sampler.cpp
    src/block.cpp
    src/integrate.cpp
    src/acr_kernels.cpp
    src/async_io.cpp
    ../../infrastructure/acr/api/kernel_registry.cpp
    # Windows: real DLL loader; Linux: stub (bridge always unavailable, CPU path)
    $<$<PLATFORM_ID:Windows>:../../infrastructure/acr/backends/cuda/cuda_bridge_loader.cpp>
    $<$<NOT:$<PLATFORM_ID:Windows>>:src/cuda_bridge_stub.cpp>
    ../../infrastructure/acr/scheduler/device_executor.cpp
    ../shared/healpix/healpix_core.cpp
    ../shared/crypto/sha256.cpp
)
target_include_directories(phase2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(phase2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../infrastructure/acr/include)
target_include_directories(phase2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../infrastructure/acr)
target_include_directories(phase2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../infrastructure/acr/backends/cuda/bridge)
target_include_directories(phase2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../infrastructure/acr/scheduler)
target_include_directories(phase2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../infrastructure/aio/include)
# CLEAN-403: sky_plane.cpp 的文件读写经 aio 机制原语 (aio_atomic_file.h /
# aio_file_io.h, header-only) ⇒ compatibility 目标同样需要 aio/src include 面。
target_include_directories(phase2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../infrastructure/aio/src)
# RELEASE-02 性能探针头面 (ASTROCS_PROBES 默认 OFF; 未定义时 astrocs/probe.h 的宏
# 全部为空语句)。独立 configure 不走根图 astrocs_probes target, 故此处显式登记。
target_include_directories(phase2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../infrastructure/observability/probes/include)
target_include_directories(phase2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../algorithms/shared)
# SCI-FIX-AIO (V11-N-01) 连带修: compatibility 目标此前缺 third_party 头路径 ⇒
# nlohmann/json 落到系统副本 (json_abi_v3_11_3), 而根图目标 (astrocs_hips/aio)
# 带 third_party (v3_12_0); 两者混链时 p2_stage2_parse_config 之类的符号
# mangling 不一致 (旧 astro_image_io.dll 恰好掩盖了该不一致)。统一到仓内
# vendored json。
target_include_directories(phase2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../../lib/third_party)
if(P2_ENABLE_OPENMP AND OpenMP_CXX_FOUND)
    target_link_libraries(phase2 PUBLIC OpenMP::OpenMP_CXX)
    # CON-004: 把 P2_ENABLE_OPENMP 作为编译宏定义（原为装饰性 option，源内 #if 恒假）。
    # 仅 ON 且找到 OpenMP 时定义 => 源内 #if defined(P2_ENABLE_OPENMP) 才真正激活并行路径。
    target_compile_definitions(phase2 PRIVATE P2_ENABLE_OPENMP=1)
endif()

enable_testing()
find_package(GTest QUIET)
if(GTest_FOUND)
    # SCI-FIX-AIO (V11-N-01): 源树内 **未跟踪** 的
    # ../../infrastructure/aio/astro_image_io.dll 是 ARCH-001 迁移前的历史构建
    # 产物 (ELF .so 误名 .dll; 旧 AstroSphereTileView 布局, 无版本化 ABI 头)。
    # 它与本仓当前 aio_hips.h 不一致时字段整体错位 —— 实测 writer 读到的
    # width=0 ⇒ rc=-2 "view 与产品集不匹配", 或更糟: 静默错位写数据。
    # 根图构建一律走 astrocs_hips/astrocs_aio 静态目标 (与 eng/tests/unit/p1_hips*
    # 同口径); 仅模块独立 configure (无根目标) 才回退到该历史产物。
    if(TARGET astrocs_hips AND TARGET astrocs_aio)
        set(P2_AIO_LINK astrocs_hips astrocs_aio)
    else()
        set(P2_AIO_LINK ${CMAKE_CURRENT_SOURCE_DIR}/../../infrastructure/aio/astro_image_io.dll)
    endif()
    add_executable(phase2_synthetic_gate tests/synthetic_gate.cpp)
    target_link_libraries(phase2_synthetic_gate PRIVATE phase2 GTest::gtest_main
        ${P2_AIO_LINK})
    add_executable(phase2_ivar_wiring tests/ivar_wiring_test.cpp)
    # SCI-FIX-AIO (V11-N-01): 与 phase2_sampler_parallel 同因同修 —— 源树内
    # 未跟踪的 astro_image_io.dll 是历史构建产物 (旧结构布局, 无版本化 ABI 头),
    # 与本仓当前 aio_hips.h 不一致时读取字段整体错位 (实测 width 读成 0 →
    # writer rc=-2 "view 与产品集不匹配")。根图构建走 astrocs_hips/astrocs_aio
    # 静态目标 (与 eng/tests/unit/p1_hips_writer 同口径); 模块独立 configure 保留
    # DLL 后备。
    if(TARGET astrocs_hips AND TARGET astrocs_aio)
        target_link_libraries(phase2_ivar_wiring PRIVATE phase2 GTest::gtest_main
            astrocs_hips astrocs_aio)
    else()
        target_link_libraries(phase2_ivar_wiring PRIVATE phase2 GTest::gtest_main
            ${P2_AIO_LINK})
    endif()
    add_executable(phase2_execution_options tests/execution_options_test.cpp)
    target_link_libraries(phase2_execution_options PRIVATE phase2 GTest::gtest_main)
    add_executable(phase2_routing tests/routing_test.cpp)
    target_link_libraries(phase2_routing PRIVATE phase2 GTest::gtest_main)
    add_executable(phase2_sampler_parallel tests/sampler_parallel_consistency_test.cpp)
    # SCI-FIX-WEIGHT (M8-C-002): Linux 合成 HiPS 门需要**当前** AIO 写面。
    # 旧直链 ../../infrastructure/aio/astro_image_io.dll 是源树内**未跟踪**的
    # 历史构建产物（缺版本化 ABI 初始化/新字段布局），与当前 aio_hips.h 不一致
    # 会让 fixture 写入失败（实测 make_synth_frame=false）。根图构建下改走
    # astrocs_hips/astrocs_aio 静态目标（与 eng/tests/unit/p1_hips_writer 同口径）；
    # 模块独立 configure（无根目标）时保留 DLL 后备。
    if(TARGET astrocs_hips AND TARGET astrocs_aio)
        target_link_libraries(phase2_sampler_parallel PRIVATE phase2 GTest::gtest_main
            astrocs_hips astrocs_aio)
    else()
        target_link_libraries(phase2_sampler_parallel PRIVATE phase2 GTest::gtest_main
            ${P2_AIO_LINK})
    endif()
    add_executable(phase2_async_io tests/async_io_test.cpp)
    target_link_libraries(phase2_async_io PRIVATE phase2 GTest::gtest_main)
    # RQS-B3 / M4-C-01 (ALG-P2-SMP-001 11.3 F2): kcorr_lookup non-uniform
    # pixfrac grid piecewise-linear + exact corners; drives production impl
    # via the internal seam at the end of sampler.cpp.
    add_executable(phase2_sampler tests/kcorr_lookup_test.cpp)
    target_include_directories(phase2_sampler PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../infrastructure/aio/include)
    target_link_libraries(phase2_sampler PRIVATE phase2 GTest::gtest_main
        ${P2_AIO_LINK})
    # M4-C-06 (SCI REJECTION 4/8): 非有限 weights 资格层校验 + INVALID_INPUT
    # hard fail 的共址回归门（正例 + 负例，能红能绿）。
    add_executable(phase2_rej_nonfinite_weights tests/rejection_nonfinite_weights_test.cpp)
    target_link_libraries(phase2_rej_nonfinite_weights PRIVATE phase2 GTest::gtest_main)

    # ── PSFSW-RETIRE-01（负责人裁决）：退役对象负例门（能红能绿） ──────────
    # ① 生产模式门 + 运行面路由：psfsw_robust 必须显式拒绝（不得静默接受）。
    add_executable(phase2_coverage_mode_retire tests/weight_mode_retire_negative_test.cpp)
    target_include_directories(phase2_coverage_mode_retire PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../infrastructure/cli)
    # coverage.cpp 的 inspect_frame 走 aio_hips_* ⇒ 与 phase2_synthetic_gate 同口径
    # 需要 AIO 链接面（P2_AIO_LINK）。
    target_link_libraries(phase2_coverage_mode_retire PRIVATE phase2 GTest::gtest_main
        ${P2_AIO_LINK})

    # ② 退役对象拒绝面（resample variance_from / calibration variance_from /
    #    drizzle 单位符号）：被测生产源直接编译，避免与 phase2 的重复符号面
    #    （astrocs_p3_rsmp 已链 astrocs_common/aio/hips/hips_properties）。
    #    注：本任务写域不含 eng/tests/**，故负例登记在 coverage 模块共址测试位。
    add_executable(p3_rsmp_retired_object_negative
        tests/retired_object_reject_negative_test.cpp
        ${CMAKE_SOURCE_DIR}/lib/algorithms/drizzle/healpix_drizzle/v6_drizzle_science.cpp
        ${CMAKE_SOURCE_DIR}/lib/algorithms/calibration/src/v6_calibration_covariance.cpp)
    target_include_directories(p3_rsmp_retired_object_negative PRIVATE
        ${CMAKE_SOURCE_DIR}/lib/algorithms/drizzle/healpix_drizzle
        ${CMAKE_SOURCE_DIR}/lib/algorithms/calibration/include)
    target_link_libraries(p3_rsmp_retired_object_negative PRIVATE
        astrocs_p3_rsmp GTest::gtest_main)

    include(GoogleTest)
    # V3 B3-A4: 每个 Phase2 门的发现测试统一加 TEST_PREFIX "phase2_<gate>."，
    # 使全量 ctest 中的 Phase2 科学门可归因、可 grep（"ctest -N | grep -i
    # synthetic" 必须命中 phase2_synthetic_gate.*），并让 CTEST-PHASE2-*
    # 注册项可用单一正则覆盖。
    gtest_discover_tests(phase2_synthetic_gate DISCOVERY_MODE PRE_TEST
        TEST_PREFIX "phase2_synthetic_gate.")
    gtest_discover_tests(phase2_ivar_wiring DISCOVERY_MODE PRE_TEST
        TEST_PREFIX "phase2_ivar_wiring.")
    gtest_discover_tests(phase2_execution_options DISCOVERY_MODE PRE_TEST
        TEST_PREFIX "phase2_execution_options.")
    gtest_discover_tests(phase2_routing DISCOVERY_MODE PRE_TEST
        TEST_PREFIX "phase2_routing.")
    gtest_discover_tests(phase2_sampler_parallel DISCOVERY_MODE PRE_TEST
        TEST_PREFIX "phase2_sampler_parallel.")
    gtest_discover_tests(phase2_async_io DISCOVERY_MODE PRE_TEST
        TEST_PREFIX "phase2_async_io.")
    gtest_discover_tests(phase2_sampler DISCOVERY_MODE PRE_TEST
        TEST_PREFIX "phase2_sampler.")
    gtest_discover_tests(phase2_rej_nonfinite_weights DISCOVERY_MODE PRE_TEST
        TEST_PREFIX "phase2_rej_nonfinite_weights.")
    gtest_discover_tests(phase2_coverage_mode_retire DISCOVERY_MODE PRE_TEST
        TEST_PREFIX "phase2_coverage_mode_retire.")
    gtest_discover_tests(p3_rsmp_retired_object_negative DISCOVERY_MODE PRE_TEST
        TEST_PREFIX "p3_rsmp_retired_object_negative.")
endif()

# BLD-002: 以下工具降级为 compatibility (EXCLUDE_FROM_ALL) —— 非正式 ALL target,
# 仅按需显式构建 (cmake --build --target ...); 不进入任何 install/发布树。
add_executable(astrocs-stage2 tools/stage2.cpp)
target_link_libraries(astrocs-stage2 PRIVATE phase2
    ${P2_AIO_LINK})
if(P2_ENABLE_OPENMP AND OpenMP_CXX_FOUND)
    # CON-006: stage2 工具本身有逐 pixel integration 并行代码，也需要编译宏。
    target_compile_definitions(astrocs-stage2 PRIVATE P2_ENABLE_OPENMP=1)
endif()
set_target_properties(astrocs-stage2 PROPERTIES EXCLUDE_FROM_ALL TRUE)
# V3 B3-A4: phase2_ivar_wiring 以 std::system("astrocs-stage2 ...") 驱动本
# compatibility 工具；该工具 EXCLUDE_FROM_ALL，必须由测试目标显式拉建，
# 否则全量 ctest 中 sh 报 "astrocs-stage2: not found"(rc=32512) 假红。
add_dependencies(phase2_ivar_wiring astrocs-stage2)

add_executable(calibrated_pair_diag tools/calibrated_pair_diag.cpp)
target_link_libraries(calibrated_pair_diag PRIVATE phase2
    ${P2_AIO_LINK})
set_target_properties(calibrated_pair_diag PROPERTIES EXCLUDE_FROM_ALL TRUE)

add_executable(rejection_cli tools/rejection_cli.cpp)
target_link_libraries(rejection_cli PRIVATE phase2)
set_target_properties(rejection_cli PROPERTIES EXCLUDE_FROM_ALL TRUE)

# V17：CUDA bridge DLL（nvcc 构建产物）复制到各 exe 目录（bridge loader
# 搜索路径 1 = exe 目录），保证 clean build 与增量 build 环境一致
# （GPU 不可用时 CudaEquivalent 之外其余 CUDA 测试已 GTEST_SKIP）。
set(_cuda_bridge "${CMAKE_CURRENT_SOURCE_DIR}/../../acr/backends/cuda/bridge/acr_cuda_bridge.dll")
if(EXISTS "${_cuda_bridge}")
  foreach(_t phase2_synthetic_gate astrocs-stage2 calibrated_pair_diag rejection_cli)
    add_custom_command(TARGET ${_t} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              "${_cuda_bridge}" "$<TARGET_FILE_DIR:${_t}>")
  endforeach()
endif()
