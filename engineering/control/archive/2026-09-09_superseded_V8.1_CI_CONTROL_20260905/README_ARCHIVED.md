# ARCHIVED_NON_NORMATIVE — 已被替代的控制包 V8.1（2026-09-09 归档）

> **本目录整体 ARCHIVED（GOV-002 归档，ASTROCS-CONSTITUTION-ALIGNMENT-V1 控制面执行）**。
> 目录内全部 56 个 Git 跟踪文件为
> `AstroCS_ALPHA0.11.0_EXISTING_WORKSPACE_CI_CONTROL_V8_1_20260905`
> （ASTROCS-ALPHA0.11.0-EXISTING-WORKSPACE-CI-CONTROL-V8.1）原样移动副本，
> 逐文件 SHA-256 经移动前后全量比对零差异；不再作为当前工程权威，
> 不得作为当前证据引用，仅作历史线索（宪章 §1.1 权威分层第 8 条）。
>
> 归档日期：2026-09-09（控制包 GOV-002）
> 原路径：`engineering/control/active/AstroCS_ALPHA0.11.0_EXISTING_WORKSPACE_CI_CONTROL_V8_1_20260905/`
> 归档动作：`mv engineering/control/active/…V8_1_20260905 engineering/control/archive/2026-09-09_superseded_V8.1_CI_CONTROL_20260905`
>（56 个 tracked 文件路径变更由前台以 rename 记账提交；SubAgent 不 git add/commit）

## 替代（当前权威）

| 用途 | 当前权威位置 |
|---|---|
| 工程最高约束（负责人冻结） | 仓库根 `ASTROCS_PROJECT_CONSTITUTION.md`（ASTROCS-CONSTITUTION-001，FROZEN；supersession 生效） |
| 当前执行控制包 | `工程控制/AstroCS_CONSTITUTION_ALIGNMENT_CONTROL_V1_20260909/`（ASTROCS-CONSTITUTION-ALIGNMENT-V1，唯一 ACTIVE） |
| 控制包活动状态总表 | `工程控制/ACTIVITY_STATE.md`（GOV-002 建立，唯一登记源） |
| 历史约束参照（ARCHIVED） | 仓库根 `AstroCS_ENGINEERING_CONSTRAINTS.md`（原 `01_FROZEN_CONSTRAINTS.md` 的上级来源，已降级） |
| 文档分类索引 | `docs/DOCUMENT_INDEX.yaml` |
| 当前负责人入口 | `REVIEW.md` + `docs/owner/` |

## 归档理由

1. V8.1 的治理前提（根级 `AstroCS_ENGINEERING_CONSTRAINTS.md` 作为 ACTIVE_NORMATIVE
   冻结约束、工作根 `TASK_LEDGER.csv` 台账、V8.0 作废声明中的节点假设）已被
   冻结宪章 supersession 条款替代或收敛；
2. 其任务域（workspace 接管/CI 机器合同/Windows 与 Fatduck 验证）已并入
   ASTROCS-CONSTITUTION-ALIGNMENT-V1 的 Gate 体系（02_GATES_AND_EXECUTION.md）；
3. `engineering/control/active/` 按宪章 §14.2 职责树只保留当前控制，
   历史包一律归档（`engineering/control/archive/`）。

## 保留内容说明

- `15_SUPERSESSION_NOTICE.md`（V8.0 作废声明）随包原样保留，其声明的
  V8.0 作废事项仍然有效，但"V8.1 是唯一有效续作包"一句已失效——
  唯一有效控制包现为 ASTROCS-CONSTITUTION-ALIGNMENT-V1（见替代表）。
- `baseline/`、`ci/`、`tasks/`、`templates/`、`validators/` 等子目录原样保留，
  供审计追溯与历史证据核对，SHA256SUMS 未改动。
- 本 README 为归档时唯一新增文件（GOV-002）。
