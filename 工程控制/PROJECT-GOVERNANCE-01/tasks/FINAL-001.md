# 任务：FINAL-001 独立总审计与差距闭合报告

状态：`NOT_STARTED`
层：L9　依赖：REAL-001　文件域互斥组：S11

## 目标

逐项复跑 GAP-001..020 对应的机器门与关键证据，核对每任务 diff 文件域/提交/状态/依赖，输出模块状态阶梯与遗留项，形成负责人审阅材料（不作发布决定）。

## 基线状态（编制时实测）

- 控制包编制时 3 项旧门为红（GAP_AUDIT §0）；本任务必须独立复跑而非复用他人自述。

## 权威依据
- ASTROCS_DESIGN.md §11.3（状态阶梯唯一口径）、§12（发布决定只属负责人）
- CONTROL_PACK_SPEC.md §7（独立验证三层）、§9（阶段汇总与归档）

## 改动范围（文件域）
### 允许改
- 工程控制/PROJECT-GOVERNANCE-01/{GAP_AUDIT,ACCEPTANCE,SUMMARY}.md
- reports/PROJECT-GOVERNANCE-01/final/**

### 禁止改
- 产品源码
- 伪造 PASS
- 代表负责人发布
- 复用 SubAgent 自述代替复跑

## 步骤
1. 逐条复跑 GAP-001..020 的机器门与关键证据，记录命令、退出码与输出摘要。
2. 核对每任务的 diff 文件域、提交、状态、依赖是否满足；越界改动必须登记。
3. 输出模块状态表：23 个模块 × CONTRACT_READY/IMPLEMENTED/INSTALLED/VERIFIED + 负向状态。
4. 生成负责人审阅材料：闭合项、遗留项、BLOCKED/UNRESOLVED 清单、建议裁决项；**不写发布决定**。

## 验收门（前台独立复跑）
- [ ] 每条 GAP 都有：任务 / 提交 / 证据路径 / 结论 四要素（给出对照表）
- [ ] 无 FAIL/BLOCKED/UNRESOLVED 时才可写控制包完成；否则如实保留
- [ ] 报告明确写出发布决定仅属负责人，且状态不超过 READY_FOR_OWNER_REVIEW
- [ ] 所有 PASS 结论都来自前台本次复跑，附命令与退出码

## 证据命令
```bash
mkdir -p run/PROJECT-GOVERNANCE-01/FINAL-001/logs
python3 ci/run_checks.py --all --json-out run/PROJECT-GOVERNANCE-01/FINAL-001/logs/ci_result.json; echo "rc=$?"
git log --oneline a861d8f6..HEAD | tee run/PROJECT-GOVERNANCE-01/FINAL-001/logs/commits.txt
```

## 执行规则（每个任务都适用）

- 开工前按 `AGENTS.md §1` 读完本任务「权威依据」列出的全部条款；未读不开工。
- 只改本文件「改动范围」声明的文件域；**不顺手改无关代码**；不改 `docs/science/**` 公式与 `docs/algorithms/**` 推导。
- 科学公式、权重/variance/ivar/SNR 定义、排异规则、归约顺序、默认容差**一律只读**（ENGINEERING_SPEC §3）。
- 工作区在本控制包编制前已有大量预存修改与未跟踪文件（见 BASE-001）：**不得** reset/stash/clean/checkout，**不得**把它们混进本任务的改动。
- SubAgent 零 git 写权限：不 commit、不 push、不建分支；改动留在工作区并交自证材料，由前台按任务原子提交。
- 所有外部命令带 `timeout`，日志落 `run/PROJECT-GOVERNANCE-01/<任务ID>/logs/`（`run/` 已 gitignore，不入库）。
- 报告「完成」必须附：命令、退出码、关键输出片段、产物路径。禁止用「环境问题/工具问题」掩盖失败。
- 发现文档冲突、科学歧义或无法证明的状态：登记 `BLOCKED`（控制包内）或 `UNRESOLVED`（GAP_AUDIT 内），不得自行选口径。

## 交付物

1. 限「改动范围」内的代码/合同/配置改动；
2. `run/PROJECT-GOVERNANCE-01/<任务ID>/logs/` 下的命令日志与机器输出；
3. 自证摘要（字段：任务ID / 改动文件清单 / 逐条验收命令与退出码 / 未决项）。
