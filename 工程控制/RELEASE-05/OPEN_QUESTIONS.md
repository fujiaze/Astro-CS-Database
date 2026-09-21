# OPEN_QUESTIONS — RELEASE-05（无法由执行 agent 裁决项）

> 依据：控制包 RELEASE-05 `00_README.md` §6（必须上呈清单）、`PROMPT.md` 第 9 条、
> AGENTS.md §10（何时必须停下来问负责人）。本文件随 REPORT-501 交付。

## Q-1（发布与成品帧判定）是否具备 0.0.1alpha 预览条件

- **状态**：未达发布门（见下），**不申请发布**；发布决定权在负责人。
- **依据**：本轮完成阶段 1 科学闭环与 CONTRACT-501，但 ARCH-501..505 / CLEAN-501 /
  PERF-501 / ACCEPT-501 / BLD-501 / E2E-501 / VIS-501 / REPORT-501 未完成；
  发布门六条（三篇论文报告 / 五一致性独立验收 / 双平台全绿 / 两组成品帧目检 /
  OPEN_QUESTIONS / FIN 授权）中仅第 1、5 条部分满足。
- **建议**：不进入 FIN；由负责人决定是否以"科学闭环 + 合同冻结"为阶段成果继续排期。

## Q-2（顶层合同结构性变更）三阶段架构目标态尚未落地，是否调整排期或范围

- **状态**：ARCH-501 已交付命名块与生命周期基础；ARCH-502/503/504（三阶段调度器）
  与 ARCH-505（20 个生产节点改写、Session/orchestrator 退役）未开始。
- **影响面**：生产路径**当前仍是整阶段 Session 装配**，不是最高设计 §8.1 的三独立
  调度器 + 命名块内存管线；"生产路径无 Session 直接装配"的验收门未达成。
- **建议**：这是本包最大未完成面，请负责人决定（a）继续按原排期推进 A 线，或
  （b）先做 ARCH-502/503/504 中风险最低的 export 子块流式作为试点。

## Q-3（控制包收口）RELEASE-04 是否物理出库

- **状态**：CONTROL_PACK_SPEC §9 要求"收口即清理"，但 RELEASE-04 是负责人交付物，
  AGENTS.md §10 禁止未经询问删除交付物。
- **现处置**：doc-index 的现行控制包已切到 RELEASE-05；RELEASE-04 入**归档白名单**，
  白名单条目必须自带 `SUMMARY.md`（收口证明），缺证明仍判红——门禁保持有牙。
- **建议**：请负责人裁决是否删除 `工程控制/RELEASE-04/`（内容已在 git 历史）。

## Q-4（规范冲突）SCI-502 的 `converged` 枚举定义

- **冲突**：控制包 `tasks/SCI-502.md` 写 `0/1/2/3 = 收敛/未收敛/达 max_iter/stalled`；
  仓内权威 `docs/plugins/algorithms_phase2/11_upm.md` §4.6 与
  `docs/algorithms/PHASE2_UPM_IMPL.md` §496 均定义
  `0=max_iter / 1=converged / 2=stalled / 3=invalid`。
- **现处置**：按 AGENTS.md §1.1「规范在别的文档去那一份读」采用**仓内权威**定义实现。
- **建议**：确认以仓内权威为准，或指示按任务书文字调整（后者需同步改两份正式文档）。

## Q-5（科学文档 vs 代码落地缺口）测光 n≥100 适用域未在生产实现

- **事实**：`docs/science/PHOTOMETRY.md` §16.5 定案"生产适用域 = 匹配星数 n ≥ 100，
  低星数帧显式降级 `degraded_reason`"，但生产硬门槛仍是 `kMinFitStars=3`
  （`frame_photometry_fit.h:70`、`module_adapters.cpp` 的 `P1_PHOT_MIN_FIT_STARS=3`），
  `lib/` 内无 `low_star_count` 与 100 阈值。
- **影响**：规范已定案、机器未落地 ⇒ 当前产品会以 n=3..99 的帧产出"已标定"结果，
  与适用域声明不符。
- **建议**：立 A 线任务实现降级路径（登记 `degraded_reason=low_star_count`），
  或由负责人裁决改为"仅文档声明、不设机器门"。

## Q-6（性能门）L2 判据是否可在本环境达标

- **状态**：PERF-501 未开始；GATE-501 正在把 L2 门改为 fail-closed 并回放 RELEASE-04
  违规数据。
- **风险**：若真判红后实测无法达到平均利用率 ≥0.85 / p50 ≥0.90，按合同**不得**用
  `record_and_justify` 或 waiver 盖红灯 ⇒ 需给证据化上限并登记本文件。
- **建议**：待 GATE-501 与 PERF-501 完成后回填。

## Q-7（双线文件域）新检查器注册归属

- **事实**：CONTRACT-501 新增 `eng/tools/contract_doc_sync.py`（双向对应校验，
  `--self-test` 4/4 红绿双向）；GATE-502 请求新增静态检查（空断言防复发）。
  两者都需要在 `eng/ci/checks.json` 注册，而该文件属 B 线域。
- **建议**：由 GATE-501 统一登记，或由负责人指定归属。

## Q-8（Windows 腿）CTRL_CLOSE 退出码 9 平台限制

- **状态**：BLD-501 未开始；G3-4 的 Windows 退出码 9 限制复核未做。
- **建议**：随 BLD-501 处置；如平台确实不可达，按"平台限制显式登记"处理，不 waiver。
