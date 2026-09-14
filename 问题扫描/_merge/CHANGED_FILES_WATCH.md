# R 层工作清单：改动面 × 引用它的问题条目（机械交叉索引）

- 基线 SHA：**b32246c4**；当前 main：**f4f88d54b51f2a52f290fed7643f3b5b9d58f3d2**；生成于复验前，**R 层开工前必须重跑刷新**。
- 基线后已进入 main 的改动（真源白名单内）：**55**；当前工作树脏：**12**；改动面合计 **65**，被审计档案点名 **62**，未点名 **3**。
- 统计口径：只匹配**全路径字符串**（含 path::符号 形式），只统计真源前缀（lib/docs/cli/runtime/providers/modules/include/tests/ci/tools/contracts/packaging/cmake/CMakeLists/VERSION/根 md/.github），排除本审计目录与免报区。

## 触发条件（满足后才实跑 R 层，中途只刷新本文件）
1. `git rev-parse HEAD` 连续两轮采样不变；且 2. 真源白名单内工作树脏文件数为 0；且 3. 全部 M 代理已交付。

## 复验规则（负责人指令，R 代理逐条执行）
1. 逐文件比对档案给的锚与当前树实际位置：**一律改为 path::符号**，行号仅作「复验时 N」附注；行变了改行。
2. 逐条目重判 bug 现状四态：仍成立 / 已被修复 / 部分修复（只报残余）/ 无法判定。
3. 已修复者从 findings/ 移出并记入该域 _merge「已修复」表；**修复但无回归测试**另立 F_TEST_GAP。
4. 凡条目引用代码当前行为（注释摘录、常量、阈值、默认值、返回值、分支）必须逐字重读重抄，禁止只改行号不改判断。
5. 表 B 是**潜在漏报面**：被改动但审计未点名，须抽查是否引入新问题（尤其 lib/ 与 docs/standards、ci/checks.json）。

## 表 A：必须复验的改动文件（按被引用次数降序，全列）

