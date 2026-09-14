# 包取证摘要：AstroCS_CP0

身份由脚本归一（去复原前缀与 .zip 后缀）。共 2 个实例。

## 【zip】reports/REAUDIT_V3/cp0_out/AstroCS_CP0.zip
- 文件 18 | 51.8 KB
- sha256: c79f50d7cdf8e9348d96b516cdead13bf94b2c649612a0591c4240fcb84f8e82
- git 事件: {'first': None, 'last': None, 'dels': None, 'adds_n': 0, 'mod_n': None, 'touch_last': 'ac2ced532a1abbbfab620d3390e277cfa1f0ecdc 2026-08-27T23:11:52+08:00'}
- zip 顶层: v3_cp0
- zip 内 marker: 00_READ_FIRST.md, MANIFEST.json
- zip 内规格文件: 00_READ_FIRST.md
- zip 内台账: v3_cp0/TASK_LEDGER.csv 表头 ['task_id', 'gate', 'depends_on', 'scope', 'required_commit', 'required_push', 'checkpoint', 'status'] 行 62
- 00_READ_FIRST 开头:

# AstroCS 预发布重审 (V3) — CP0 身份与数据审核包

## 候选/起点
- 起点 SHA (start_sha): `535e73879662346ee1f599d7a9cae96c6c23680d` (= `origin/main` 经 `git fetch origin --prune` 后实测)。
- 候选 SHA (candidate_sha): `535e73879662346ee1f599d7a9cae96c6c23680d` — 本轮 ID-001..003 为身份/数据冻结,
NaN
- 历史锚: A=`b38b446e6`, B=`83471979a`(预定接缝基线), C=V3起点`origin/main`(本轮起点), D=V3完成候选(尚未产生)。

## Gate
- 当前 Gate: **G0 身份冻结**。
- 目标: 起点 SHA、干净状态、工具链、数据 manifest。

## 状态
- 状态词: `AWAITING_EXTERNAL_REVIEW` (Agent 仅报告证据, 最终 PASS/REJECT 由外部审核人裁决)。
- 本轮完成: ID-001, ID-002, ID-003 (三条均 PASS)。
- 未完成项: ID-004 .. PKG-001 全部 `NOT_STARTED` (ID-004.. 之后的 Gate 未进入)。

## 计数
- 所有计数由 `SUMMARY.json` 单源提供 (本文件不维护独立计数)。
- 任务计数 / 发现计数 / 测试计数 / 构建计数 见 `SUMMARY.json`。

## 结论
- 未提交、未 push、未创建分支; 仅 main; 三 SHA 相同。
- 外部工作区变化(4 项)逐项登记于 `EXTERNAL_WORKTREE_CHANGES.md`, 无来源不明修改。
- 32R 数据与 V2 manifest 完全一致 (35 文件 hash/size 全对, 11+11+10, hash 唯一), 未自动替换基线。
- 结论: **AWAITING_EXTERNAL_REVIEW**。


## 【zip_recovered_from_history】history_zip/AstroCS_CP0.zip
- 文件 18 | 51.8 KB
- sha256: c79f50d7cdf8e9348d96b516cdead13bf94b2c649612a0591c4240fcb84f8e82
- git 事件: {'first': None, 'last': None, 'dels': None, 'adds_n': 0, 'mod_n': None, 'touch_last': 'ac2ced532a1abbbfab620d3390e277cfa1f0ecdc'}
- zip 顶层: v3_cp0
- zip 内 marker: 00_READ_FIRST.md, MANIFEST.json
- zip 内规格文件: 00_READ_FIRST.md
- zip 内台账: v3_cp0/TASK_LEDGER.csv 表头 ['task_id', 'gate', 'depends_on', 'scope', 'required_commit', 'required_push', 'checkpoint', 'status'] 行 62
- 00_READ_FIRST 开头:

# AstroCS 预发布重审 (V3) — CP0 身份与数据审核包

## 候选/起点
- 起点 SHA (start_sha): `535e73879662346ee1f599d7a9cae96c6c23680d` (= `origin/main` 经 `git fetch origin --prune` 后实测)。
- 候选 SHA (candidate_sha): `535e73879662346ee1f599d7a9cae96c6c23680d` — 本轮 ID-001..003 为身份/数据冻结,
NaN
- 历史锚: A=`b38b446e6`, B=`83471979a`(预定接缝基线), C=V3起点`origin/main`(本轮起点), D=V3完成候选(尚未产生)。

## Gate
- 当前 Gate: **G0 身份冻结**。
- 目标: 起点 SHA、干净状态、工具链、数据 manifest。

## 状态
- 状态词: `AWAITING_EXTERNAL_REVIEW` (Agent 仅报告证据, 最终 PASS/REJECT 由外部审核人裁决)。
- 本轮完成: ID-001, ID-002, ID-003 (三条均 PASS)。
- 未完成项: ID-004 .. PKG-001 全部 `NOT_STARTED` (ID-004.. 之后的 Gate 未进入)。

## 计数
- 所有计数由 `SUMMARY.json` 单源提供 (本文件不维护独立计数)。
- 任务计数 / 发现计数 / 测试计数 / 构建计数 见 `SUMMARY.json`。

## 结论
- 未提交、未 push、未创建分支; 仅 main; 三 SHA 相同。
- 外部工作区变化(4 项)逐项登记于 `EXTERNAL_WORKTREE_CHANGES.md`, 无来源不明修改。
- 32R 数据与 V2 manifest 完全一致 (35 文件 hash/size 全对, 11+11+10, hash 唯一), 未自动替换基线。
- 结论: **AWAITING_EXTERNAL_REVIEW**。

