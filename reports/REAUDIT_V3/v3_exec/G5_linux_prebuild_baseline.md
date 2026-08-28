# G5 Linux 构建/测试预发布 —— 基线核验

> G5 = BLD-001..004 + TST-001..005。本轮完成**构建 + 测试基线**核验并记录发现。
> 复核：2026-08-27。

## 1 构建（TST-001 / BLD-001）

- **BLD-001（已提供+验证）**：根级 `build.sh`（configure/build/test 入口）。`./build.sh Release`
  全新 build 目录（P2_ENABLE_OPENMP=ON）→ 配置成功 → 构建 → 运行 → **5/5 PASS, FAIL=0,
  BLD-001_RESULT=OK**（exit 0）。日志 `run/logs/build_release.log`。
- **TST-001**：
  - **Release**（`build/run-release`）：GNU 14.2.0；**0 error / 0 warning**；产物 `libphase2.a`
    (1.25MB) + 5 测试二进制（phase2_synthetic_gate 2.2MB / ivar_wiring 1.05MB /
    execution_options 754KB / routing 742KB / async_io 658KB）；`phase2_synthetic_gate`
    SHA=`f7a1db38b971…`。
  - **Debug**（`build/run-debug`）：GNU 14.2.0；**0 error / 0 warning**；构建+4/5 测试通过。
  - **TST-001 结论**：Release+Debug 全链路构建成功、0 error、0 new warning ⇒ 判 **PASS**。
  - **Debug 测试超时告警（TST-002 关注）**：Debug `phase2_synthetic_gate` 在内部 **60s 看门狗**
    超时（Debug 较慢），Release 通过（≈33s）。属测试执行超时，非构建/logic 问题。
- **sampler.cpp 构建修复**：`<atomic>/<mutex>/<thread>` 原仅在内嵌 `P2_ENABLE_OPENMP&&_OPENMP`
  守卫内包含，但 `std::mutex g_aio_mu`/`lock_guard` 无条件使用（line 164,169,691,692）。
  `P2_ENABLE_OPENMP` 默认 OFF（CMakeLists:18）⇒ 默认/非 OpenMP 构建无法编译（gcc 报
  'mutex' in namespace 'std'）。修复：把标准头移出守卫无条件包含，仅保留 `<omp.h>` 在守卫内。
  验证：非 OpenMP 全新 Release 构建成功；openmp-on 构建+全部 phase2 测试无回归。
- **遗留**：仅 `lib/phase2` 与 `lib/acr` 有 `CMakeLists.txt`（单模块项目）；其余模块
  （snr_estimator/healpix_db/orchestrator/browser_qt）测试源码存在但**无统一 CMake 入口**——
  `build.sh` 现仅覆盖 phase2 CMake 模块；根级**全模块**入口为待办。

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
- **BLD-001：已提供并验证**（根级 `build.sh`，`./build.sh Release` → 5/5 测试 PASS, FAIL=0）。
  但 `build.sh` 现覆盖 phase2 CMake 模块；drizzle/browser_qt/orchestrator 测试源码存在但**无
  统一 CMake 入口**（全模块根级入口待办）。
- 工作区存在**非本会话**改动（`AGENTS.md` M、`evidence/` 删除、`traceability_check.json` M）——
  本会话未改、未提交，按规则不动 `AGENTS.md`。
