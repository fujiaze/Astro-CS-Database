# 双线合同与冻结点（CONTRACT-501 / RELEASE-05）

> 上游：ASTROCS_DESIGN.md §8（软件架构）、§9（CPU 后端与资源）；ENGINEERING_SPEC.md §4.1（管线纪律）
> 依据：控制包 RELEASE-05 `00_README.md` §3（双线纪律）、`TASK_LIST.md`（文件域列）

> ID: CONTRACT-501-DUAL-LINE  状态: FROZEN（RELEASE-05 冻结点）  机器 schema: `eng/contracts/schemas/dual_line_file_domain.schema.json`

## 1 目的

A/B 双线并行期间，双方**文件域互斥**、接口约定先冻结，避免 merge 冲突与互相阻塞；争议有据可依。

## 2 文件域

| 线 | 文件域（glob） | 内容 |
|---|---|---|
| A（代码架构） | `lib/**`、`实验/**/code`、`实验/**/results` | 命名块机制、三阶段调度器、节点改写、旧架构退役、死代码、探针性能优化 |
| B（门禁测试合同） | `eng/ci/**`、`eng/tests/**`、`eng/contracts/**`、`docs/contracts/**`、`docs/ci/**` | 门禁合理性审计与修复、测试合理性审计与补充、合同预先约定、双向对应 |
| 共享面（阶段 1 冻结） | `docs/science/**`、`docs/plugins/**`、`docs/architecture/**`、`docs/owner/**` | DOC-502 一次性改完冻结；双线期间改动走前台登记、串行合并 |
| 报告面（SCI） | `实验/**/REPORT_paper.md` | 归 SCI-503/504/505，双线期间只增不改 |

## 3 域边界纪律（违反即回退）

- A 线：不改门禁阈值/注册表、不改测试断言口径；
- B 线：不改 `lib/` 生产代码；发现代码缺陷**只登记派单**给 A 线，不越域；
- 两线均不改共享面（阶段 1 后）；确需改动 ⇒ 前台登记 + 串行合并。

## 4 数值等价基线

- A 线节点改写以 **B 线审计前的现有测试**为数值等价基线；
- B 线若需改变科学断言口径 ⇒ 提差异单，在每日合并点由前台裁决，不私自改；
- 三命令合成全链等价比对容差按 Oracle 冻结，**不放宽**（ARCH-505 步骤 1/4）。

## 5 合并点规则

1. 前台按序**串行提交**，一个 commit = 一个明确目的；
2. 触及共享面 ⇒ 先在 `工程控制/RELEASE-05/ACCEPTANCE.md` 的「双线文件域冲突登记」表登记，再由前台裁决；
3. 科学口径争议 ⇒ 按 AGENTS.md §8 查证流程（子代理并行查一手文献 + 开源实现 → 独立比对 → 裁决）；仍无法收敛 ⇒ 写入 `OPEN_QUESTIONS.md`，不硬改。

## 6 冻结点

本文件与同批的 `PIPELINE_BLOCK_CONTRACT.md`、`SCHEDULER_CONTRACT.md`、`PERF_GATE_CONTRACT.md` 在 CONTRACT-501 完成后**冻结**；冻结后变更走差异单（登记于 `工程控制/RELEASE-05/ACCEPTANCE.md`）。
