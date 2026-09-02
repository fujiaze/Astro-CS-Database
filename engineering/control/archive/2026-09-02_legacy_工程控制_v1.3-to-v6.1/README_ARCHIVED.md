# ARCHIVED_NON_NORMATIVE — 历史工程控制包（2026-09-02 归档）

> **本目录整体 ARCHIVED（GOV-002 归档）**。目录内全部文件（控制包、tasks、
> evidence、docs、checklists、agent、contracts、templates、tools 等共 854 个
> Git 跟踪文件）为 V1.3 Recovery Pack 及 V3/V4/V5/V6/V6.1 历史控制包内容，
> 不再作为当前工程权威，不得作为当前证据引用。
>
> 归档日期：2026-09-02（GOV-002，wave W1）
> 原路径：`工程控制/`（仓库根中文目录，2026-07-29 由 `engineering_v1.3/`
> 重命名而来，commit 036a3bb）
> 归档动作：`git mv 工程控制 engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1`

## 替代（当前权威）

| 用途 | 当前权威位置 |
|---|---|
| 工程约束（负责人冻结） | 仓库根 `AstroCS_ENGINEERING_CONSTRAINTS.md`（GOV-001 建立） |
| 当前执行控制包 | 工作根 `control/active/AstroCS_V7_MODULAR_REFOUNDATION_CONTROL_20260902_FINAL3/`（ASTROCS-ALPHA3-MODULAR-REFOUNDATION-V7） |
| 文档分类索引 | `docs/DOCUMENT_INDEX.yaml`（GOV-002 建立） |
| 科学/算法权威 | `docs/science/`、`docs/algorithms/`（当前提交） |
| 模块文档 | `docs/modules/`、`docs/architecture/`（当前提交） |
| 当前负责人入口 | `REVIEW.md`（GOV-004 重建为 `docs/owner/`） |

## 保留理由

本目录文件全部由 Git 追踪（854 个），保留用于审计追溯与科学/工程来源核对，
**不删除**。V4/V5/V6 控制包内含科学验证矩阵、任务结果与审核结论，作为历史
来源仍有参考价值；其 SHA256SUMS 保持原样以维持包完整性校验。

## 机器索引

- DOCUMENT_INDEX.yaml 对本目录登记为：`status: ARCHIVED_NON_NORMATIVE`
  （目录级聚合条目；控制包内文件不再逐文件登记，避免与包内 SHA256SUMS 冲突）。
- `tools/doccheck/check_doc_index.py` 校验：active 索引不包含任何
  `ARCHIVED_NON_NORMATIVE` 路径前缀 `engineering/control/archive/` 的文件。
