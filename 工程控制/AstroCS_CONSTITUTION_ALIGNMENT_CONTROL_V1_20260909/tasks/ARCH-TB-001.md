# ARCH-TB-001｜线程预算登记与库存生成器幂等（THREAD-BUDGET + UT-ARCH 转绿）

## 目标
修复两个相关但独立目的的红灯，**须拆成两笔原子 commit**（宪章 §14.5「一个 commit 只有一个明确目的」）：

**(1) `THREAD-BUDGET`（三 profile 全红）** —— 前台实测失败原文：

```
THREAD_BUDGET_CHECK_FAIL (3):
  lib/calibration/src/module_entry.cpp:54: omp_set_num_threads 未登记: #define CAL_OMP_SET(n) omp_set_num_threads((n))
  lib/drizzle/src/module_entry.cpp:44: omp_set_num_threads 未登记: #define DRZ_OMP_SET(n)   omp_set_num_threads(n)
  lib/cosmetic/src/module_entry.cpp:64: omp_set_num_threads 未登记: #define COS_OMP_SET(n) omp_set_num_threads((n))
```

**先实证后处置**：逐处回溯宏实参 `n` 的来源链（`orchestrator`/`host budget` 注入 vs 编译期字面量）。
- 若确为 **host budget / 逐内核 benchmark 租借注入** → 按既有先例（`ac_api.cpp`、`aio_pipeline_engine.cpp`、`weighted_integration_*.cpp`）在 `tools/arch/check_thread_budget.py` 的 `REGISTERED` 中登记，**注记必须写明线程数来源**（文件级键，兼容 Windows 反斜杠路径）；
- 若为**编译期字面量** → **改代码**为预算注入，不得登记（宪章 §10.4 禁止硬编码）。
- `hardcoded_num_threads` 独立扫描**不得放宽**；登记项只豁免"未登记"，不豁免硬编码字面量。

**(2) `UT-ARCH`（linux-main 红）** —— 3 项失败，其中 1 项是 `THREAD-BUDGET` 的级联，另 2 项独立：

```
FAIL: test_05_regeneration_idempotent  —— 生成器两次运行输出逐字节不同
FAIL: test_01_real_repo_passes (TestThreadBudget) —— 上述 THREAD-BUDGET 级联
```

修**生成器确定性**（排序键、路径分隔符归一、新增测试目录稳定纳入清单——实测差异涉及 `p1hips_digest_verify` 等 WCS/AIO 线新增测试目录），令 `test_05` 逐字节幂等。**不得**删除或跳过该测试。

## 依赖
`无硬依赖`。BASE_SHA 取执行时最新 main（三 SHA 一致）。

## 写入白名单
- `tools/arch/`
- `lib/calibration/src/`
- `lib/drizzle/src/`
- `lib/cosmetic/src/`
- `tests/arch/`

## 裁决依据（前台 rev6，见 `07_FRONT_DESK_RULINGS_20260912.md`）
- 裁决 **R-10**：登记"预算注入形态"必须先实证，硬编码扫描不放宽。
- 裁决 **R-11**：库存生成器幂等性为机器一致性检查（宪章 §12.3），不得跳过测试处置。
- 宪章 §10.4 统一线程预算「线程/ISA/block 由逐内核 benchmark 选择，禁止硬编码」；§10.5 资源门禁。

## 非目标与禁令
- 不顺手修复域外问题；发现后登记 finding（05 号登记册续写）。
- 不修改/放宽科学公式、默认容差、冻结门或负责人裁决。
- 不 reset/stash/clean/rebase；不创建 branch/worktree/clone；SubAgent 不 git add/commit/push。
- **严禁以"放宽检查规则 / 加入 known-failures 基线 / 改 waivable / 跳过测试"的方式让红灯消失**（裁决 R-05、R-13）。
- 新增/修改测试必须同提交注册 CI 检查项（宪章 §17 + 负责人 CI 指令）；运行产物禁止落根目录。

## 必须动作
1. 读取冻结宪章相关条款、`docs/standards/STANDARDS_REGISTRY.md`、本任务域 SCI/ALG/DATA/ARCH 文档。
2. 测试设计先行：先红后绿、负向注入必败、1/N worker parity（适用时）、确定性 bitwise（适用时）。
3. 执行并保存命令证据（timeout/cwd/argv/起止/rc/stdout/stderr/SHA）；heavy 同 run ID 资源监控。
4. 同步订正 CI：新测试目标→`ci/checks.json` 显式检查项；漂移锚→复测工具。
5. 修复后必须**本地复现 CI 同构条件**验证（不得只在本机默认环境跑绿即报 PASS）。

## 验收
- write_scope 零越界；预存 dirty 零覆盖；所有新测试故障注入必败。
- 本任务对应的 CI 检查项在本地同构条件下 **exit 0**，并给出命令与日志路径。
- 一个任务一个原子 commit 并 push main；fetch 后核对 HEAD/main/origin/main 三 SHA。

## 返回证据
scope/acceptance/provenance 三检查 + changed_files + 命令日志 + 任务特化产物。
