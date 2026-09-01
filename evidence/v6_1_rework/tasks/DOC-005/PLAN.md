# DOC-005: 重建 L0 负责人文档

任务 ID: DOC-005
Gate: G7
依赖: DOC-002; DOC-003; DOC-004
平台: Linux
变更类别: documentation

## 目标

按 V6.1 控制包 `04_TASK_SPECIFICATIONS.md` DOC-005：

> `REVIEW.md` 只链接 SCIENCE/PIPELINE/ARCHITECTURE/RELEASE_STATUS/CHANGE_REVIEW。
> 内容由当前 ledger/trace/evidence 生成；不得互相矛盾。每个结论链接 L1/L3 hash。
> Phase 状态逐一 IMPLEMENTED/EXPERIMENTAL/NOT_IMPLEMENTED；ACR DORMANT；
> GUI NOT_INCLUDED。

## 验收项与实现对照

| 验收项 | 实现 | 证据 |
|---|---|---|
| REVIEW.md 只链接 5 份 L0 | check_l0_docs PASS(REVIEW + 5 份, 链接完整, 简洁可审) | c01 |
| 内容由当前 ledger/trace 生成 | REVIEW.md §3 指向 evidence/v6_1_rework/TASK_LEDGER.csv + COMMITS.csv 链 | c01 |
| 每个结论链接 L1/L3 hash | REVIEW.md §5 指向 TASK_RESULT.json 自引用 hash | c01 |
| Phase 状态逐一标注 | REVIEW.md §4 状态表: Phase1/2/3 IMPLEMENTED, CPU IMPLEMENTED, ACR DORMANT, GUI NOT_INCLUDED, 真实/32R/Windows EXPERIMENTAL | c01 |
| 不互相矛盾 | check_final_traceability PASS(66 claims, VERSION 单源, RELEASE_STATUS 诚实) | c02 |

## 实现文件

- `REVIEW.md`：更新为 V6.1 当前进度(G3..G10) + 组件状态表(逐项 IMPLEMENTED/
  DORMANT/NOT_INCLUDED/EXPERIMENTAL) + evidence/v6_1_rework 路径(替换旧
  evidence/refactor V6 残留)

## 测试结果

- c01: DOC-002_PASS(REVIEW + 5 L0 链接完整); c02: REL-002_PASS(追溯诚实)

## 说明

- REVIEW.md 曾指向旧 V6 evidence/refactor 路径与 G0..G11 门序; 已更新为
  V6.1 evidence/v6_1_rework + G3..G10。
