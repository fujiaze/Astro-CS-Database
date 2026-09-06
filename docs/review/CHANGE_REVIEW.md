# CHANGE_REVIEW — L0 治理评审层（变更摘要）

> 文档 ID：DOC-REVIEW-CHANGE-001
> 状态：ACTIVE_INFORMATIVE（L0 治理评审汇总层）
> 目标产品：`0.11.0-alpha.2`（根 VERSION，GOV-003 唯一源）
> 本文件汇总治理评审五文档层与仓库当前状态的收敛变化，供负责人逐项审查；
> 详细变更链以 git 历史与 `docs/owner/CHANGE_REVIEW.md`（GOV-004）为权威。

## 1. 本层（docs/review/）的建立

`tools/check_l0_docs.py`（DOC-002，活跃注册于 `ci/checks.json` DOC-L0）要求
`docs/review/` 下五份治理评审文档存在且非空、`REVIEW.md` 链接五份。
docs/review/ 曾在 GOV-002 归档（现 `docs/archive/review/`，ARCHIVED_NON_NORMATIVE），
本轮按检查器要求重建，定位为 **L0 治理评审汇总层**：权威仍为
`docs/owner/`（GOV-004）与 `docs/science/`、`docs/algorithms/`、
`docs/contracts/`，本层只汇总口径与入口，不建立第二权威、不复制公式。

## 2. 内容对齐的仓库事实（静态可核）

| 事实 | 依据 |
|---|---|
| 版本 `0.11.0-alpha.2` | 根 `VERSION`（V81-ADOPT-006 统一活动版本源） |
| 三入口隔离完成 | CLI-002（commit `de2d6d7f`）：parser kRules/kHelp 删 run/graph；`cli/commands.cpp` 注释 cmd_run_pipeline/cmd_graph 已移除 |
| 资源门禁生产接线 | MON-004（commit `0b60f02a`）：`cli/resource_gate.h` evaluate_gate 进入 phase1/2/3 run（exit 10） |
| CLI 协议与实现对齐 | `docs/api/CLI_PROTOCOL_V1.md` 命令树 == `cli/parser.cpp` kRules（MAIN23 波次1 检查器对齐） |
| 纯 CPU 生产后端 | 根 CMake `ASTROCS_ENABLE_ACR` 默认 OFF；ACR DORMANT（约束 §C） |
| AGENTS.md 治理短块恢复 | commit `2dbf193c` |

## 3. 科学影响

无。本层为文档汇总层，`scientific_change=NO`；不改公式、容差、接口、源码
（约束 §E.1）；科学公式与默认容差一律以 `docs/science/`、`docs/algorithms/` 为准。

## 4. 验证（本层可执行证据）

| 检查 | 命令 | 预期 |
|---|---|---|
| L0 文档完整性（DOC-L0） | `python3 tools/check_l0_docs.py` | DOC-002_PASS / exit 0 |
| 文档索引覆盖 | `python3 tools/doccheck/check_doc_index.py --strict` | DOC_INDEX_PASS / exit 0 |
| 版本一致性 | `python3 ci/check_version.py --expected $(cat VERSION)` | VERSION_CHECK_PASS / exit 0 |

## 5. 已知限制（如实）

1. 本层与 `docs/owner/` 为双入口并存（检查器路径要求 docs/review/，治理权威
   登记 docs/owner/）；两处口径已对齐，后续收敛（若合并路径）属负责人决策。
2. `docs/owner/` 五份文档部分表述仍停留在 GOV-004 基线（如 §F.1、执行验收
   状态），其收敛属 GOV-005/owner 域，本层不代改。
3. 执行验收（合成/门禁/Windows）未在当前提交复跑，一律 NOT_VERIFIED，不冒充。

---
authoring_task: DOC-L0
authoring_layer: docs/review (L0 governance review)
base_product_version: 0.11.0-alpha.2
