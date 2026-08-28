# G5 Linux 构建/测试预发布 —— 基线核验

> G5 = BLD-001..004 + TST-001..005。本轮完成**构建 + 测试基线**核验并记录发现。
> 复核：2026-08-27。

## 1 构建（TST-001 / BLD-001）

- 目标构建成功：`cmake --build build/linux-openmp-on --target phase2_synthetic_gate
  phase2_ivar_wiring phase2_execution_options -j4` → `[100%] Built`, exit 0（gcc 14.2, Release+OpenMP）。
- **BLD-001 发现**：**无根级可重复 Linux configure/build/test 入口**。仅 `lib/phase2` 与
  `lib/acr` 有 `CMakeLists.txt`（`project(astro_phase2)` / `project(...)`，单模块项目）；
  其余模块（snr_estimator/healpix_db/orchestrator 等）无 CMake 目标；仓库无根 `CMakeLists`/
  `Makefile`/`CMakePresets`/`build.sh`；v19 `BUILD_RELEASE.md` 仅给出 Windows `toolchain.ps1`。
  ⇒ BLD-001 未满足（缺根级入口）。

## 2 测试结果（TST-002）

| 测试目标 | 结果 | 备注 |
|---|---|---|
| `phase2_synthetic_gate` | **81 PASSED / 0 FAILED**（91 ran, 10 SKIPPED ≈ 33s） | SKIP 为环境相关 RealHips/Identity 用例 |
| `phase2_execution_options` | 6 PASSED | ✓ |
| `phase2_routing` | 4 PASSED | ✓ |
| `phase2_ivar_wiring` | **1 FAILED** | `WireProductionStage2PerFrameIvar` 在 `ivar_wiring_test.cpp:274` |
| `phase2_async_io_test` | 二进制未找到（`phase2_async_io_test` 命名不符） | 需核对实际 target 名 |

## 3 ivar_wiring 失败归因

- `ASSERT_EQ(rc,0)`（`:274`）失败，`stage2 (A,B,C) 运行失败 rc=32512`（= 127×256 ⇒ 子进程以
  127 退出，即 **stage2 未找到/加载失败**），非逻辑断言失败。为**测试 harness 环境问题**
  （`run_stage2` 路径/数据），非科学/代码逻辑 bug。
- 已在干净代码树上复现（本会话仅改文档，未改 `lib/`）；属**预存环境问题**，待 TST-002 需
  `FAIL=0` 时须修复 harness 或提供 stage2 可执行路径后复跑。

## 4 结论

- 构建可复现、核心 synthetic_gate 通过（81/0）；2 个轻量测试通过。
- G5 基线记录：**BLD-001 缺根级入口**（真实缺口）；**TST-002 有 1 个环境性 FAIL**
  （ivar_wiring/harness），需修复后重验。G5 任务暂置 NOT_STARTED（待完整门禁）。
- 工作区存在**非本会话**改动（`AGENTS.md` M、`evidence/` 删除、`traceability_check.json` M）——
  本会话未改、未提交，按规则不动 `AGENTS.md`。