| 改动文件 | 来源 | 被引 | 涉及条目 |
|---|---|---|---|
| CMakeLists.txt | CW | 408 | L01-011, L03-009, L03-008, L05-008, L07-025, L07-015, L08-006, L10-017 |
| ci/checks.json | CW | 301 | L06-015, L06-016, L11-016, L12-002, L16-010, L17-001, L23-006, L23-009 |
| docs/contracts/DATA_SEMANTICS.md | CW | 149 | L03-014, L18-017, L09-002, L03-017, L07-005, F00-04, F00-06, L02-002 |
| lib/core/src/module_adapters.cpp | C+W | 135 | L05-004, L05-011, L11-011, L12-023, F00-04, L01-003, L01-010, L01-013 |
| tests/unit/CMakeLists.txt | CW | 94 | L07-015, L10-017, F00-04, L01-013, L01-020, L02-005, L03-006, L03-008 |
| lib/astro_image_io/src/hips/aio_hips_writer.cpp | CW | 64 | M2b-L15 |
| cli/commands.cpp | C+W | 62 | L05-004, L05-011, L12-023, L17-004, F00-03, L05-019, L09-008, L11-001 |
| docs/algorithms/DRIZZLE_GEOMETRY.md | CW | 48 | L20-005, L20-006, L04-003, L04-004, L04-005, L04-006, L04-009, L04-012 |
| docs/science/ASTROMETRY.md | CW | 45 | L13-001 |
| docs/algorithms/HIPS_WRITER.md | CW | 44 | L03-002, L03-003, L03-011, L03-012, L03-013, L03-014, L03-018, L03-019 |
| docs/science/CALIBRATION.md | CW | 42 | L18-017, F00-07, F00-08 |
| docs/algorithms/CALIBRATION_ALGORITHMS.md | CW | 41 | L12-001, L12-002, F00-07, F00-08 |
| docs/algorithms/PLATESOLVE.md | CW | 38 |  |
| cli/CMakeLists.txt | CW | 37 | L05-008, L25-010, L12-001 |
| lib/phase2/CMakeLists.txt | CW | 34 | L08-006, F00-04, L01-013, L01-020, L02-005, L03-006, L03-008, L03-009 |
| docs/api/CLI_PROTOCOL_V1.md | CW | 27 | L12-001, L12-011, L12-012, L12-013, L12-016, L12-021, L18-012, L18-013 |
| cli/resource_gate.h | CW | 24 | L17-004, L11-001, L11-002, L11-003, L11-019, L11-021 |
| lib/drizzle/README.md | CW | 24 | L03-008, L04-008, L13-012 |
| lib/calibration/src/master_generator.cpp | CW | 23 | L05-004 |
| lib/phase1/tests/ | W | 21 | L07-025, L23-005, L04-004, F00-01, F00-02, F00-03, F00-04, F00-05 |
| ci/steps/linux_build_root_graph.sh | CW | 19 | L12-001, L12-002 |
| lib/hips/CMakeLists.txt | CW | 17 | L03-009, F00-04, L01-013, L01-020, L02-005, L03-006, L03-008, L03-018 |
| lib/healpix_db/healpix_drizzle/hp_drizzle_api.cpp | CW | 16 |  |
| cli/parser.cpp | CW | 14 | L11-005, L11-006, L11-008, L12-011, L12-012 |
| docs/architecture/PRODUCTION_EXECUTION_INVENTORY.csv | CW | 14 |  |
| ci/validate_registry.py | CW | 13 | L23-001, L23-009 |
| lib/plate_solve/cpp/ipv/include/ipv_types.h | CW | 12 |  |
| lib/plate_solve/cpp/ipv/src/ipv_solver.cpp | CW | 11 |  |
| .github/workflows/ci-linux.yml | CW | 10 | L17-006, L22-010, L25-002 |
| tests/cli/test_cli_protocol.py | CW | 10 | L12-024, L12-012, L12-001 |
| tests/unit/master_flat_median_test.cpp | CW | 10 | L05-001, F00-01 |
| lib/hips/memory.md | CW | 8 | L01-006, L01-008, L01-011, L01-014, L01-020, L02-017, L04-008, L04-018 |
| lib/snr_estimator/include/ | W | 8 | F00-01, F00-02, F00-03, F00-04, F00-05, F00-06, L01-001, L01-002 |
| docs/contracts/DATA_ARTIFACTS.md | CW | 7 |  |
| lib/phase1_session/README.md | CW | 7 | F00-06, L01-004, L01-013, L01-017, L01-020, L02-003, L02-004, L02-012 |
| lib/phase2/memory.md | CW | 7 | L01-006, L01-008, L01-011, L01-014, L01-020, L02-017, L04-008, L04-018 |
| lib/plate_solve/cpp/ipv/src/ipv_select.cpp | CW | 7 |  |
| cli/monitor.h | W | 7 | L09-016 |
| tests/unit/p1001_real_nodes_test.cpp | CW | 6 | L04-001, L05-013 |
| lib/snr_estimator/src/ | W | 6 | F00-01, F00-02, F00-03, F00-04, F00-05, F00-06, L01-001, L01-002 |
| include/astrocs/core/context.h | CW | 5 |  |
| tests/api/test_seam_metric_gate.py | CW | 5 | L09-011, L09-016 |
| lib/core/src/context.cpp | CW | 4 |  |
| lib/healpix_db/healpix_drizzle/hp_drizzle_api.h | CW | 4 |  |
| tests/unit/mon001_gate_test.cpp | CW | 4 |  |
| .github/workflows/ci-windows.yml | CW | 3 |  |
| ci/workflow_binding.json | CW | 3 |  |
| lib/calibration/tests/p1cal/p1cal_tests_core.cpp | CW | 3 |  |
| lib/plate_solve/cpp/ipv/src/ipv_entry.cpp | CW | 3 |  |
| tests/unit/p1wcs/CMakeLists.txt | CW | 3 | L01-011, F00-04, L01-013, L01-020, L02-005, L03-006, L03-008, L03-009 |
| tests/unit/p1_noise/ | W | 3 | L07-013, F00-01, F00-02, F00-03, F00-04, F00-05, F00-06, L01-001 |
| lib/astro_image_io/memory.md | CW | 2 | L01-006, L01-008, L01-011, L01-014, L01-020, L02-017, L04-008, L04-018 |
| lib/gaia_xpsd_client/tests/test_gaia_zlib_configure_contract.py | CW | 2 |  |
| lib/snr_estimator/CMakeLists.txt | W | 2 | F00-04, L01-013, L01-020, L02-005, L03-006, L03-008, L03-009, L03-018 |
| lib/astro_image_io/Makefile | CW | 1 | L03-006, L04-019, L05-013, L05-019, L07-013, L07-025, L13-019, L13-020 |
| lib/core/src/runtime.cpp | CW | 1 |  |
| lib/phase1_session/memory.md | CW | 1 | L01-006, L01-008, L01-011, L01-014, L01-020, L02-017, L04-008, L04-018 |
| lib/phase2/tests/ivar_wiring_test.cpp | CW | 1 |  |
| tests/cli/test_cli_build.py | CW | 1 |  |
| tests/unit/drizzle_precision_default_test.cpp | CW | 1 |  |
| tests/unit/ipv_platform_binding_test.cpp | CW | 1 |  |
| lib/photometric_calib/cpp/test/filter_qe_provenance.json | W | 1 |  |

