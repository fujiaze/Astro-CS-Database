# ============================================================================
# 编排层共址测试 —— ORCH-001 批次 1（补测试）+ 批次 3（可执行级接线门）
# ----------------------------------------------------------------------------
# 改前事实：ctest -N | grep -ci orchestrator = 0（编排层零测试注册）。
# 本文件注册 7 条 ctest：3 条既有共址单测（Makefile 时代从不进 ctest）、
# 2 条 legacy CLI 入口冒烟、1 条可执行级饱和接线门（批次 3）、
# 1 条曲线解析能红能绿门（PHOTOCURVE-ORCH-01）。
# 测试工作目录固定在 <build>/orch_gate_work/ 下：既有 test_orchestrator_cli.cpp
# 按 Makefile 布局从 ../../.. 推导项目根（= 仓库根），本布局保持该相对深度不变。
# ============================================================================

set(ORCH_TEST_WORK ${CMAKE_BINARY_DIR}/orch_gate_work)
file(MAKE_DIRECTORY ${ORCH_TEST_WORK})
file(MAKE_DIRECTORY ${ORCH_TEST_WORK}/cli_integration)

# ── 1) 既有共址单测（三件套：Logger / Checkpoint / CLI 集成）──────────────
foreach(orch_test logger checkpoint)
  add_executable(orchestrator_test_${orch_test} test_${orch_test}.cpp)
  target_link_libraries(orchestrator_test_${orch_test} PRIVATE astrocs_infra_orchestrator)
  add_test(NAME orchestrator_${orch_test}_units COMMAND orchestrator_test_${orch_test})
  set_tests_properties(orchestrator_${orch_test}_units PROPERTIES
    WORKING_DIRECTORY ${ORCH_TEST_WORK})
endforeach()

# CLI 集成测试（1944 行，Part 1..7）：按文件布局查找 cwd 下的 orchestrator.exe，
# 故将新构建的 legacy 入口以该名字投放到测试工作目录（POST_BUILD 拷贝，跨平台）。
add_executable(orchestrator_test_cli test_orchestrator_cli.cpp)
target_link_libraries(orchestrator_test_cli PRIVATE astrocs_infra_orchestrator)
# add_custom_command(TARGET ...) 只能在其 target 所在目录使用（CMake 限制），
# 而 orchestrator_legacy_cli 声明于上一级 ⇒ 这里用 custom target 投放。
add_custom_target(orchestrator_cli_test_drop ALL
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:orchestrator_legacy_cli>
          ${ORCH_TEST_WORK}/cli_integration/orchestrator.exe
  DEPENDS orchestrator_legacy_cli
  COMMENT "ORCH-001: 投放 legacy CLI 入口到共址测试工作目录 (orchestrator.exe)")
add_dependencies(orchestrator_test_cli orchestrator_cli_test_drop)
add_test(NAME orchestrator_cli_integration COMMAND orchestrator_test_cli)
set_tests_properties(orchestrator_cli_integration PROPERTIES
  WORKING_DIRECTORY ${ORCH_TEST_WORK}/cli_integration)

# ── 2) legacy CLI 入口冒烟（真跑编译产物，不是源级断言）──────────────────
add_test(NAME orchestrator_legacy_cli_smoke COMMAND orchestrator_legacy_cli --version)
set_tests_properties(orchestrator_legacy_cli_smoke PROPERTIES
  WORKING_DIRECTORY ${ORCH_TEST_WORK}
  PASS_REGULAR_EXPRESSION "AstroCS Orchestrator")
add_test(NAME orchestrator_legacy_cli_validate
  COMMAND orchestrator_legacy_cli --validate
          ${CMAKE_CURRENT_SOURCE_DIR}/../configs/stage1.template.json)
set_tests_properties(orchestrator_legacy_cli_validate PROPERTIES
  WORKING_DIRECTORY ${ORCH_TEST_WORK}
  PASS_REGULAR_EXPRESSION "VALID")

# ── 3) ORCH-001 批次 3：可执行级「科学配置解析 → 运行时」接线门 ──────────
# 最小链路 = 真实 run_stage_snr + 真实 DllLoader + 真实 PipelineFrame(AIO) +
# 生产噪声模块源(noise_model.cpp, 零改动) 作为 snr_estimator 模块；
# 断言 header SATURATE/DATAMAX 的解析结果真的进入 SnrNoiseModelConfig 并改变
# 模块运行结果。SAT-001 的 W3 是源级正则断言，本门是可执行级升级。
# AIO 模块夹具 = 生产源 aio_pipeline.cpp + aio_log.cpp（帧容器本体，零改动）。
add_library(orchestrator_gate_module_aio SHARED
  ${CMAKE_SOURCE_DIR}/lib/infrastructure/aio/src/aio_pipeline.cpp
  ${CMAKE_SOURCE_DIR}/lib/infrastructure/aio/src/aio_log.cpp)
target_include_directories(orchestrator_gate_module_aio PRIVATE
  ${CMAKE_SOURCE_DIR}/lib/infrastructure/aio/include
  ${CMAKE_SOURCE_DIR}/lib/infrastructure/aio/src)
