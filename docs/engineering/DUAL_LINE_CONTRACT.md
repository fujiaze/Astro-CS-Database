# 文件域与合并点合同

> 上游：docs/ACSD_DESIGN.md §8（软件架构）、§9（CPU 后端与资源）；docs/ACSD_DESIGN.md §8.2（命名块内存管线与块生命周期））

机器 schema：`eng/contracts/schemas/dual_line_file_domain.schema.json`（文件域与合并点的机器取值源）。

## 1 目的

代码线、门禁与测试合同线、共享面的**写域互斥**：接口约定先冻结再改动，避免并发改动互相阻塞；争议有据可依。

## 2 文件域

| 线 | 文件域（glob） | 内容 |
|---|---|---|
| A（代码架构） | `lib/**`、`实验/**/code`、`实验/**/results` | 命名块机制、三阶段调度器、节点改写、探针性能优化 |
| B（门禁测试合同） | `eng/ci/**`、`eng/tests/**`、`eng/contracts/**`、`docs/engineering/**`、§2.1 B 域 19 篇 | 门禁与测试的合理性审计与补充、合同预先约定、双向对应 |
| shared（共享面） | `docs/science/**`、`docs/engineering/**`、§2.1 shared 域 60 篇 | 改动走登记，串行合并 |
| report（实验报告面） | `实验/**/REPORT_paper.md` | 只增不改 |

glob 记法：`X/**` = 该目录下全部跟踪文件；`{a,b}` = 并集。§2.1 的两份点名清单是本表的组成部分。

## 2.1 文件域点名清单

文档迁移把说明文档从 docs 树下的 `ci/`、`contracts/`、`architecture/`、`owner/`、`plugins/` 五处迁到 `docs/engineering/` 与 `docs/detail/`。其中 `ci/`、`owner/`、`plugins/` 三处现为空目录（两读法复核：`test -e` 判 EXISTS，`git ls-files --cached` 判 0 篇）；`contracts/`、`architecture/` 两处仍各有在位机器可读件（4 件 / 6 件，见本节末条）。归属按各文件的迁移前文件域平移，线的定义不变。

旧目录在本节只写目录名、不写完整路径：文档索引门把正文里出现的 docs 完整路径一律按可达性引用判定，溯源散文同样判红；写全路径会让本文件凭空产生未登记悬空。

B 域 19 篇：

`docs/engineering/VALIDATION_EVIDENCE_STANDARD.md`

来源：原 `ci/` 5 篇 ＋ `contracts/` 的说明文档 14 篇。

shared 域（篇数待归属裁决后重算）：

`docs/engineering/{ASYNC_IO_CONTRACT,BUILD_GRAPH,CACHE_POLICY,COMPATIBILITY_POLICY,CPU_BACKEND_ARCH,DATA_FLOW,DEPENDENCY_RULES,EXECUTION_MODEL,ISA_BIT_MANIP_VARIANTS,ISA_VARIANTS,MODULE_MAP,OWNERSHIP_AND_LIFETIME,PERFORMANCE_MODEL,PHASE3_MODULE_ARCH,PROJECT_SPEC,RELEASE_STATUS}.md`

`docs/engineering/{abi/ABI_003_SECURE_LOADER,cpu/CPU_001_CAPABILITY_PROBE,cpu/CPU_003_AVX2_PROVIDER,execution_options_contract,observability/RESOURCE_MONITORING_CONTRACT,observability/RUN_GRAPH_CONTRACT,observability/STRUCTURED_LOGGING_CONTRACT}.md`

`docs/detail/{algorithms_phase1,algorithms_phase2,algorithms_phase3,infrastructure}/**`

`docs/detail/00_INDEX.md`

来源：原 `architecture/` 27 篇 ＋ `owner/` 5 篇 ＋ `plugins/` 24 篇 ＋ `docs/detail/` 四目录各 1 篇目录招牌。

本行原含 9 个已删名（`ARCHITECTURE` / `ARCHITECTURE_OVERVIEW` / `ERROR_MODEL` /
`IO_AND_ATOMICITY` / `PIPELINE` / `PIPELINE_OVERVIEW` / `SCIENCE_OVERVIEW` /
`THREADING_MODEL` / `THREAD_BUDGET_ARCH`），已从点名清单移除：这些文件在仓内不存在，
点名清单按可达性判定，留着即产生悬空。其内容的后继正本为 `ARCH-001.md`（架构与阶段管线）、
`ERROR_HANDLING_STANDARD.md`（错误模型与退出码）、`execution_options_contract.md`
（线程预算与并行轴分配）、`io/IO_003_ATOMIC_OUTPUT_PUBLISH.md`（原子发布）；
**后继正本是否计入 shared 域属归属问题，待裁**（同本节下方 `DATA_SEMANTICS.md` 一条的既有处置），
本单不擅自扩大域范围。清单标称「60 篇」的计数随本行移除同步失效，篇数待归属裁决后重算。

docs/engineering/ 与 docs/engineering/ 留在表内：两处各有在位机器可读件（4 件 / 6 件）。

本节的生效文件域只有上面两份点名清单的反引号行（B 域 19 篇 1 条、shared 域 4 条，共五条）；其余各行（含「来源」行与旧目录溯源说明）是溯源散文，不参与归属判定。

（`DATA_SEMANTICS.md` 现位于 `docs/science/DATA_SEMANTICS.md`，随 `docs/science/` 计入 shared 域；其迁移前的 `contracts/` 位置已无此文件，归属待裁。）

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
