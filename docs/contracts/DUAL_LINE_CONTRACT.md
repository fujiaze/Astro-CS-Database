# 文件域与合并点合同

> 上游：docs/ASTROCS_DESIGN.md §8（软件架构）、§9（CPU 后端与资源）；ENGINEERING_SPEC.md §4.1（管线纪律）

机器 schema：`eng/contracts/schemas/dual_line_file_domain.schema.json`（文件域与合并点的机器取值源）。

## 1 目的

代码线、门禁与测试合同线、共享面的**写域互斥**：接口约定先冻结再改动，避免并发改动互相阻塞；争议有据可依。

## 2 文件域

| 线 | 文件域（glob） | 内容 |
|---|---|---|
| A（代码架构） | `lib/**`、`实验/**/code`、`实验/**/results` | 命名块机制、三阶段调度器、节点改写、探针性能优化 |
| B（门禁测试合同） | `eng/ci/**`、`eng/tests/**`、`eng/contracts/**`、`docs/contracts/**`、`docs/ci/**` | 门禁与测试的合理性审计与补充、合同预先约定、双向对应 |
| shared（共享面） | `docs/science/**`、`docs/plugins/**`、`docs/architecture/**`、`docs/owner/**` | 改动走登记，串行合并 |
| report（实验报告面） | `实验/**/REPORT_paper.md` | 只增不改 |

## 3 域边界纪律

- A 线：不改门禁阈值/注册表、不改测试断言口径；
- B 线：不改 `lib/` 生产代码；代码缺陷只登记派单给 A 线，不越域；
- 共享面：改动一律走登记，串行合并。

## 4 数值等价基线

- 节点改写以**冻结 Oracle** 为数值等价基线；
- 改变科学断言口径 ⇒ 提差异单，按差异单流程判定，不私自改；
- 三命令合成全链等价比对容差按 Oracle 冻结，**不放宽**。

## 5 合并点规则

1. 按序**串行提交**，一个 commit = 一个明确目的；
2. 触及共享面 ⇒ 差异单在本合同内登记后再合并；
3. 科学口径争议 ⇒ 按 `AGENTS.md` §8 查证流程（子代理并行查一手文献 + 开源实现 → 独立比对 → 裁决）；仍无法收敛 ⇒ 登记到未决问题正本（`docs/` 内），不硬改。

## 6 变更纪律

本文件与 `PIPELINE_BLOCK_CONTRACT.md`、`SCHEDULER_CONTRACT.md`、`PERF_GATE_CONTRACT.md` 的条款是现行硬约束：变更走差异单（§5 第 2 条），登记后串行合并。
