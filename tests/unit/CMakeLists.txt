# CORE-*/IO-*/CPU-* 单元测试 (G2/G3 任务逐步加入)
add_executable(core_contracts_test core_contracts_test.cpp)
target_include_directories(core_contracts_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(core_contracts_test PRIVATE astrocs_contracts)
add_test(NAME core_contracts COMMAND core_contracts_test)

add_executable(core_artifact_test core_artifact_test.cpp)
target_include_directories(core_artifact_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(core_artifact_test PRIVATE astrocs_core)
add_test(NAME core_artifact COMMAND core_artifact_test)

add_executable(core_module_test core_module_test.cpp)
target_include_directories(core_module_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(core_module_test PRIVATE astrocs_core)
add_test(NAME core_module COMMAND core_module_test)

add_executable(core_pipeline_test core_pipeline_test.cpp)
target_include_directories(core_pipeline_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(core_pipeline_test PRIVATE astrocs_core)
add_test(NAME core_pipeline COMMAND core_pipeline_test)

add_executable(core_context_test core_context_test.cpp)
target_include_directories(core_context_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(core_context_test PRIVATE astrocs_core)
add_test(NAME core_context COMMAND core_context_test)

add_executable(core_scheduler_test core_scheduler_test.cpp)
target_include_directories(core_scheduler_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(core_scheduler_test PRIVATE astrocs_core)
add_test(NAME core_scheduler COMMAND core_scheduler_test)

add_executable(core_checkpoint_test core_checkpoint_test.cpp)
target_include_directories(core_checkpoint_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(core_checkpoint_test PRIVATE astrocs_core)
add_test(NAME core_checkpoint COMMAND core_checkpoint_test)

add_executable(core_logging_test core_logging_test.cpp)
target_include_directories(core_logging_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(core_logging_test PRIVATE astrocs_core)
add_test(NAME core_logging COMMAND core_logging_test)

add_executable(rt001_abi_test rt001_abi_test.cpp)
target_include_directories(rt001_abi_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(rt001_abi_test PRIVATE astrocs_core)
add_test(NAME rt001_abi COMMAND rt001_abi_test)

add_executable(rt002_budget_test rt002_budget_test.cpp)
target_include_directories(rt002_budget_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(rt002_budget_test PRIVATE astrocs_core)
add_test(NAME rt002_budget COMMAND rt002_budget_test)

add_executable(rt003_context_test rt003_context_test.cpp)
target_include_directories(rt003_context_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(rt003_context_test PRIVATE astrocs_core)
add_test(NAME rt003_context COMMAND rt003_context_test)

add_executable(rt005_registry_test rt005_registry_test.cpp)
target_include_directories(rt005_registry_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party)
target_link_libraries(rt005_registry_test PRIVATE astrocs_module_adapters)
add_test(NAME rt005_registry COMMAND rt005_registry_test)

add_executable(rt006_scheduler_test rt006_scheduler_test.cpp)
target_include_directories(rt006_scheduler_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party)
target_link_libraries(rt006_scheduler_test PRIVATE astrocs_module_adapters astrocs_core)
add_test(NAME rt006_scheduler COMMAND rt006_scheduler_test)

add_executable(rt007_artifact_store_test rt007_artifact_store_test.cpp)
target_include_directories(rt007_artifact_store_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party)
target_link_libraries(rt007_artifact_store_test PRIVATE astrocs_core)
add_test(NAME rt007_artifact_store COMMAND rt007_artifact_store_test)

add_executable(rt008_runtime_client_test rt008_runtime_client_test.cpp)
target_include_directories(rt008_runtime_client_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../cli
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party)
target_link_libraries(rt008_runtime_client_test PRIVATE astrocs_cli_runtime)
add_test(NAME rt008_runtime_client COMMAND rt008_runtime_client_test)

# RT-009: 节点 trace 数据源（run 后 status/时间/workers/provider; IR 保留）
add_executable(rt009_node_trace_test rt009_node_trace_test.cpp)
target_include_directories(rt009_node_trace_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../cli
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party)
target_link_libraries(rt009_node_trace_test PRIVATE astrocs_cli_runtime)
add_test(NAME rt009_node_trace COMMAND rt009_node_trace_test)

# B8-P1: cli/process 跨平台子进程封装(argv 传参替代 std::system)共址单测
add_executable(cli_process_test cli_process_test.cpp)
target_include_directories(cli_process_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../cli
  ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party)
target_link_libraries(cli_process_test PRIVATE astrocs_cli_process)
add_test(NAME cli_process COMMAND cli_process_test)

# B8-P1-2: benchmark cpu verdict 推导(cli_common.h header-only)共址单测
add_executable(cli_bench_verdict_test cli_bench_verdict_test.cpp)
target_include_directories(cli_bench_verdict_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../cli
  ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party
  ${CMAKE_BINARY_DIR})
add_test(NAME cli_bench_verdict COMMAND cli_bench_verdict_test)

# B13-R13-4: WideCharToMultiByte 缓冲计算共址单测 (cli_common.h header-only)
add_executable(cli_wideconv_test cli_wideconv_test.cpp)
target_include_directories(cli_wideconv_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../cli
  ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party
  ${CMAKE_BINARY_DIR})
add_test(NAME cli_wideconv COMMAND cli_wideconv_test)

# B13-R13-5: gaia_client.c byte_unshuffle malloc 失败返回码化 — 共址测试
# (#include 源文件访问 static; OOM 注入用 malloc(SIZE_MAX) 标准必败路径)
add_executable(gaia_unshuffle_test gaia_unshuffle_test.c)
target_include_directories(gaia_unshuffle_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/gaia_xpsd_client/src)
find_package(OpenMP)
if(OpenMP_C_FOUND)
  target_link_libraries(gaia_unshuffle_test PRIVATE OpenMP::OpenMP_C)
endif()
target_link_libraries(gaia_unshuffle_test PRIVATE z m Threads::Threads)
add_test(NAME gaia_unshuffle COMMAND gaia_unshuffle_test)

add_executable(io_adapter_test io_adapter_test.cpp)
target_include_directories(io_adapter_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(io_adapter_test PRIVATE astrocs_io_adapter)  # BLD-003: 原 astrocs_io STATIC 更名
add_test(NAME io_adapter COMMAND io_adapter_test)

add_executable(io_ownership_test io_ownership_test.cpp)
target_include_directories(io_ownership_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/astro_image_io/include)
target_link_libraries(io_ownership_test PRIVATE astrocs_aio astrocs_core)
add_test(NAME io_ownership COMMAND io_ownership_test)

add_executable(io_reentrant_test io_reentrant_test.cpp)
target_include_directories(io_reentrant_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/astro_image_io/third_party/cfitsio)
if(MSVC AND ACS_ZLIB_ROOT)
  # HOSTFIX (WIN-BUILD LNK1104): 不可硬编码 libz.lib —— fatduck 本机 zlib-msvc
  # 布局是 libz.lib, hosted runner 的 vcpkg x64-windows-static-md 实际产物是
  # zs.lib/zlib.lib; 父 CMakeLists 的 ACS_ZLIB_LIBRARY (ACS_ZLIB_LIB 注入 /
  # find_library z/libz/zlib/zlibstatic/zs) 是唯一已解析的 zlib 库路径, 直接复用。
  target_link_libraries(io_reentrant_test PRIVATE astrocs_cfitsio "${ACS_ZLIB_LIBRARY}")
else()
  target_link_libraries(io_reentrant_test PRIVATE astrocs_cfitsio z m)
endif()
add_test(NAME io_reentrant COMMAND io_reentrant_test)

# B13-R13-3: executor observed_provider_ 数据竞争回归 (TSAN 可复现:
# nice -n 19 timeout 300 在 TSAN 构建下运行)。executor.cpp 不在 astrocs_core
# 内 (tests/runtime Python harness 独立编译), 此处按同组源闭包直编。
add_executable(executor_provider_race_test executor_provider_race_test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/core/src/executor.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/core/src/context.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/core/src/artifact.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/core/src/logging.cpp)
target_include_directories(executor_provider_race_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party)
target_link_libraries(executor_provider_race_test PRIVATE Threads::Threads)
add_test(NAME executor_provider_race COMMAND executor_provider_race_test)
# QA-002 (DEEP-SAN-TSAN io_reentrant 根因修复): vendored cfitsio 4.6.4 内部
# FFLOCK/FFUNLOCK 宏把 pthread_mutex_* 返回值写入全局诊断状态
# Fitsio_Pthread_Status (fitsio2.h:25-26) — 该写自身不受任何锁保护, 是上游
# 固有模式(仅存诊断返回码, 不参与数据流; 锁互斥语义正确)。TSan 对该写写
# 竞争必然报 race (ffopen/fits_store_Fptr/fits_clear_Fptr, 全部位于
# third_party 内部), 测试进程以非零退出码收场 → ctest FAIL。
# 治理: 仓库既有先例 tools/quality/tsan/cfitsio.supp (QA-002, owner+expiry
# 注明仅限第三方 cfitsio 内部符号, 自有代码 race 一律不豁免) — configure 时
# 复制进 build 树再挂到测试环境。必须走 build 树副本: sanitizer flag 解析按
# 空白切 token, 源树路径若含空格 (如本地 checkout) 会破坏 TSAN_OPTIONS。
# 非 TSan 构建下该环境变量无运行时消费, 无副作用。
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/tsan)
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/quality/tsan/cfitsio.supp
  ${CMAKE_BINARY_DIR}/tsan/cfitsio.supp COPYONLY)
set_tests_properties(io_reentrant PROPERTIES
  ENVIRONMENT "TSAN_OPTIONS=suppressions=${CMAKE_BINARY_DIR}/tsan/cfitsio.supp")

add_executable(cpu_abi_test cpu_abi_test.cpp)
target_include_directories(cpu_abi_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(cpu_abi_test PRIVATE astrocs_cpu astrocs_contracts)
add_test(NAME cpu_abi COMMAND cpu_abi_test)

add_executable(cpu_lease_test cpu_lease_test.cpp)
target_include_directories(cpu_lease_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/backend_host)
target_link_libraries(cpu_lease_test PRIVATE astrocs_cpu astrocs_contracts Threads::Threads)
add_test(NAME cpu_lease COMMAND cpu_lease_test)

# P1-4 baseline kernels 异常屏障注入单测: TU 匿名命名空间 include
# baseline_kernels_impl.inc 直调 run_banded 注入可抛 body(与 OpComputer 同一
# run_body_guarded → std::thread 通路), 断言不 terminate + ACS_ERR_INTERNAL +
# 租借/归还配平。仅需头文件与产品 .inc, 链 astrocs_cpu 取 abi_boundary_probe。
add_executable(cpu_backend_exception_test cpu_backend_exception_test.cpp)
target_include_directories(cpu_backend_exception_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/backend_host)
target_link_libraries(cpu_backend_exception_test PRIVATE astrocs_cpu astrocs_contracts Threads::Threads)
add_test(NAME cpu_backend_exception COMMAND cpu_backend_exception_test)

add_executable(cpu_features_test cpu_features_test.cpp)
target_include_directories(cpu_features_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/backend_host)
target_link_libraries(cpu_features_test PRIVATE astrocs_cpu astrocs_contracts)
add_test(NAME cpu_features COMMAND cpu_features_test)

add_executable(cpu_provider_test cpu_provider_test.cpp)
target_include_directories(cpu_provider_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/backend_host)
target_link_libraries(cpu_provider_test PRIVATE astrocs_cpu astrocs_contracts)
add_test(NAME cpu_provider COMMAND cpu_provider_test)

add_executable(cpu_fallback_test cpu_fallback_test.cpp)
target_include_directories(cpu_fallback_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/backend_host)
target_link_libraries(cpu_fallback_test PRIVATE astrocs_cpu astrocs_contracts)
add_test(NAME cpu_fallback COMMAND cpu_fallback_test)

add_executable(cpu_bench_test cpu_bench_test.cpp)
target_include_directories(cpu_bench_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/backend_host)
target_link_libraries(cpu_bench_test PRIVATE astrocs_cpu astrocs_contracts)
add_test(NAME cpu_bench COMMAND cpu_bench_test)

add_executable(cpu_profile_test cpu_profile_test.cpp)
target_include_directories(cpu_profile_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/backend_host)
target_link_libraries(cpu_profile_test PRIVATE astrocs_cpu astrocs_contracts)
add_test(NAME cpu_profile COMMAND cpu_profile_test)

# CPU-003 (G3): v2 profile 生成与复读
add_executable(cpu003_profile_v2_test cpu003_profile_v2_test.cpp)
target_include_directories(cpu003_profile_v2_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/backend_host)
target_link_libraries(cpu003_profile_v2_test PRIVATE astrocs_cpu astrocs_contracts)
add_test(NAME cpu003_profile_v2 COMMAND cpu003_profile_v2_test)

# CPU-006 (G3, V7.1): benchmark report (aggregate/verdict/verify, 正例+负向样例)
add_executable(cpu006_bench_report_test cpu006_bench_report_test.cpp)
target_include_directories(cpu006_bench_report_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/backend_host)
target_link_libraries(cpu006_bench_report_test PRIVATE astrocs_cpu astrocs_contracts)
add_test(NAME cpu006_bench_report COMMAND cpu006_bench_report_test)

# CPU-007 (G3, V7.1): profile 存储生命周期 (原子写/失效隔离/负向样例)
add_executable(cpu007_profile_store_test cpu007_profile_store_test.cpp)
target_include_directories(cpu007_profile_store_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/backend_host)
target_link_libraries(cpu007_profile_store_test PRIVATE astrocs_cpu astrocs_contracts)
add_test(NAME cpu007_profile_store COMMAND cpu007_profile_store_test)

# CPU-008 (G3, V7.1): 自适应线程建议 (no fixed cores/cgroup-job 模拟/内存预算/负向样例)
add_executable(cpu008_worker_advisor_test cpu008_worker_advisor_test.cpp)
target_include_directories(cpu008_worker_advisor_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/backend_host)
target_link_libraries(cpu008_worker_advisor_test PRIVATE astrocs_cpu astrocs_contracts)
add_test(NAME cpu008_worker_advisor COMMAND cpu008_worker_advisor_test)

# CPU-004 (G3): 逐 kernel 自适应路由
add_executable(cpu004_routing_test cpu004_routing_test.cpp)
target_include_directories(cpu004_routing_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/backend_host)
target_link_libraries(cpu004_routing_test PRIVATE astrocs_cpu astrocs_contracts)
add_test(NAME cpu004_routing COMMAND cpu004_routing_test)

# MON-001 (G3): 资源记录器(采样/阶段分段/统计/三产物)
add_executable(mon001_recorder_test mon001_recorder_test.cpp)
target_include_directories(mon001_recorder_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../cli)
target_link_libraries(mon001_recorder_test PRIVATE astrocs_cpu astrocs_contracts)
add_test(NAME mon001_recorder COMMAND mon001_recorder_test)

# MON-002 (G3): 资源门禁(compute/io/memory/mixed/first-10s/锁退化)
add_executable(mon002_gate_test mon002_gate_test.cpp)
target_include_directories(mon002_gate_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../cli)
target_link_libraries(mon002_gate_test PRIVATE astrocs_cpu astrocs_contracts)
add_test(NAME mon002_gate COMMAND mon002_gate_test)

# MON-002 (G3, V7 04_CPU_RESOURCE_TASKS): RSS/allocation report 内存泄露/增长检测
# (定期采样 RSS/private/commit/allocator outstanding; cache/高水位/工作集三分类;
# 结束回落验证; 原始曲线; 哨兵纪律; 注入leak失败/伪造monitor/格式篡改负向)
add_executable(mon002_alloc_test mon002_alloc_test.cpp)
target_include_directories(mon002_alloc_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../cli)
target_link_libraries(mon002_alloc_test PRIVATE astrocs_cpu astrocs_contracts)
add_test(NAME mon002_alloc COMMAND mon002_alloc_test)

# MON-001 (G3, V7.1): 资源阈值判定(监控缺失FAIL/70%样本U>=0.75/队列10s U<0.50/
# utilization_value 无硬编码分母/哨兵自洽)
add_executable(mon001_gate_test mon001_gate_test.cpp)
target_include_directories(mon001_gate_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../cli)
target_link_libraries(mon001_gate_test PRIVATE astrocs_cpu astrocs_contracts)
add_test(NAME mon001_gate COMMAND mon001_gate_test)

# P1-001 (G4): Phase1 8 类模块注册(端口/执行类/Runtime 委托)
add_executable(p1001_modules_test p1001_modules_test.cpp)
target_include_directories(p1001_modules_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party)
target_link_libraries(p1001_modules_test PRIVATE astrocs_module_adapters astrocs_core)
add_test(NAME p1001_modules COMMAND p1001_modules_test)

# CPU-001 (G3): 独立 provider 构建 — 三份 self-test + 负例
# 每个 provider 静态库各自链接进独立可执行, 运行共享 backend_table.inc 的
# backend_self_test(handshake→allocator→cancel→budget→logger); -R "cpu0" 可全跑。
# 注意: 三个 provider 库都导出 astrocs_backend_get_api_v1(共享 table.inc 的同一符号),
# 因此每个可执行只能链接其中一个 provider 库(禁止混合链接, 否则重复符号)。
# avx2/avx512 库不含 host_services → 源显式加入; baseline 在 astrocs_cpu 内。
set(CPU001_SRC ${CMAKE_CURRENT_SOURCE_DIR}/cpu001_provider_selftest.cpp)
add_executable(cpu001_provider_selftest_baseline ${CPU001_SRC})
target_include_directories(cpu001_provider_selftest_baseline PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/backend_host)
target_link_libraries(cpu001_provider_selftest_baseline PRIVATE
  astrocs_cpu astrocs_contracts astrocs_common)
add_test(NAME cpu001_selftest_baseline COMMAND cpu001_provider_selftest_baseline)

add_executable(cpu001_provider_selftest_avx2 ${CPU001_SRC}
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/backend_host/host_services.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/backend_host/cpu_features.cpp)
target_include_directories(cpu001_provider_selftest_avx2 PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/backend_host)
target_link_libraries(cpu001_provider_selftest_avx2 PRIVATE
  astrocs_cpu_avx2 astrocs_contracts astrocs_common)
add_test(NAME cpu001_selftest_avx2
  COMMAND cpu001_provider_selftest_avx2 avx2)

add_executable(cpu001_provider_selftest_avx512 ${CPU001_SRC}
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/backend_host/host_services.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/backend_host/cpu_features.cpp)
target_include_directories(cpu001_provider_selftest_avx512 PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/backend_host)
target_link_libraries(cpu001_provider_selftest_avx512 PRIVATE
  astrocs_cpu_avx512 astrocs_contracts astrocs_common)
add_test(NAME cpu001_selftest_avx512
  COMMAND cpu001_provider_selftest_avx512 avx512)
# CPU-001 avx512 治理 (DEEP-SAN-ASAN/TSAN/COV + WIN-TEST-UNIT 同一失败):
# provider 激活合同 = required ⊆ detected 否则拒绝 (backend_loader.cpp ⑤
# "unsupported ISA...不尝试执行"); CPU-004 capability gate 同语义 (无 AVX-512
# hw/OS ZMM state → 拒绝)。无 AVX-512F 硬件的 host 上 avx512 provider 本就不
# 激活 — selftest 对"backend 未激活"走 SKIP (返回 77) 而非 FAIL;
# 有硬件时全部断言 (含 self_test/kernel 表) 不变, avx2/baseline 不动。
set_tests_properties(cpu001_selftest_avx512 PROPERTIES SKIP_RETURN_CODE 77)

# 负例: feature bits 不满足的 provider 加载被拒(用 astrocs_cpu 内的 backend_loader 预检)
add_executable(cpu001_negative_test cpu001_negative_test.cpp)
target_include_directories(cpu001_negative_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/backend_host)
target_link_libraries(cpu001_negative_test PRIVATE
  astrocs_cpu astrocs_contracts astrocs_common)
add_test(NAME cpu001_negative COMMAND cpu001_negative_test)

# G9: OpenMP 可选 (clang Debug 无 libomp 时降级串行; P2_ENABLE_OPENMP 默认 OFF)
find_package(OpenMP QUIET)
if(OpenMP_CXX_FOUND)
  set(ASTROCS_OMP_TARGET OpenMP::OpenMP_CXX)
else()
  set(ASTROCS_OMP_TARGET "")
endif()

add_executable(cpu_monitor_test cpu_monitor_test.cpp)
target_include_directories(cpu_monitor_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../cli)
target_link_libraries(cpu_monitor_test PRIVATE astrocs_contracts Threads::Threads)
add_test(NAME cpu_monitor COMMAND cpu_monitor_test)

add_executable(p1_calibration_test p1_calibration_test.cpp)
target_include_directories(p1_calibration_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/calibration/include)
target_link_libraries(p1_calibration_test PRIVATE astrocs_calibration ${ASTROCS_OMP_TARGET})
add_test(NAME p1_calibration COMMAND p1_calibration_test)

# B13-R13-7: master flat 负中位数拒绝 (SCIENCE: median<=0 不归一化, 修常数 0.1 假主帧)
add_executable(master_flat_median_test master_flat_median_test.cpp)
target_include_directories(master_flat_median_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/calibration/include)
target_link_libraries(master_flat_median_test PRIVATE astrocs_calibration ${ASTROCS_OMP_TARGET})
add_test(NAME master_flat_median COMMAND master_flat_median_test)

# ----------------------------------------------------------------------------
# P1-CAL-TEST (TEST-P1-CAL-001, 控制包 queue 31, lock-P1-CAL) — lib/calibration
# 可复用验证面。结构与注册详见 lib/calibration/tests/p1cal/CMakeLists.txt
# (FIX-CAL-A..F fixture / 独立 oracle / negative / 1-N worker / performance /
# 故障注入自检); 合同锚 ALG-CAL §9 TEST-CAL-DESIGN-001 (P1-CAL-DOC 冻结)。
# 注意: p1cal_selfcheck_test 与 p1cal_tests 共享 p1cal_tests_core.cpp TU。
# ----------------------------------------------------------------------------
add_subdirectory(../../lib/calibration/tests/p1cal p1cal)

# ----------------------------------------------------------------------------
# P1-COS-TEST (TEST-P1-COS-001, 控制包 queue 34, lock-P1-COS) — lib/cosmetic
# 可复用验证面。结构与注册详见 lib/cosmetic/tests/p1cos/CMakeLists.txt
# (FIX-COS-A..F fixture / 独立 oracle / negative / 1-N worker / performance /
# 故障注入自检); 合同锚 ALG-COS §9 TEST-COS-DESIGN-001 (P1-COS-DOC 冻结)。
# 注意: p1cos_selfcheck_test 与 p1cos_tests 共享 p1cos_tests_core.cpp TU;
# 被测面为 astrocs_calibration 的 ac_correct_frame(+_f64) 现状生产实现。
# ----------------------------------------------------------------------------
add_subdirectory(../../lib/cosmetic/tests/p1cos p1cos)

# ----------------------------------------------------------------------------
# P1-DRZ-TEST (TEST-P1-DRZ-001, 控制包 queue 37, lock-P1-DRZ) —
# lib/healpix_db/healpix_drizzle 可复用验证面。结构与注册详见
# lib/healpix_db/healpix_drizzle/tests/p1drz/CMakeLists.txt (FIX-DRZ-A..F
# fixture / 独立 oracle / negative / performance / 故障注入自检); 合同锚
# ALG-DRZ §9 TEST-DRZ-DESIGN-001 (P1-DRZ-DOC 冻结) + SCI-DRZ-001 §5/§7/§8。
# 注意: p1drz_selfcheck_test 与 p1drz_tests 共享 p1drz_tests_core.cpp TU;
# 域基线 09ee5363 (NaN/Inf 传播), 与 drizzle_nonfinite_test 互补不重复。
# ----------------------------------------------------------------------------
add_subdirectory(../../lib/healpix_db/healpix_drizzle/tests/p1drz p1drz)

# ----------------------------------------------------------------------------
# P1-HIPS-TEST (TEST-P1-HIPS-001, 控制包 queue 39, lock-P1-HIPS) —
# lib/astro_image_io/src/hips 可复用验证面。结构与注册详见
# lib/astro_image_io/tests/p1hips/CMakeLists.txt (FIX-HIPS-A..F fixture /
# 独立 oracle / negative / 1-N worker / performance / 故障注入自检); 合同锚
# HIPS_WRITER.md §9 TEST-HIPS-DESIGN-001 (P1-HIPS-DOC 冻结)。
# 注意: p1hips_selfcheck_test 与 p1hips_tests 共享四组测试 TU; 被测面为
# astrocs_hips 的 C ABI 九导出现状生产实现 (API-HIPS-001)。
# ----------------------------------------------------------------------------
add_subdirectory(../../lib/astro_image_io/tests/p1hips p1hips)

# ----------------------------------------------------------------------------
# P1-PHOT-TEST (TEST-P1-PHOT-001, lock-P1-PHOT) — lib/photometric_calib
# 可复用验证面。结构与注册详见 lib/photometric_calib/tests/p1phot/
# CMakeLists.txt (FIX-PHOT-A..F fixture / 独立 oracle / negative /
# I5 线程扫描 / performance / 故障注入自检); 合同锚
# PHOTOMETRIC_FIT.md §13.4 TEST-PHOT-DESIGN-001 (P1-PHOT-DOC 冻结) +
# SCI-PHOT-001 §11 冻结容差。注意: p1phot_selfcheck_test 与 p1phot_tests
# 共享四组测试 TU; 被测面为 lib/photometric_calib/cpp/src 五生产 TU
# (API-PHOT-001 六导出) + lib/phase1/photometry/photometer.cpp (F6 legacy,
# 只读); gaia_client 以测试桩替身接入 (外部域 CAT-GAIA, 生产源零修改)。
# ----------------------------------------------------------------------------
add_subdirectory(../../lib/photometric_calib/tests/p1phot p1phot)

# ----------------------------------------------------------------------------
# P1-NOISE-TEST (TEST-P1-NOISE-001, 控制包 queue 41, lock-P1-NOISE) —
# lib/snr_estimator (astrocs.p1.noise-snr) 可复用验证面。结构与注册详见
# lib/snr_estimator/tests/p1noise/CMakeLists.txt (FIX-NOISE-A..G fixture /
# 独立 oracle / negative / performance / 故障注入自检); 合同锚
# NOISE_ESTIMATION.md §13.4 TEST-NOISE-DESIGN-001 (P1-NOISE-DOC 冻结)。
# 注意: p1noise_selfcheck_test 与 p1noise_tests 共享 p1noise_tests_core.cpp
# TU; 被测面为 lib/snr_estimator/cpp/src/noise_model.cpp 现状生产实现
# (根构建无 snr_estimator 目标, 测试面独立编译 astrocs_p1_noise_prod,
# 生产源零改动; lib/snr_estimator/ 为 P1-NOISE 迁移目标目录)。
# 本块由 P1-NOISE-TEST agent 单独追加 (多 agent 并发追加区, 只增不改)。
# ----------------------------------------------------------------------------
add_subdirectory(../../lib/snr_estimator/tests/p1noise p1noise)

# ----------------------------------------------------------------------------
# P1-HIPS-IMPL (SA-P1H-I, queue 40, lock-P1-HIPS): hips_writer 模块适配器
#   测试 — direct 通道链 astrocs_hips+astrocs_aio STATIC (legacy aio_hips_*
#   九导出面, 与 p1_hips_writer_test 同口径) vs DLL C ABI 适配器
#   astrocs_p1_hips_writer (唯一导出 astrocs_module_query_v1, 环境变量
#   ASTROCS_HIPS_DLL_PATH 加载): 入口契约/导出面净化(legacy 九符号不得
#   dlsym; nm -D 唯一导出另由构建检查实证)/describe/validate 512-5 负例/
#   plan 真实 work_units/write_product direct-vs-plugin 产物树逐文件
#   size 一致/executor 硬租约 BUDGET 105/entry cancel (DISP-HIPS-001:
#   abort 不清理已写文件)/strbuf 尺寸探测+不足/allocator 分配失败负面
#   (批次 R malloc hardening 合同面)。DLL 路径经 ENVIRONMENT 注入
#   (生成器表达式, 兼容含空格构建路径)。
# ----------------------------------------------------------------------------
add_executable(hips_writer_adapter_test p1_hips/adapter_test.c
  p1_hips/adapter_entry_impl.cpp
  ../../lib/hips/src/aio_publish.cpp)
target_include_directories(hips_writer_adapter_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/astro_image_io/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/hips/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/hips/src)
target_link_libraries(hips_writer_adapter_test PRIVATE
  astrocs_hips astrocs_aio ${CMAKE_DL_LIBS})
add_dependencies(hips_writer_adapter_test astrocs_p1_hips_writer)
add_test(NAME hips_writer_adapter COMMAND hips_writer_adapter_test
  ${CMAKE_CURRENT_BINARY_DIR}/run/t_hips_adapter
  ${CMAKE_CURRENT_BINARY_DIR}/run/t_hips_ref)
set_tests_properties(hips_writer_adapter PROPERTIES
  ENVIRONMENT "ASTROCS_HIPS_DLL_PATH=$<TARGET_FILE:astrocs_p1_hips_writer>")

# --- AIO-002: HiPS 原子发布与临时目录清理 (staging→校验→fsync→原子 promote) --
# 被测面: lib/hips/src/aio_publish.cpp (publish.h v1 合同, 4 原语唯一实现) +
# module_entry.cpp 事务面 (write_product 全链 staging 重定向)。
# 组: units (4 原语正/负/幂等) / atomic (正向发布 + 注入必败 + cancel/kill
# 中断无 partial + RAII 自愈; 注入 ASTROCS_HIPS_PUBLISH_FAULT=registry)。
add_executable(hips_publish_atomic_test p1_hips/publish_atomic_test.c
  p1_hips/adapter_entry_impl.cpp
  ../../lib/hips/src/aio_publish.cpp)
target_include_directories(hips_publish_atomic_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/astro_image_io/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/hips/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/hips/src)
target_link_libraries(hips_publish_atomic_test PRIVATE
  astrocs_hips astrocs_aio ${CMAKE_DL_LIBS})
add_dependencies(hips_publish_atomic_test astrocs_p1_hips_writer)
add_test(NAME hips_publish_atomic_units COMMAND hips_publish_atomic_test
  units ${CMAKE_CURRENT_BINARY_DIR}/run/t_hips_publish)
add_test(NAME hips_publish_atomic COMMAND hips_publish_atomic_test
  atomic ${CMAKE_CURRENT_BINARY_DIR}/run/t_hips_publish)
# --- AIO-002 end ------------------------------------------------------------

add_executable(p1_stars_test p1_stars_test.cpp)
target_include_directories(p1_stars_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/phase1/stars)
target_link_libraries(p1_stars_test PRIVATE astrocs_phase1_stars)
add_test(NAME p1_stars COMMAND p1_stars_test)

add_executable(p1_wcs_phot_test p1_wcs_phot_test.cpp)
target_include_directories(p1_wcs_phot_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/phase1/wcs
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/phase1/photometry)
target_link_libraries(p1_wcs_phot_test PRIVATE astrocs_phase1_wcs astrocs_phase1_phot)
add_test(NAME p1_wcs_phot COMMAND p1_wcs_phot_test)

add_executable(p1_noise_test p1_noise_test.cpp)
target_include_directories(p1_noise_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/phase1/noise)
target_link_libraries(p1_noise_test PRIVATE astrocs_phase1_noise)
add_test(NAME p1_noise COMMAND p1_noise_test)

add_executable(p1_nside_test p1_nside_test.cpp)
target_include_directories(p1_nside_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/common/healpix
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/phase3_session)
target_link_libraries(p1_nside_test PRIVATE astrocs_common astrocs_phase3_session astrocs_hips astrocs_contracts)
add_test(NAME p1_nside COMMAND p1_nside_test)

# R9-B (bug 狩猎 P1 batchJ): healpix NESTED neighbors corner 分支 + query_disc
# 完备性修复验证; 源文件共址 lib/common/healpix/tests/ (仓库现行测试装配模式)
add_executable(r9b_healpix_neighbors_test ../../lib/common/healpix/tests/test_healpix_neighbors.cpp)
target_link_libraries(r9b_healpix_neighbors_test PRIVATE astrocs_common)
add_test(NAME r9b_healpix_neighbors COMMAND r9b_healpix_neighbors_test)

add_executable(p1_hips_writer_test p1_hips_writer_test.cpp)
target_include_directories(p1_hips_writer_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/astro_image_io/include)
target_link_libraries(p1_hips_writer_test PRIVATE astrocs_hips astrocs_aio)
add_test(NAME p1_hips_writer COMMAND p1_hips_writer_test)

# ----------------------------------------------------------------------------
add_executable(p1_ir_facade_test p1_ir_facade_test.cpp)
target_include_directories(p1_ir_facade_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/calibration/include)
target_link_libraries(p1_ir_facade_test PRIVATE astrocs_calibration ${ASTROCS_OMP_TARGET})
add_test(NAME p1_ir_facade COMMAND p1_ir_facade_test)

# P1 batchD (bug 狩猎 R6): session manifest 写穿 + cosmetic 值类型合同
# (源文件共址 lib/phase1_session/tests/, 经本文件注册 — 仓库现行测试装配模式)
add_executable(p1_session_manifest_test ../../lib/phase1_session/tests/test_p1_session_manifest.cpp)
target_include_directories(p1_session_manifest_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/phase1_session
  ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/calibration/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/astro_image_io/include)
target_link_libraries(p1_session_manifest_test PRIVATE astrocs_phase1_session)
add_test(NAME p1_session_manifest COMMAND p1_session_manifest_test)

# P1-SESSION-TEST (TEST-P1-SESSION-001, 控制包 queue 47, lock-P1-SESSION) —
# lib/phase1_session 可复用验证面。结构与注册详见
# lib/phase1_session/tests/p1sess/CMakeLists.txt (FIX-SESS-A..F fixture /
# 独立 oracle / negative / 1-N worker / performance / 故障注入自检); 合同锚
# API-P1-SESSION / DATA-P1-SESSION (P1-SESSION-DOC 冻结) + tasks/
# 05_PHASE1_TASKS.md §3 (验收: each node once and failure propagation)。
# 注意: p1sess_selfcheck_test 与 p1sess_tests 共享 units/negative 测试 TU;
# 被测面为 p1_session 五导出 C API 现状生产实现 (astrocs_phase1_session
# 静态库), manifest stages 即 direct 面节点调用账本。
# ----------------------------------------------------------------------------
add_subdirectory(../../lib/phase1_session/tests/p1sess p1sess)


# ----------------------------------------------------------------------------

add_executable(p1_resource_test p1_resource_test.cpp)
target_include_directories(p1_resource_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/calibration/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../cli)
target_link_libraries(p1_resource_test PRIVATE astrocs_calibration ${ASTROCS_OMP_TARGET} Threads::Threads)
add_test(NAME p1_resource COMMAND p1_resource_test)

add_executable(p2_workers_test p2_workers_test.cpp)
target_include_directories(p2_workers_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../cli
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/phase2/include)
target_link_libraries(p2_workers_test PRIVATE astrocs_phase2 astrocs_contracts ${ASTROCS_OMP_TARGET})
add_test(NAME p2_workers COMMAND p2_workers_test)

add_executable(p2_upm_synthetic_test p2_upm_synthetic_test.cpp)
target_link_libraries(p2_upm_synthetic_test PRIVATE astrocs_contracts)
add_test(NAME p2_upm_synthetic COMMAND p2_upm_synthetic_test)

add_executable(p2_block_plan_test p2_block_plan_test.cpp)
target_include_directories(p2_block_plan_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/phase2/include)
target_link_libraries(p2_block_plan_test PRIVATE astrocs_phase2)
add_test(NAME p2_block_plan COMMAND p2_block_plan_test)

add_executable(p2_rejection_test p2_rejection_test.cpp)
target_include_directories(p2_rejection_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/phase2/include)
target_link_libraries(p2_rejection_test PRIVATE astrocs_phase2)
add_test(NAME p2_rejection COMMAND p2_rejection_test)

add_executable(p2_output_semantics_test p2_output_semantics_test.cpp)
target_include_directories(p2_output_semantics_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/phase2/include)
target_link_libraries(p2_output_semantics_test PRIVATE astrocs_phase2)
add_test(NAME p2_output_semantics COMMAND p2_output_semantics_test)

add_executable(p2_ir_facade_test p2_ir_facade_test.cpp)
target_link_libraries(p2_ir_facade_test PRIVATE astrocs_contracts)
add_test(NAME p2_ir_facade COMMAND p2_ir_facade_test)

add_executable(p2_seam_gate_test p2_seam_gate_test.cpp)
target_include_directories(p2_seam_gate_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../cli)
target_link_libraries(p2_seam_gate_test PRIVATE astrocs_contracts)
add_test(NAME p2_seam_gate COMMAND p2_seam_gate_test)

add_executable(p3_wcs_test p3_wcs_test.cpp)
target_include_directories(p3_wcs_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/phase3_session)
target_link_libraries(p3_wcs_test PRIVATE astrocs_phase3_session astrocs_common)
add_test(NAME p3_wcs COMMAND p3_wcs_test)

add_executable(p3_interp_test p3_interp_test.cpp)
target_link_libraries(p3_interp_test PRIVATE astrocs_contracts)
add_test(NAME p3_interp COMMAND p3_interp_test)

add_executable(p3_coverage_test p3_coverage_test.cpp)
target_link_libraries(p3_coverage_test PRIVATE astrocs_contracts)
add_test(NAME p3_coverage COMMAND p3_coverage_test)

add_executable(p3_output_test p3_output_test.cpp)
target_include_directories(p3_output_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/phase3_session
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/astro_image_io/include)
target_link_libraries(p3_output_test PRIVATE astrocs_phase3_session astrocs_hips astrocs_common)
add_test(NAME p3_output COMMAND p3_output_test)

add_executable(p3_assembly_test p3_assembly_test.cpp)
target_include_directories(p3_assembly_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/phase3_session
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/astro_image_io/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/common/healpix)
target_link_libraries(p3_assembly_test PRIVATE astrocs_phase3_session astrocs_hips astrocs_aio astrocs_common)
add_test(NAME p3_assembly COMMAND p3_assembly_test)

# 依赖 ASTROCS_REPO 定位仓库内资源文件的测试统一注入仓库根路径，
# 保证 ctest 无论从哪个工作目录启动均可解析 (QA-001 ctest 回归全绿)
set_tests_properties(
  core_pipeline cpu_provider cpu_bench io_adapter p1_ir_facade p2_ir_facade
  PROPERTIES ENVIRONMENT "ASTROCS_REPO=${CMAKE_SOURCE_DIR}")

# ----------------------------------------------------------------------------
# bug 狩猎 R13 P2-1/P2-2: 批次 K/I 产物共址测试注册 (此前从未进构建/ctest)
# ----------------------------------------------------------------------------
# TEST-CAL-001 (批次 K): photometry_apply.cpp 不在 astrocs_calibration 库源
# 列表 (根 CMakeLists calibration 段), 与测试 TU 一并编译; HAS_ZSTD 不加 —
# 根 CMake 从未为 astrocs_hips/astrocs_aio 定义该宏, hiss_*/aio_compressor
# 以无 zstd 分支自洽 (ZSTD 未编译时 writer 保持 RAW), 测试 TU 自身亦无
# zstd 依赖, 与库既有编译形态一致。
add_executable(test_photometry_apply_test
  ../../lib/calibration/tests/test_photometry_apply.cpp
  ../../lib/calibration/src/photometry_apply.cpp)
target_include_directories(test_photometry_apply_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/calibration/src
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/calibration/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/astro_image_io/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/astro_image_io/src
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/common)
target_link_libraries(test_photometry_apply_test PRIVATE
  astrocs_calibration astrocs_hips astrocs_aio astrocs_common ${ASTROCS_OMP_TARGET})
add_test(NAME cal_photometry_apply COMMAND test_photometry_apply_test)

# P1 batchI 恶意 fixture (FP64 硬失败/HCSD JSON 上限/n_pix 2^62/恶意
# tile_width 2048 拒绝): AIO_ENABLE_HEALPIX 仅为测试 TU 提供 aio_healpix_io.h
# 声明 (实现在 astrocs_hips, 其已带同宏); HAS_ZSTD 不加 — fixture 在无
# zstd 时走"伪压缩块"分支, 测试 TU 自洽, 与库一致。
add_executable(p1_io_hardening_test ../../lib/astro_image_io/tests/test_p1_io_hardening.cpp)
target_include_directories(p1_io_hardening_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/astro_image_io/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/astro_image_io/src
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/common
  ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party)
target_compile_definitions(p1_io_hardening_test PRIVATE AIO_ENABLE_HEALPIX)
target_link_libraries(p1_io_hardening_test PRIVATE astrocs_aio astrocs_hips astrocs_common)
add_test(NAME p1_io_hardening COMMAND p1_io_hardening_test)

# ----------------------------------------------------------------------------
# P1-5: phase2 run manifest artifacts 收集回归 (collect_node_artifact_paths)
# ----------------------------------------------------------------------------
# 旧 cmd_phase2_run 按 node_id=="res" 过滤节点 manifest（P2-006 Canonical IR
# 7 节点链 coverage/…/write 无此 id）→ run manifest artifacts 恒空而仍标
# complete。修复后按 manifest 内容收集（与 phase3 同型）；本测试在收集函数
# 层级注入 IR 节点 manifest（真实链 node id + 真实临时文件证据），端到端
# run manifest 证据由 tests/cli/test_phase2_inprocess.py 覆盖。
add_executable(p15_artifact_collect_test p15_artifact_collect_test.cpp)
target_include_directories(p15_artifact_collect_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../cli)
target_link_libraries(p15_artifact_collect_test PRIVATE astrocs_cli_runtime)
add_test(NAME p15_artifact_collect COMMAND p15_artifact_collect_test)


# ----------------------------------------------------------------------------
# CAT-GAIA-TEST (TEST-GAIA-001): gaia_xpsd_client 验证测试面
#   固定 seed fixture 生成器在构建期现场生成（无二进制 fixture 提交）；
#   gaia_cat_test 七模式（unit/properties/negative/worker/dump/truncate/
#   performance，#include gaia_client.c 启用 GAIA_ALLOC_TEST 钩子）；
#   gaia_cat_polar_ref 为 GAIA_POLAR_PRUNE_DISABLED 差分参考；oracle
#   (tests/oracle/gaia_oracle.py) 独立复算比对。KI-1/KI-2 见
#   gaia_cat_test.c 文件头（测试内登记，不修复生产代码）。
# ----------------------------------------------------------------------------
set(GAIA_CAT_DATA ${CMAKE_CURRENT_BINARY_DIR}/gaia_cat_test_data)
add_executable(gaia_xpsd_fixture_gen gaia_xpsd_fixture_gen.c)
target_link_libraries(gaia_xpsd_fixture_gen PRIVATE z m)
set(GAIA_CAT_CLEAN ${GAIA_CAT_DATA}/cat/clean)
set(GAIA_CAT_TRUNC ${GAIA_CAT_DATA}/cat/truncdb)
set(GAIA_CAT_FIXTURES
  ${GAIA_CAT_DATA}/inc/gaia_cat_manifest.h
  ${GAIA_CAT_CLEAN}/gaia_sp.xpsd
  ${GAIA_CAT_CLEAN}/gaia_dr3.xpsd
  ${GAIA_CAT_CLEAN}/manifest.ndjson
  ${GAIA_CAT_TRUNC}/truncate.xpsd)
add_custom_command(
  OUTPUT ${GAIA_CAT_FIXTURES}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${GAIA_CAT_DATA}/inc
          ${GAIA_CAT_CLEAN} ${GAIA_CAT_TRUNC} ${GAIA_CAT_DATA}/work
  COMMAND gaia_xpsd_fixture_gen ${GAIA_CAT_CLEAN}
          --header ${GAIA_CAT_DATA}/inc/gaia_cat_manifest.h
  COMMAND ${CMAKE_COMMAND} -E rm -f ${GAIA_CAT_CLEAN}/truncate.xpsd
  COMMAND gaia_xpsd_fixture_gen ${GAIA_CAT_TRUNC}
  COMMAND ${CMAKE_COMMAND} -E rm -f ${GAIA_CAT_TRUNC}/gaia_sp.xpsd
          ${GAIA_CAT_TRUNC}/gaia_dr3.xpsd ${GAIA_CAT_TRUNC}/manifest.ndjson
  DEPENDS gaia_xpsd_fixture_gen
  VERBATIM)
add_custom_target(gaia_cat_fixtures DEPENDS ${GAIA_CAT_FIXTURES})

add_executable(gaia_cat_test gaia_cat_test.c)
target_include_directories(gaia_cat_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/gaia_xpsd_client/src
  ${GAIA_CAT_DATA}/inc)
add_executable(gaia_cat_polar_ref gaia_cat_test.c)
target_compile_definitions(gaia_cat_polar_ref PRIVATE GAIA_POLAR_PRUNE_DISABLED)
target_include_directories(gaia_cat_polar_ref PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/gaia_xpsd_client/src
  ${GAIA_CAT_DATA}/inc)
find_package(OpenMP)
foreach(gt gaia_cat_test gaia_cat_polar_ref)
  if(OpenMP_C_FOUND)
    target_link_libraries(${gt} PRIVATE OpenMP::OpenMP_C)
  endif()
  target_link_libraries(${gt} PRIVATE z m Threads::Threads)
  add_dependencies(${gt} gaia_cat_fixtures)
endforeach()

add_test(NAME gaia_cat_unit
  COMMAND gaia_cat_test unit ${GAIA_CAT_CLEAN})
add_test(NAME gaia_cat_properties
  COMMAND gaia_cat_test properties ${GAIA_CAT_CLEAN})
add_test(NAME gaia_cat_worker
  COMMAND gaia_cat_test worker ${GAIA_CAT_CLEAN})
add_test(NAME gaia_cat_negative
  COMMAND gaia_cat_test negative ${GAIA_CAT_CLEAN} ${GAIA_CAT_DATA}/work)
add_test(NAME gaia_cat_truncate
  COMMAND gaia_cat_test truncate ${GAIA_CAT_TRUNC})
add_test(NAME gaia_cat_performance
  COMMAND gaia_cat_test performance ${GAIA_CAT_TRUNC}
          ${GAIA_CAT_DATA}/perf.json)
add_test(NAME gaia_cat_dump
  COMMAND gaia_cat_test dump ${GAIA_CAT_CLEAN} ${GAIA_CAT_DATA}/impl.json)
add_test(NAME gaia_cat_polar_ref_dump
  COMMAND gaia_cat_polar_ref dump ${GAIA_CAT_CLEAN}
          ${GAIA_CAT_DATA}/polar_ref.json)
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  add_test(NAME gaia_cat_oracle
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/../oracle/gaia_oracle.py
            ${GAIA_CAT_CLEAN} ${GAIA_CAT_DATA}/impl.json
            ${GAIA_CAT_DATA}/polar_ref.json)
  set_tests_properties(gaia_cat_oracle PROPERTIES
    DEPENDS "gaia_cat_dump;gaia_cat_polar_ref_dump")
else()
  message(WARNING "gaia_cat_oracle skipped: Python3 interpreter not found")
endif()

# ----------------------------------------------------------------------------
# CAT-GAIA-IMPL (SA-CATG-I): 模块适配器测试 — astrocs_catalog_gaia DLL
#   共址直接路径 (#include gaia_client.c, 无 alloc 钩子) vs DLL C ABI 适配器:
#   入口契约/导出面净化(legacy 符号不得 dlsym)/describe/validate 负例/
#   plan 真实 work_units/五 op direct-vs-plugin bitwise/executor 租借/
#   预算耗尽/host cancel 文件边界取消/inspect。DLL 经测试环境变量加载
#   (生成器表达式, 兼容含空格构建路径)。fixture 复用 gaia_cat_fixtures。
# ----------------------------------------------------------------------------
add_executable(gaia_adapter_test gaia_adapter_test.c)
target_include_directories(gaia_adapter_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/gaia_xpsd_client/src
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/gaia_xpsd_client/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${GAIA_CAT_DATA}/inc)
if(OpenMP_C_FOUND)
  target_link_libraries(gaia_adapter_test PRIVATE OpenMP::OpenMP_C)
endif()
target_link_libraries(gaia_adapter_test PRIVATE z m Threads::Threads)
add_dependencies(gaia_adapter_test gaia_cat_fixtures astrocs_catalog_gaia)
add_test(NAME gaia_cat_adapter
  COMMAND gaia_adapter_test ${GAIA_CAT_CLEAN})
set_tests_properties(gaia_cat_adapter PROPERTIES
  ENVIRONMENT "ASTROCS_GAIA_DLL_PATH=$<TARGET_FILE:astrocs_catalog_gaia>")

# CAT-GAIA-INT: gaia 模块本地集成测试 (integration descriptor + typed ports +
# 调用计数 + 真实 plan/trace; global DAG registration 归 SA-RT-05 不做)。
# descriptor/module.yaml 经编译定义注入 (只读, 路径含空格安全); trace JSONL
# 写到构建目录 run 侧文件, 重放检查在测试内完成。
set(GAIA_INT_TRACE_OUT ${CMAKE_CURRENT_BINARY_DIR}/gaia_integration_trace.jsonl)
add_executable(gaia_integration_test gaia_integration_test.c)
target_include_directories(gaia_integration_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/gaia_xpsd_client/src
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/gaia_xpsd_client/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${GAIA_CAT_DATA}/inc)
target_compile_definitions(gaia_integration_test PRIVATE
  ASTROCS_GAIA_INTEGRATION_JSON="${CMAKE_CURRENT_SOURCE_DIR}/../../lib/gaia_xpsd_client/integration/astrocs_catalog_gaia.integration.json"
  ASTROCS_GAIA_MODULE_YAML="${CMAKE_CURRENT_SOURCE_DIR}/../../lib/gaia_xpsd_client/module.yaml")
if(OpenMP_C_FOUND)
  target_link_libraries(gaia_integration_test PRIVATE OpenMP::OpenMP_C)
endif()
target_link_libraries(gaia_integration_test PRIVATE ${CMAKE_DL_LIBS} z m Threads::Threads)
add_dependencies(gaia_integration_test gaia_cat_fixtures astrocs_catalog_gaia)
add_test(NAME gaia_integration
  COMMAND gaia_integration_test ${GAIA_CAT_CLEAN})
set_tests_properties(gaia_integration PROPERTIES
  ENVIRONMENT "ASTROCS_GAIA_DLL_PATH=$<TARGET_FILE:astrocs_catalog_gaia>;ASTROCS_GAIA_INT_TRACE_OUT=${GAIA_INT_TRACE_OUT}")

# ----------------------------------------------------------------------------
# P1-DRZ-IMPL (SA-P1D-I): drizzle 模块适配器测试 — astrocs_p1_drizzle DLL
#   直接路径 (#include drizzle_adapter_impl.cpp → legacy hp_drizzle_run /
#   hp_drizzle_reverse_run 统计面, 链 astrocs_drizzle STATIC, 与 p1drz_tests
#   同手法) vs DLL C ABI 适配器: 入口契约/导出面净化(legacy 六符号不得
#   dlsym; nm -D 唯一导出另由构建检查实证)/describe/validate 负例/
#   plan 真实 work_units/drizzle+reverse direct-vs-plugin bitwise 统计与
#   平面 memcmp/executor 租借计数/入口取消。DLL 经测试环境变量加载
#   (生成器表达式, 兼容含空格构建路径)。
# ----------------------------------------------------------------------------
add_executable(drizzle_adapter_test drizzle_adapter_test.cpp)
target_include_directories(drizzle_adapter_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/healpix_db/healpix_drizzle
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/astro_image_io/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/drizzle/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_compile_definitions(drizzle_adapter_test PRIVATE
  AIO_ENABLE_FITS AIO_ENABLE_HEALPIX)
target_link_libraries(drizzle_adapter_test PRIVATE
  astrocs_drizzle ${CMAKE_DL_LIBS} ${ASTROCS_OMP_TARGET})
add_dependencies(drizzle_adapter_test astrocs_p1_drizzle)
add_test(NAME drizzle_adapter COMMAND drizzle_adapter_test)
set_tests_properties(drizzle_adapter PROPERTIES
  ENVIRONMENT "ASTROCS_DRIZZLE_DLL_PATH=$<TARGET_FILE:astrocs_p1_drizzle>")

# calibration_adapter_test (P1-CAL-IMPL): 模块 C ABI v1 adapter 测试。
# direct 通道链 astrocs_calibration STATIC (AC_API 全导出面); plugin 通道
# dlopen astrocs_p1_calibration DLL (唯一导出 astrocs_module_query_v1)。
# DLL 路径经 ENVIRONMENT 注入; include 与根 astrocs_calibration :338 一致。
add_executable(calibration_adapter_test calibration_adapter_test.cpp)
target_include_directories(calibration_adapter_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/calibration/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/astro_image_io/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(calibration_adapter_test PRIVATE
  astrocs_calibration ${CMAKE_DL_LIBS} ${ASTROCS_OMP_TARGET})
add_dependencies(calibration_adapter_test astrocs_p1_calibration)
add_test(NAME calibration_adapter COMMAND calibration_adapter_test)
set_tests_properties(calibration_adapter PROPERTIES
  ENVIRONMENT "ASTROCS_CAL_DLL_PATH=$<TARGET_FILE:astrocs_p1_calibration>")

# P1-CAL-INT: calibration 模块本地集成测试 (integration descriptor + typed
# ports + 调用计数 + 真实 plan/trace; global DAG registration 归 SA-RT-05
# 不做)。descriptor/module.yaml 经编译定义注入 (只读, 路径含空格安全);
# trace JSONL 写到构建目录, 重放检查在测试内完成。对齐 gaia_integration
# 先例; fixture 头 p1cal_fixtures.hpp (FIX-CAL 谱系)。
set(CAL_INT_TRACE_OUT ${CMAKE_CURRENT_BINARY_DIR}/calibration_integration_trace.jsonl)
add_executable(calibration_integration_test calibration_integration_test.cpp)
target_include_directories(calibration_integration_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/calibration/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/calibration/tests/p1cal
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_compile_definitions(calibration_integration_test PRIVATE
  ASTROCS_CAL_INTEGRATION_JSON="${CMAKE_CURRENT_SOURCE_DIR}/../../lib/calibration/integration/astrocs_p1_calibration.integration.json"
  ASTROCS_CAL_MODULE_YAML="${CMAKE_CURRENT_SOURCE_DIR}/../../lib/calibration/module.yaml")
target_link_libraries(calibration_integration_test PRIVATE
  ${CMAKE_DL_LIBS} ${ASTROCS_OMP_TARGET})
# libgomp 必须随可执行文件早装载: DLL (astrocs_p1_calibration) 自链 libgomp,
# 若测试 TU 无 OMP 符号引用, --as-needed 会把本 target 的 libgomp 从
# DT_NEEDED 丢弃, libgomp 退化为 dlopen 晚装载 → GOMP 并行区 SEGV (主 build
# 复现 10/10)。gaia_integration 先例不崩是因其 TU include gaia_client.c
# (含 omp 符号) 而天然保留 libgomp; 本测试为纯 host stub 无 OMP 引用, 用
# --no-as-needed 达成同机制 (仅 GNU ld 语义, MSVC 无此选项)。
if(UNIX AND NOT APPLE)
  target_link_options(calibration_integration_test PRIVATE
    "LINKER:--no-as-needed")
endif()
add_dependencies(calibration_integration_test astrocs_p1_calibration)
add_test(NAME calibration_integration COMMAND calibration_integration_test)
set_tests_properties(calibration_integration PROPERTIES
  ENVIRONMENT "ASTROCS_CAL_DLL_PATH=$<TARGET_FILE:astrocs_p1_calibration>;ASTROCS_CAL_INT_TRACE_OUT=${CAL_INT_TRACE_OUT}")

# cosmetic_adapter_test (P1-COS-IMPL): 模块 C ABI v1 adapter 测试。
# direct 通道链 astrocs_calibration STATIC (AC_API 全导出面); plugin 通道
# dlopen astrocs_p1_cosmetic DLL (唯一导出 astrocs_module_query_v1)。
# DLL 路径经 ENVIRONMENT 注入; include 与 DLL 生产源口径一致
# (lib/calibration/include 生产头 + lib/cosmetic/include types.h)。
# OMP 教训 (P1-CAL-INT 58d20223): DLL (astrocs_p1_cosmetic) 自链 libgomp,
# 本 TU 链 astrocs_calibration 已有 OMP 符号引用, 仍显式 --no-as-needed
# 双保险 (仅 GNU ld 语义, MSVC 无此选项), 防 dlopen 晚装载 SEGV。
add_executable(cosmetic_adapter_test cosmetic_adapter_test.cpp)
target_include_directories(cosmetic_adapter_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/calibration/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/cosmetic/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/astro_image_io/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(cosmetic_adapter_test PRIVATE
  astrocs_calibration ${CMAKE_DL_LIBS} ${ASTROCS_OMP_TARGET})
if(UNIX AND NOT APPLE)
  target_link_options(cosmetic_adapter_test PRIVATE
    "LINKER:--no-as-needed")
endif()
add_dependencies(cosmetic_adapter_test astrocs_p1_cosmetic)
add_test(NAME cosmetic_adapter COMMAND cosmetic_adapter_test)
set_tests_properties(cosmetic_adapter PROPERTIES
  ENVIRONMENT "ASTROSCOS_DLL_PATH=$<TARGET_FILE:astrocs_p1_cosmetic>")

# cosmetic_integration_test (P1-COS-INT): 模块本地集成测试 (MODULE_MIGRATION
# _TEMPLATE <prefix>-INT)。覆盖 integration descriptor 三方一致 (12 §5) +
# typed ports 绑定负面 (config 101/100/103/102/CONFIG_SCHEMA + manifest
# 111/112/110) + 两节点合成链调用计数每节点恰一 (validate/plan/create/
# execute 事务/inspect/destroy, lease 2/2 平衡) + 真实 plan (work_units=
# width*height 独立算术, io=0 真实值, 无观测词) + 真实 trace (dll_sha256
# FIPS 180-4 磁盘实测, 标准 K 表 + "abc" 已知向量自检锚; 先例 K 表缺 3 项
# 错 2 项已登记不复制) + 重放无 repeated-call + 负面 ABI 三态。global DAG
# registration 归 SA-RT-05 (descriptor global_dag_registration=delegated)。
# fixture 头 p1cos_fixtures.hpp (FIX-COS-B 谱系, splitmix64 固定 seed)。
set(COS_INT_TRACE_OUT ${CMAKE_CURRENT_BINARY_DIR}/cosmetic_integration_trace.jsonl)
add_executable(cosmetic_integration_test cosmetic_integration_test.cpp)
target_include_directories(cosmetic_integration_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/cosmetic/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/cosmetic/tests/p1cos
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_compile_definitions(cosmetic_integration_test PRIVATE
  ASTROSCOS_INTEGRATION_JSON="${CMAKE_CURRENT_SOURCE_DIR}/../../lib/cosmetic/integration/astrocs_p1_cosmetic.integration.json"
  ASTROSCOS_MODULE_YAML="${CMAKE_CURRENT_SOURCE_DIR}/../../lib/cosmetic/module.yaml")
target_link_libraries(cosmetic_integration_test PRIVATE
  ${CMAKE_DL_LIBS} ${ASTROCS_OMP_TARGET})
# OMP 教训 (P1-CAL-INT 58d20223): DLL (astrocs_p1_cosmetic) 自链 libgomp,
# 本 TU 为纯 host stub 无 OMP 符号引用, --as-needed 会把 libgomp 从
# DT_NEEDED 丢弃 → dlopen 晚装载 GOMP 并行区 SEGV (主 build 复现)。
# 链 ${ASTROCS_OMP_TARGET} + --no-as-needed 归一 (仅 GNU ld, MSVC 无此选项)。
if(UNIX AND NOT APPLE)
  target_link_options(cosmetic_integration_test PRIVATE
    "LINKER:--no-as-needed")
endif()
add_dependencies(cosmetic_integration_test astrocs_p1_cosmetic)
add_test(NAME cosmetic_integration COMMAND cosmetic_integration_test)
set_tests_properties(cosmetic_integration PROPERTIES
  ENVIRONMENT "ASTROSCOS_DLL_PATH=$<TARGET_FILE:astrocs_p1_cosmetic>;ASTROSCOS_INT_TRACE_OUT=${COS_INT_TRACE_OUT}")

# ----------------------------------------------------------------------------
# ----------------------------------------------------------------------------
# P1-PSF-TEST (TEST-P1-PSF-001, 控制包 queue 45, lock-P1-PSF) —
# lib/dynamic_psf 可复用验证面。结构与注册详见
# lib/dynamic_psf/tests/p1psf/CMakeLists.txt (FIX-PSF-A..E fixture /
# 独立 oracle 旋转主轴投影 / units·properties·oracle·negative·boundary·
# performance·selfcheck 七注册); 合同锚 TEST-PSF-DESIGN-001
# (STAR_PSF_ALGORITHMS.md §11, P1-PSF-DOC 冻结)。
# 注意: p1psf_selfcheck_test 与 p1psf_tests 共享 core TU; 被测面为生产
# dpsf_* C API 七导出 (dpsf_psf.cpp + dpsf_log.cpp 独立编入测试目标,
# 根构建无 dynamic_psf 生产 target, 生产源零改动 — 对齐 p1star 先例)。
# ----------------------------------------------------------------------------
add_subdirectory(../../lib/dynamic_psf/tests/p1psf p1psf)
# P1-STAR-TEST (TEST-P1-STAR-001, 控制包 queue 49, lock-P1-STAR) —
# lib/star_detector 可复用验证面。结构与注册详见
# lib/star_detector/tests/p1star/CMakeLists.txt (FIX-STAR-A..G fixture /
# 独立 oracle / negative + OOM 注入 / performance / 故障注入自检); 合同锚
# ALG-STARDET-001 §11.4 TEST-STAR-DESIGN-001 (P1-STAR-DOC 冻结)。
# 注意: p1star_selfcheck_test 与 p1star_tests 共享 core TUs; 被测面为生产
# sdet_* C API (5 src 独立编入测试目标, 根构建无 star_detector 生产 target,
# 生产源零改动); 与 p1_stars_test (P1-003 桥接层) 互补不重复。
# 域基线 dc2f5f3e (批次 R sdet malloc 事务化)。
# ----------------------------------------------------------------------------
add_subdirectory(../../lib/star_detector/tests/p1star p1star)


# ----------------------------------------------------------------------------
# P1-NOISE-IMPL (queue 43, lock-P1-NOISE) — astrocs_p1_noise adapter 对拍面
# (对齐 p1_hips/adapter_test.c 先例 P1-HIPS-IMPL 1959dc89 / CAL/COS
# adapter_test.cpp 同构): direct 通道 = module_entry.cpp 直通编译 +
# astrocs_p1_noise_prod STATIC (P1-NOISE-TEST 冻结被测面, 生产源零改动);
# plugin 通道 = dlopen astrocs_p1_noise DLL 经 astrocs_module_query_v1。
# direct-vs-plugin BITWISE 对拍 (f32/f64 + 内联 fill + 独立 fill 影子
# floor 回退同构 + scale_law/gain_variance 标量); strbuf 两阶段/plan
# work_units 事实/BUDGET 105 双路径/entry cancel/describe/负面 ABI/
# dlsym 探针 (legacy snr_noise_* 七符号全 NULL, 唯一 query 存在)。
# DLL 路径经 ENVIRONMENT 注入 (生成器表达式, 兼容含空格构建路径)。
# ----------------------------------------------------------------------------
add_executable(p1_noise_adapter_test p1_noise/adapter_test.cpp
  p1_noise/adapter_entry_impl.cpp)
target_include_directories(p1_noise_adapter_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/snr_estimator/cpp/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/snr_estimator/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/snr_estimator/src)
target_link_libraries(p1_noise_adapter_test PRIVATE
  p1noise_under_test ${CMAKE_DL_LIBS})
add_dependencies(p1_noise_adapter_test astrocs_p1_noise)
add_test(NAME p1_noise_adapter COMMAND p1_noise_adapter_test
  ${CMAKE_CURRENT_BINARY_DIR}/run/t_noise_adapter)
set_tests_properties(p1_noise_adapter PROPERTIES
  ENVIRONMENT "ASTROCS_NOISE_DLL_PATH=$<TARGET_FILE:astrocs_p1_noise>")

# --- P1-WCS-TEST (TEST-P1-WCS-001): WCS/plate_solve 可复用验证面 -----------
# 六组 CTest: p1wcs_units / p1wcs_properties / p1wcs_oracle /
# p1wcs_negative / p1wcs_performance / p1wcs_selfcheck (label: p1wcs)。
# 被测面: ipv 数学内核 TU 直编 (生产源零改动) + lib/phase1/wcs WcsTan。
add_subdirectory(p1wcs p1wcs)
# --- P1-WCS-TEST end --------------------------------------------------------


# ----------------------------------------------------------------------------
# AIO-001 (ASTROCS-CONSTITUTION-ALIGNMENT-V1) — 唯一 AIO C ABI v1 与内容哈希复核
# 合同锚: include/astrocs/io/aio_abi_v1.h + contracts/data/aio_abi_contract_v1.json。
# 被测面: lib/astro_image_io/src/aio_abi.cpp (唯一公共 ABI 实现; astrocs_aio 域,
# 宪章 §8.5 交付单元); oracle: FIPS 180-4 标准向量 (hashlib 预生成) +
# lib/common crypto::Sha256 交叉对拍 + acs_fio_status 0..13 编译期对齐证明。
# 组: aio_abi_units / aio_abi_negative / aio_abi_selfcheck (故障注入必败:
# ASTROCS_AIO_FAULT=n1_hash_value_flip|n2_verify_mismatch_shortcut|n3_file_size_skip)。
# OpenMP 并行轴: 1/4/16 worker 复核位级一致 (reentrant; 非重计算, 显式 worker 传入)。
# ----------------------------------------------------------------------------
add_executable(aio_abi_tests
  aio_abi_tests.cpp
  ../../lib/astro_image_io/src/aio_abi.cpp)
target_include_directories(aio_abi_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/astro_image_io/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/common
  ${CMAKE_CURRENT_SOURCE_DIR}/../../modules/services/io/include)
target_link_libraries(aio_abi_tests PRIVATE astrocs_common ${ASTROCS_OMP_TARGET})
add_test(NAME aio_abi_units COMMAND aio_abi_tests units)
add_test(NAME aio_abi_negative COMMAND aio_abi_tests negative)

add_executable(aio_abi_selfcheck
  aio_abi_selfcheck.cpp
  aio_abi_tests.cpp
  ../../lib/astro_image_io/src/aio_abi.cpp)
target_compile_definitions(aio_abi_selfcheck PRIVATE AIO_ABI_TEST_NO_MAIN)
target_include_directories(aio_abi_selfcheck PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/astro_image_io/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/common
  ${CMAKE_CURRENT_SOURCE_DIR}/../../modules/services/io/include)
target_link_libraries(aio_abi_selfcheck PRIVATE astrocs_common)
add_test(NAME aio_abi_selfcheck COMMAND aio_abi_selfcheck)
