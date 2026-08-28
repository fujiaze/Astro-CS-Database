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
| `phase2_execution_options` | **6 PASSED** | ✓ |
| `phase2_routing` | **4 PASSED** | ✓ |
| `phase2_ivar_wiring` | **1 PASSED** | ✓（从仓库根运行；先前 FAIL 系 CWD 误用） |
| `phase2_async_io` | **10 PASSED** | ✓ |

> 注：测试须从**仓库根**调用（`stage2_exe()` 用相对路径
> `build/linux-openmp-on/astrocs-stage2` 解析 stage2；从 build 目录运行会因相对路径失效
> 返回 rc=127）。从仓库根运行全部通过。

## 3 结论

- **TST-002（phase2 模块）全绿**：synthetic_gate 81/0、ivar_wiring 1、execution_options 6、
  routing 4、async_io 10，合计 **0 FAIL**（从仓库根运行）。先前报告中的 ivar_wiring FAIL
  为**本会话调用 CWD 误用**（非代码/logic bug），已更正。
- **BLD-001 真实缺口**：无根级可重复 Linux configure/build/test 入口；仅 phase2/acr 有
  CMakeLists，且模块测试面广（drizzle/browser_qt/orchestrator 均有测试源码但**无统一 CMake
  入口**）。
- 工作区存在**非本会话**改动（`AGENTS.md` M、`evidence/` 删除、`traceability_check.json` M）——
  本会话未改、未提交，按规则不动 `AGENTS.md`。
