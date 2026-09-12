# 前台交接说明（rev6.2 执行中）

> 生成：2026-09-12，重启 harness 之前。
> 用途：重启会终止当前会话与其全部 SubAgent；本文件供**接手的会话**快速恢复上下文。

## 1. 现行执行模式（负责人指令，已写入 agent persona）

负责人指令原文：
> 「首先，禁用调 CP 框架；其次，维护工作树给所有子代理，前台负责合并。
> 会有热点文件的情况其他提取约束，或者合并为一个 agent 执行。」

* **不使用 `cp` 工具**。插件 `control-pack-runtime` 已从
  `/workspace/.dsh/profiles/web/cordis.patch.yml` 注释停用（**重启后生效**）。
* 前台**直接以 SubAgent 派发**；轻量台账 = `run/pack_state/WORK_LEDGER.json`。
* **共享工作树**：全部 SubAgent 在同一工作树 `/workspace/Astro CS Database` 直接作业，不建影子树。
* **提交/push 由前台统一执行**；SubAgent 禁止任何 git 写操作。
* **热点文件三重处置**：① `ci/checks.json` 等单一写者归前台；② 同热点目录的多个任务
  **合并为一个 SubAgent 串行执行**；③ 其余任务写域**精确到文件**，派发消息逐一点名。
* **前台验收标准**：不采信自述，**独立重跑该任务的检查**（能重跑的检查才算验收）；
  精确暂存 → 原子提交 → push → fetch 核对三 SHA。**预存 dirty 永不收编。**

## 2. 本轮已提交的交付（`e6254d4d..HEAD`，共 31 个提交）

转绿的门与对应提交（每个都经前台独立机器验收）：

| 门 | 提交 |
|---|---|
| `AGENTS-GOV` | `b40c8a49` |
| `DATA-ARTIFACTS` | `2fdd6626` |
| `CON-COMMENTS` + `CON-FULL-INTEGRATION` | `1dc5447d` |
| `UT-VERSION` | `19bdf1ab` |
| `CTEST-P1WCS-ASTROPY-CROSS` | `af56020c` |
| `CTEST-P1001-REAL-NODES` | `43f0c429` |
| `THREAD-BUDGET` + `UT-ARCH` | `da74dcad` |
| `WIN-BUILD-RELEASE` / `WIN-PACKAGE-CANDIDATE` | `27909ddf` |
| `CTEST-P1WCS-STD-F1-BRIDGE`（新注册） | `8069d85d` |
| `UT-GAIA-ZLIB`（新注册） | `0c9130d6` |

裁决与登记册：
* `工程控制/AstroCS_CONSTITUTION_ALIGNMENT_CONTROL_V1_20260909/07_FRONT_DESK_RULINGS_20260912.md`
  —— 裁决 **R-01 … R-31**。
* `工程控制/.../05_FINDINGS_REGISTER_20260911.md` —— 附录 **C**（C.1–C.9）与附录 **D**（D.0–D.7）。

## 3. 重启会中断的在跑任务（工作树中的未提交改动已快照）

见 `run/pack_state/PRE_RESTART_SNAPSHOT.md`（含每个 dirty 文件的 sha256）。

| 任务 | 写域 | 性质 |
|---|---|---|
| `MON-FIX-001` | `cli/resource_gate.h`、`cli/commands.cpp`、`cli/resource_recorder.h`、`ci/resource_monitor.py` | 修 05 号册 C.1 资源监控判决器真实缺陷（P1） |
| `UT-BACKEND-FIX-001` | `tests/backend/**` | §30.1 ivar fixture（解 UT-BACKEND 12 项 + p2002） |
| `P3-PROJ-FAILFAST-001` | `lib/core/src/module_adapters.cpp`、`docs/contracts/PUBLIC_API.md` | FG-CI-01 投影静默忽略 → fail-fast（裁决 R-31） |
| `CLI-INPUT-GATE-001` | `cli/parser.cpp`、`cli/runtime_client.cpp`、`tests/cli/**` | FG-CI-03 空输入静默成功 + FG-CI-04 权重键不可达 |
| `TEST-DISCRIM-001` | `tests/unit/p1001_real_nodes_test.cpp`、`tests/unit/p2001_real_nodes_test.cpp` | 修恒真析取式断言（附录 D.2） |

**接手时先做**：`git status` 看这些写域的半成品，逐任务决定「继续 / 回退重派」。
证据目录：`run/mon_fix_001/`、`run/ut_backend_fix_001/`、`run/cli_input_gate_001/`。

## 4. 下一批优先级（附录 D.7）

1. **`CLI-COMPLETE-GATE-001`（P0）** —— 只含 cal+cos 的不完整链被写成 `status=complete` 且
   `astrocs verify` 背书 `verify:ok`，而 `out_dir` 无 `p1_stack.json`/`p1_final.json`。
   定位 `cli/commands.cpp:1494`。**须与 MON-FIX-001 串行**（同一文件）。
2. `CLI-DEFECT-001` —— FG-CI-02/09/10（phase3 manifest artifacts 恒空、`output_dir` 默认 `"."`、
   取消路径硬编码 `write_run_manifest(".")`）。
3. `TEST-MUTATION-GATE-001` —— 常驻变异注入门（附录 D.2：p1001 对「下游照跑」「PSF 数值真伪」
   **零鉴别力**，5 组注入中 2 组未败）。
4. `REAL-001` —— 真实数据链终验（含 STD-F1 遗留的 **0.5px 绝对原点疑点**，见 D.3）。

## 5. ⚠️ 并发会话风险（必须先处理）

本工作区当时**同时有 4 个活跃会话**：

| 会话 | 标题 |
|---|---|
| `session-faedf2e7` | 接替AstroCS V2控制包执行任务（本轮前台） |
| `session-9dfa51b8` | AstroCS V2 控制包执行前台启动（**旧前台，仍在派 CPRun worker**） |
| `session-ef4c0431` | 继续修复对话问题并完成修复 |
| `session-3577b4d9` | PM (1) 审核项目，制作新的控制包 |

旧前台以 git 身份 `astrocs-frontdesk <frontdesk@astrocs.local>` 提交并 push
（本前台为 `付家泽 <fujiaze@126.com>`，可据此区分提交来源）。
**接手时必须先确认这些会话已停止**，否则文件级单一写者在跨会话层面不可强制。

## 6. 项目最高约束

`ASTROCS_PROJECT_CONSTITUTION.md`（`ASTROCS-CONSTITUTION-001`，`FROZEN`）——最高守则，
冲突以它为准，Agent 无权放宽；修改权仅在项目负责人（§1.2）。
