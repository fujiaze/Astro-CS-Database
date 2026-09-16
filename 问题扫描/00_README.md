# 全项目只读问题扫描工作区（READ-ONLY AUDIT SWEEP）

> **REBASE 抬头（ROOT-004，2026-09-16）**：本目录是**上一轮（旧基线）**的 bug 清单，其判据锚在旧宪章、旧工程约束、旧控制包路径与旧文档体系上。
> **现行结论一律以 `问题扫描/REBASE_TABLE.md` 为准**（口径见 `问题扫描/REBASE.md`；接续入口见 `reports/PROJECT-GOVERNANCE-01/root-scan/CONTINUATION.md`）。
> 本目录的 `findings/**`、`_cache/**`、`_merge/**`、`_recheck/**`、`_verify/**`、`账本/**`、`INDEX.md`、`SUMMARY.md` 一律**保留为历史证据**，不再单独作为整改依据；原始 finding 文件**零删除**。

- 工作区代号：`RQS-2026-01`｜模式：**只读研究，零执行**（不构建、不测试、不跑脚本、不碰 git）
- 建立原因：负责人直接要求「在根目录创建一个文件夹，把全项目搜寻到的所有问题按类别与 P0/P1/P2 建子目录归档」。
  按 AGENTS.md「目录规范」，根目录新增条目需登记并经负责人确认；本目录即负责人本会话显式指定产物，
  后续是否迁入 `reports/` 由负责人裁定，本工作区不改仓库任何其他文件。
- 作业规程（所有子代理必读，含类别定义、优先级判据、finding schema、禁改清单）：见 `10_PROTOCOL.md`

## 目录结构

```
问题扫描/
├── 00_README.md            本文件：导航与统计
├── 10_PROTOCOL.md          作业规程（类别/优先级/schema/只读纪律）
├── 20_AGENT_PLAN.md        分层代理派发计划与归属表
├── _cache/                 第一层：叶子扫描代理的原始档案（只追加，不改写他人档案）
├── _merge/                 第二层：合并验证代理的核对与改判记录
├── findings/               第二层产物：按类别 × 优先级归档的定稿问题
│   ├── A_SCI_DEF/p0|p1|p2      科学定义与推导
│   ├── B_STD_MISMATCH/         国际标准偏离
│   ├── C_DOC_CODE_GAP/         文档—代码—接口割裂
│   ├── D_COMMENT/              注释不清晰
│   ├── E_TRACE_BREAK/          追溯与锚点断裂
│   ├── F_TEST_GAP/             测试与验证缺口
│   ├── G_GOV_GATE/             治理与门禁
│   ├── H_NUMERIC/              数值与稳定性
│   └── I_DOC_HYGIENE/          文档陈旧与可维护性
├── INDEX.md                第三层：全部 finding 一表索引（前台维护）
└── SUMMARY.md              第三层：前台归纳（根因聚类、系统性结论、待负责人裁定项）
```

## 三级流水线（自下而上）

1. **叶子扫描（L01–L18）**：各领一片「SCI 文档 ↔ ALG 文档 ↔ 实现 ↔ 测试」全链，写 `_cache/Lxx.md`。
2. **合并验证（M1–M6）**：逐条重新读原文核对证据、去重、改判优先级与类别，写 `findings/<类别>/pN/<slice>.md`
   与 `_merge/Mx.md`（含剔除理由）。
3. **前台归纳**：交叉校验各合并域、亲自复核全部 P0，写 `INDEX.md` 与 `SUMMARY.md`。

## 状态

- 进行中。统计与各优先级数量在 `INDEX.md`、归纳结论在 `SUMMARY.md`。

## 阅读建议

先看 `SUMMARY.md`（系统性问题与根因），再看 `findings/A_SCI_DEF/p0`、`findings/B_STD_MISMATCH/p0`、
`findings/C_DOC_CODE_GAP/p0`（科学正确性与合规），最后按类别看 P1/P2。每条 finding 自带证据行号与权威依据。