set_target_properties(orchestrator_gate_module_aio PROPERTIES
  OUTPUT_NAME "astro_image_io" PREFIX "")
if(NOT MSVC)
  target_compile_options(orchestrator_gate_module_aio PRIVATE -w)
endif()

# SNR 模块夹具 = 生产噪声模型源(noise_model.cpp + snr_science.cpp, 零改动)
# + 测试替身 gate_module_snr_extract_spy.cpp（只替身 snr_extract_model_v3 /
# snr_free_model_v3 —— 控制点提取不在本门判据内；噪声模型本体必须是生产源）。
add_library(orchestrator_gate_module_snr SHARED
  ${CMAKE_SOURCE_DIR}/lib/algorithms/noise_snr/cpp/src/noise_model.cpp
  ${CMAKE_SOURCE_DIR}/lib/algorithms/noise_snr/cpp/src/snr_science.cpp
  gate/gate_module_snr_extract_spy.cpp)
target_include_directories(orchestrator_gate_module_snr PRIVATE
  ${CMAKE_SOURCE_DIR}/lib/algorithms/noise_snr/cpp/include
  ${CMAKE_SOURCE_DIR}/lib/algorithms/noise_snr/include
  ${CMAKE_SOURCE_DIR}/lib/algorithms/shared/include)
set_target_properties(orchestrator_gate_module_snr PROPERTIES
  OUTPUT_NAME "snr_estimator" PREFIX "")

add_executable(orchestrator_saturation_wiring_gate
  gate/orchestrator_saturation_wiring_gate.cpp)
target_link_libraries(orchestrator_saturation_wiring_gate
  PRIVATE astrocs_infra_orchestrator)
add_dependencies(orchestrator_saturation_wiring_gate
  orchestrator_gate_module_aio orchestrator_gate_module_snr)
add_test(NAME orchestrator_saturation_wiring_gate
  COMMAND orchestrator_saturation_wiring_gate
          $<TARGET_FILE:orchestrator_gate_module_aio>
          $<TARGET_FILE:orchestrator_gate_module_snr>
          ${ORCH_TEST_WORK}/saturation_gate)
set_tests_properties(orchestrator_saturation_wiring_gate PROPERTIES
  WORKING_DIRECTORY ${ORCH_TEST_WORK})

# ── 4) PHOTOCURVE-ORCH-01：滤镜/QE 曲线解析「能红能绿」门 ────────────────────
# 被测面 = 生产**唯一实现** lib/algorithms/photometry/cpp/src/filter_curve_json.h
# （orchestrator 的 run_stage_photometric 与 frame_photometry_fit 都只做委托）。
# 门内三类判据: 绿（两版 filters.json 都取到声明的 Baader R）、红（修复前的文本
# 搜索口径在同一文件上取到 Antlia V Pro Series B）、负例（不存在名/非库键反例/
# provenance 有名字而 filters 段没有 ⇒ 显式报错）。依据 ENGINEERING_SPEC.md:175
# 「每项检查有正例与负例（能红能绿）」+ docs/standards/CODE_STANDARD.md §MUST
# 「禁止重复 production science implementation（单一实现 + oracle）」。
# 注: 本 TU 无文件系统原语（读文件经 aio_file::read_all）⇒ 不触发
# AIO-IO-BOUNDARY 台账 N2「未登记文件出现命中」。
add_executable(orchestrator_curve_resolve_gate test_photometry_curve_resolve.cpp)
target_include_directories(orchestrator_curve_resolve_gate PRIVATE
  ${CMAKE_SOURCE_DIR}/lib/algorithms/photometry/cpp/src
  ${CMAKE_SOURCE_DIR}/lib/infrastructure/aio/src
  ${CMAKE_SOURCE_DIR}/lib/algorithms/shared
  ${CMAKE_SOURCE_DIR}/lib/algorithms/shared/crypto)
target_link_libraries(orchestrator_curve_resolve_gate PRIVATE astrocs_common)
add_test(NAME orchestrator_curve_resolve_gate
  COMMAND orchestrator_curve_resolve_gate ${CMAKE_SOURCE_DIR})
set_tests_properties(orchestrator_curve_resolve_gate PROPERTIES
  WORKING_DIRECTORY ${ORCH_TEST_WORK})
# 故障注入负例入口（ENGINEERING_SPEC.md:176）: 把 [G1] 的装载口径换成修复前的
# 文本搜索, 期望判红并输出 FAULT_INJECT(legacy) DETECTED（rc=0 = 注入被检出）。
add_test(NAME orchestrator_curve_resolve_gate_fault_inject
  COMMAND orchestrator_curve_resolve_gate ${CMAKE_SOURCE_DIR} --fault-inject=legacy)
set_tests_properties(orchestrator_curve_resolve_gate_fault_inject PROPERTIES
  WORKING_DIRECTORY ${ORCH_TEST_WORK}
  PASS_REGULAR_EXPRESSION "FAULT_INJECT\\(legacy\\) DETECTED")