## 表 B：被改动但审计档案未点名（潜在漏报面）

- tests/backend/phase2_fixture_main.cpp（工作树脏）
- tests/backend/test_p3003_parallel_resampler.py（工作树脏）
- tests/backend/test_p3006_production_pipeline.py（工作树脏）

## 表 C：按复验主体（档案）分布的工作量

| 审计档案 | 引用改动面次数 |
|---|---|
| 问题扫描/_cache/L12.md | 72 |
| 问题扫描/_merge/CHANGED_FILES_WATCH.md | 68 |
| 问题扫描/_cache/L11.md | 63 |
| 问题扫描/_cache/L03.md | 59 |
| 问题扫描/_cache/L05.md | 52 |
| 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md | 51 |
| 问题扫描/_cache/L04.md | 50 |
| 问题扫描/_cache/L01.md | 45 |
| 问题扫描/_cache/L15.md | 42 |
| 问题扫描/_cache/L23.md | 41 |
| 问题扫描/_cache/L10.md | 38 |
| 问题扫描/_cache/L16.md | 34 |
| 问题扫描/findings/G_GOV_GATE/p0/M5b_L12_L17.md | 32 |
| 问题扫描/_cache/L07.md | 31 |
| 问题扫描/_cache/L17.md | 30 |
| 问题扫描/_cache/L13.md | 30 |
| 问题扫描/findings/G_GOV_GATE/p1/M5a_L11.md | 29 |
| 问题扫描/_merge/M8.md | 28 |
| 问题扫描/_merge/00_COORDINATION.md | 26 |
| 问题扫描/findings/G_GOV_GATE/p1/M5b_L12_L17.md | 26 |
| 问题扫描/findings/C_DOC_CODE_GAP/p1/M2a_L04_L10.md | 24 |
| 问题扫描/_merge/M5a.md | 22 |
| 问题扫描/findings/E_TRACE_BREAK/p1/M2a_L04_L10.md | 22 |
| 问题扫描/_merge/M3.md | 21 |
| 问题扫描/_merge/M5b.md | 20 |
| 问题扫描/_merge/M9.md | 20 |
| 问题扫描/findings/F_TEST_GAP/p0/M9_L24_L26.md | 20 |
| 问题扫描/_cache/L18.md | 19 |
| 问题扫描/_cache/L27.md | 19 |
| 问题扫描/findings/G_GOV_GATE/p1/M8a_L25_L27.md | 19 |
| 问题扫描/_cache/L25.md | 18 |
| 问题扫描/_cache/L02.md | 18 |
| 问题扫描/_cache/L06.md | 18 |
| 问题扫描/_merge/M8a.md | 18 |
| 问题扫描/_cache/L21.md | 18 |
| 问题扫描/_cache/L22.md | 16 |
| 问题扫描/findings/C_DOC_CODE_GAP/p1/M8a_L25_L27.md | 16 |
| 问题扫描/_cache/L08.md | 15 |
| 问题扫描/_merge/M6a.md | 15 |
| 问题扫描/findings/C_DOC_CODE_GAP/p1/M5a_L11.md | 15 |
| 问题扫描/_cache/L20.md | 15 |
| 问题扫描/findings/G_GOV_GATE/p1/M6b_L16_L18.md | 14 |
| 问题扫描/findings/I_DOC_HYGIENE/p1/M3_L05_L07.md | 14 |
| 问题扫描/_cache/L26.md | 14 |
| 问题扫描/findings/C_DOC_CODE_GAP/p1/M3_L05_L07.md | 13 |
| 问题扫描/findings/F_TEST_GAP/p1/M8_L22_L23.md | 13 |
| 问题扫描/_cache/L14.md | 12 |
| 问题扫描/findings/G_GOV_GATE/p0/M8a_L25_L27.md | 11 |
| 问题扫描/findings/C_DOC_CODE_GAP/p1/M6a_L13_L14.md | 11 |
| 问题扫描/findings/F_TEST_GAP/p0/M8_L22_L23.md | 11 |
| 问题扫描/findings/G_GOV_GATE/p0/M5a_L11_L17.md | 11 |
| 问题扫描/_merge/M2b.md | 10 |
| 问题扫描/findings/C_DOC_CODE_GAP/p1/M2b_L03_L15.md | 10 |
| 问题扫描/findings/E_TRACE_BREAK/p1/M6b_L16_L18.md | 10 |
| 问题扫描/findings/F_TEST_GAP/p0/M2a_L04_L10.md | 10 |
| 问题扫描/findings/F_TEST_GAP/p1/M2a_L04_L10.md | 10 |
| 问题扫描/findings/C_DOC_CODE_GAP/p1/M5b_L12_L17.md | 10 |
| 问题扫描/findings/F_TEST_GAP/p1/M3_L05_L07.md | 9 |
| 问题扫描/findings/I_DOC_HYGIENE/p2/M3_L05_L07.md | 9 |
| 问题扫描/_cache/F00_FRONT_SPOTCHECKS.md | 8 |
| 问题扫描/_cache/L24.md | 8 |
| 问题扫描/_merge/M2a.md | 8 |
| 问题扫描/_merge/M6b.md | 8 |
| 问题扫描/findings/E_TRACE_BREAK/p1/M5a_L11.md | 8 |
| 问题扫描/findings/F_TEST_GAP/p0/M4_L08_L09.md | 8 |
| 问题扫描/findings/F_TEST_GAP/p2/M6a_L13_L14.md | 8 |
| 问题扫描/findings/G_GOV_GATE/p0/M6b_L16_L18.md | 8 |
| 问题扫描/findings/I_DOC_HYGIENE/p2/M6a_L13_L14.md | 8 |
| 问题扫描/findings/I_DOC_HYGIENE/p2/M8a_L25_L27.md | 8 |
| 问题扫描/_merge/M7.md | 8 |
| 问题扫描/_cache/L09.md | 8 |
| 问题扫描/findings/A_SCI_DEF/p1/M1a_L01_L02.md | 8 |
| 问题扫描/findings/D_COMMENT/p1/M6a_L13_L14.md | 7 |
| 问题扫描/findings/D_COMMENT/p2/M2a_L04_L10.md | 7 |
| 问题扫描/findings/E_TRACE_BREAK/p0/M3_L05_L07.md | 7 |
| 问题扫描/findings/E_TRACE_BREAK/p1/M1a_L01_L02.md | 7 |
| 问题扫描/findings/E_TRACE_BREAK/p1/M3_L05_L07.md | 7 |
| 问题扫描/findings/I_DOC_HYGIENE/p1/M6a_L13_L14.md | 7 |
| 问题扫描/findings/A_SCI_DEF/p0/M2b_L03_L15.md | 7 |
| 问题扫描/findings/C_DOC_CODE_GAP/p0/M1a_L01_L02.md | 7 |
| 问题扫描/findings/A_SCI_DEF/p0/M3b_L06.md | 6 |
| 问题扫描/findings/C_DOC_CODE_GAP/p2/M8a_L25_L27.md | 6 |
| 问题扫描/findings/D_COMMENT/p2/M6a_L13_L14.md | 6 |
| 问题扫描/findings/E_TRACE_BREAK/p2/M8_L22_L23.md | 6 |
| 问题扫描/findings/F_TEST_GAP/p0/M2b_L03_L15.md | 6 |
| 问题扫描/findings/F_TEST_GAP/p0/M3b_L06.md | 6 |
| 问题扫描/findings/G_GOV_GATE/p1/M2b_L03_L15.md | 6 |
| 问题扫描/findings/G_GOV_GATE/p1/M2a_L04_L10.md | 6 |
| 问题扫描/findings/I_DOC_HYGIENE/p2/M5b_L12_L17.md | 6 |
| 问题扫描/findings/A_SCI_DEF/p1/M3_L05_L07.md | 6 |
| 问题扫描/findings/A_SCI_DEF/p2/M3_L05_L07.md | 6 |
| 问题扫描/findings/B_STD_MISMATCH/p0/M2b_L03_L15.md | 6 |
| 问题扫描/findings/C_DOC_CODE_GAP/p1/M1a_L01_L02.md | 6 |
| 问题扫描/20_AGENT_PLAN.md | 5 |
| 问题扫描/40_OWNER_DECISIONS.md | 5 |
| 问题扫描/_merge/M1a.md | 5 |
| 问题扫描/findings/F_TEST_GAP/p1/M2b_L03_L15.md | 5 |
| 问题扫描/findings/F_TEST_GAP/p1/M4_L08_L09.md | 5 |
| 问题扫描/findings/F_TEST_GAP/p1/M1a_L01_L02.md | 5 |
| 问题扫描/findings/A_SCI_DEF/p1/M7_A_SCI_DEF_p1_b.md | 5 |
| 问题扫描/findings/H_NUMERIC/p0/M2a_L04_L10.md | 5 |
| 问题扫描/findings/C_DOC_CODE_GAP/p0/M2b_L03_L15.md | 5 |
| 问题扫描/findings/E_TRACE_BREAK/p1/M5b_L12_L17.md | 5 |
| 问题扫描/findings/I_DOC_HYGIENE/p2/M7_I_DOC_HYGIENE_p2.md | 5 |
| 问题扫描/_cache/L19.md | 5 |
| 问题扫描/_merge/M3b.md | 4 |
| 问题扫描/findings/C_DOC_CODE_GAP/p0/M3b_L06.md | 4 |
| 问题扫描/findings/C_DOC_CODE_GAP/p0/M7_C_DOC_CODE_GAP_p0.md | 4 |
| 问题扫描/findings/E_TRACE_BREAK/p2/M8a_L25_L27.md | 4 |
| 问题扫描/findings/B_STD_MISMATCH/p1/M2b_L03_L15.md | 4 |
| 问题扫描/findings/F_TEST_GAP/p2/M8a_L25_L27.md | 4 |
| 问题扫描/findings/G_GOV_GATE/p1/M8_L22_L23.md | 4 |
| 问题扫描/findings/C_DOC_CODE_GAP/p0/M4_L08_L09.md | 4 |
| 问题扫描/findings/B_STD_MISMATCH/p1/M9_L24_L26.md | 4 |
| 问题扫描/findings/A_SCI_DEF/p0/M1a_L01_L02.md | 4 |
| 问题扫描/findings/A_SCI_DEF/p1/M2a_L04_L10.md | 4 |
| 问题扫描/findings/C_DOC_CODE_GAP/p1/M6b_L16_L18.md | 4 |
| 问题扫描/findings/H_NUMERIC/p2/M2a_L04_L10.md | 4 |
| 问题扫描/findings/D_COMMENT/p2/M3_L05_L07.md | 4 |
| 问题扫描/SUMMARY.md | 3 |
| 问题扫描/findings/C_DOC_CODE_GAP/p0/M5b_L12_L17.md | 3 |
| 问题扫描/findings/D_COMMENT/p1/L28b.md | 3 |
| 问题扫描/findings/E_TRACE_BREAK/p2/M6b_L16_L18.md | 3 |
| 问题扫描/findings/I_DOC_HYGIENE/p2/M8_L22_L23.md | 3 |
| 问题扫描/findings/E_TRACE_BREAK/p0/M6b_L16_L18.md | 3 |
| 问题扫描/findings/F_TEST_GAP/p1/M5b_L12_L17.md | 3 |
| 问题扫描/findings/I_DOC_HYGIENE/p1/M7_I_DOC_HYGIENE_p1.md | 3 |
| 问题扫描/findings/A_SCI_DEF/p1/M7_A_SCI_DEF_p1_a.md | 3 |
| 问题扫描/findings/A_SCI_DEF/p1/M9_L24_L26.md | 3 |
| 问题扫描/findings/A_SCI_DEF/p2/M7_A_SCI_DEF_p2.md | 3 |
| 问题扫描/findings/G_GOV_GATE/p1/M7_G_GOV_GATE_p1.md | 3 |
| 问题扫描/findings/C_DOC_CODE_GAP/p2/M2b_L03_L15.md | 3 |
| 问题扫描/findings/E_TRACE_BREAK/p1/M9_L24_L26.md | 3 |
| 问题扫描/findings/B_STD_MISMATCH/p0/M1a_L01_L02.md | 3 |
| 问题扫描/findings/B_STD_MISMATCH/p0/M2a_L04_L10.md | 3 |
| 问题扫描/findings/B_STD_MISMATCH/p0/M9_L24_L26.md | 3 |
| 问题扫描/_cache/L28b.md | 2 |
| 问题扫描/findings/C_DOC_CODE_GAP/p1/M8_L22_L23.md | 2 |
| 问题扫描/findings/E_TRACE_BREAK/p1/M4_L08_L09.md | 2 |
| 问题扫描/findings/F_TEST_GAP/p1/M5a_L11.md | 2 |
| 问题扫描/findings/G_GOV_GATE/p0/M6a_L13_L14.md | 2 |
| 问题扫描/findings/I_DOC_HYGIENE/p2/M6b_L16_L18.md | 2 |
| 问题扫描/findings/C_DOC_CODE_GAP/p1/M4_L08_L09.md | 2 |
| 问题扫描/findings/D_COMMENT/p2/M5a_L11.md | 2 |
| 问题扫描/findings/C_DOC_CODE_GAP/p2/M3_L05_L07.md | 2 |
| 问题扫描/findings/D_COMMENT/p1/M3_L05_L07.md | 2 |
| 问题扫描/findings/E_TRACE_BREAK/p1/M2b_L03_L15.md | 2 |
| 问题扫描/findings/G_GOV_GATE/p1/M9_L24_L26.md | 2 |
| 问题扫描/findings/H_NUMERIC/p1/M2b_L03_L15.md | 2 |
| 问题扫描/findings/D_COMMENT/p2/M1a_L01_L02.md | 2 |
| 问题扫描/findings/E_TRACE_BREAK/p2/M1a_L01_L02.md | 2 |
| 问题扫描/findings/G_GOV_GATE/p0/M7_G_GOV_GATE_p0.md | 2 |
| 问题扫描/findings/G_GOV_GATE/p2/M2a_L04_L10.md | 2 |
| 问题扫描/findings/C_DOC_CODE_GAP/p0/M2a_L04_L10.md | 2 |
| 问题扫描/findings/C_DOC_CODE_GAP/p1/M3b_L06.md | 2 |
| 问题扫描/findings/E_TRACE_BREAK/p2/M7_E_TRACE_BREAK_p2.md | 2 |
| 问题扫描/findings/H_NUMERIC/p1/M3b_L06.md | 2 |
| 问题扫描/findings/C_DOC_CODE_GAP/p2/M6a_L13_L14.md | 2 |
| 问题扫描/findings/B_STD_MISMATCH/p1/M2a_L04_L10.md | 2 |
| 问题扫描/findings/C_DOC_CODE_GAP/p1/M9_L24_L26.md | 2 |
| 问题扫描/10_PROTOCOL.md | 1 |
| 问题扫描/findings/G_GOV_GATE/p2/M8a_L25_L27.md | 1 |
| 问题扫描/findings/H_NUMERIC/p2/M8_L22_L23.md | 1 |
| 问题扫描/_merge/M4.md | 1 |
| 问题扫描/findings/B_STD_MISMATCH/p1/M8_L22_L23.md | 1 |
| 问题扫描/findings/F_TEST_GAP/p0/M3_L05_L07.md | 1 |
| 问题扫描/findings/G_GOV_GATE/p0/M3b_L06.md | 1 |
| 问题扫描/findings/E_TRACE_BREAK/p2/M5b_L12_L17.md | 1 |
| 问题扫描/findings/D_COMMENT/p2/L28b.md | 1 |
| 问题扫描/findings/H_NUMERIC/p1/M7_H_NUMERIC_p1.md | 1 |
| 问题扫描/findings/D_COMMENT/p1/M1a_L01_L02.md | 1 |
| 问题扫描/findings/A_SCI_DEF/p0/M3_L05_L07.md | 1 |
| 问题扫描/findings/A_SCI_DEF/p0/M7_A_SCI_DEF_p0.md | 1 |
| 问题扫描/findings/A_SCI_DEF/p1/M3b_L06.md | 1 |
| 问题扫描/findings/C_DOC_CODE_GAP/p0/M3_L05_L07.md | 1 |
| 问题扫描/findings/E_TRACE_BREAK/p1/M3b_L06.md | 1 |
| 问题扫描/findings/I_DOC_HYGIENE/p2/M1a_L01_L02.md | 1 |
| 问题扫描/findings/A_SCI_DEF/p2/M8_L22_L23.md | 1 |
| 问题扫描/findings/A_SCI_DEF/p0/M2a_L04_L10.md | 1 |
| 问题扫描/findings/B_STD_MISMATCH/p1/M1a_L01_L02.md | 1 |
| 问题扫描/findings/F_TEST_GAP/p1/M9_L24_L26.md | 1 |
| 问题扫描/findings/F_TEST_GAP/p2/M9_L24_L26.md | 1 |
| 问题扫描/findings/G_GOV_GATE/p1/M6a_L13_L14.md | 1 |
| 问题扫描/findings/C_DOC_CODE_GAP/p0/M6a_L13_L14.md | 1 |
| 问题扫描/findings/D_COMMENT/p2/M3b_L06.md | 1 |
| 问题扫描/findings/E_TRACE_BREAK/p1/M8a_L25_L27.md | 1 |
| 问题扫描/findings/G_GOV_GATE/p1/M1a_L01_L02.md | 1 |
| 问题扫描/findings/G_GOV_GATE/p1/M3_L05_L07.md | 1 |
| 问题扫描/findings/G_GOV_GATE/p2/M1a_L01_L02.md | 1 |
| 问题扫描/findings/G_GOV_GATE/p2/M4_L08_L09.md | 1 |
| 问题扫描/findings/H_NUMERIC/p1/M1a_L01_L02.md | 1 |
| 问题扫描/findings/B_STD_MISMATCH/p2/M8a_L25_L27.md | 1 |
| 问题扫描/findings/A_SCI_DEF/p1/M4_L08_L09.md | 1 |
| 问题扫描/findings/F_TEST_GAP/p2/M5b_L12_L17.md | 1 |
