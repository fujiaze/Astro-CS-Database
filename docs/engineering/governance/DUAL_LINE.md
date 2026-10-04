# 双线文件域与合并点合同

上游：最高设计的软件架构与 CPU 后端与资源两章 [1]。

代码线、合同与测试线、共享面的写域互斥、数值等价基线与合并点规则。

机器 schema：`eng/contracts/schemas/dual_line_file_domain.schema.json`（文件域与合并点的机器取值源）。

## 目的

代码线、合同与测试线、共享面的**写域互斥**：接口约定先冻结再改动，避免并发改动互相阻塞；争议有据可依。

## 文件域

| 线 | 文件域（glob） | 内容 |
|---|---|---|
| A（代码架构） | `lib/**`、`实验/**/code` | 命名块机制、三阶段调度器、节点改写、探针性能优化 |
| B（合同与测试） | `eng/tests/**`、`eng/contracts/**`、B 域点名清单 | 合同与测试的合理性审计与补充、合同预先约定、双向对应 |
| shared（共享面） | `docs/science/**`、`docs/engineering/**`、shared 域点名清单 | 改动走登记，串行合并 |
| report（实验报告面） | `实验/**/REPORT_paper.md` | 只增不改 |

glob 记法：`X/**` = 该目录下全部跟踪文件；`{a,b}` = 并集。本篇的点名清单是上表的组成部分：`docs/engineering/**` 与 `docs/science/**` 整体归 shared，
因此本表的四条线互斥，任一文件只属一条。

## 文件域点名清单

文档迁移把说明文档从 docs 树下的 `ci/`、`contracts/`、`architecture/`、`owner/`、`plugins/` 五处迁到 `docs/engineering/` 与 `docs/detail/`。其中 `ci/`、`owner/`、`plugins/` 三处现为空目录（两读法复核：`test -e` 判 EXISTS，`git ls-files --cached` 判 0 篇）；`contracts/`、`architecture/` 两处仍各有在位机器可读件（4 件 / 6 件，见本节末条）。归属按各文件的迁移前文件域平移，线的定义不变。

旧目录在本节只写目录名、不写完整路径：文档索引门把正文里出现的 docs 完整路径一律按可达性引用判定，溯源散文同样判红；写全路径会让本文件凭空产生未登记悬空。

B 域（按下列点名清单计 1 篇）：

`../testing/VALIDATION_EVIDENCE.md`

来源：原 `ci/` 5 篇 ＋ `contracts/` 的说明文档 14 篇。

shared 域（按下列点名清单计 4 条；归属裁决后重算总篇数）：

`docs/engineering/{architecture/DATA_FLOW,architecture/MODULE_MAP,build/BUILD_GRAPH,build/RELEASE,contracts/ASYNC_IO,contracts/OWNERSHIP_LIFETIME,resources/PERFORMANCE_MODEL,resources/cpu/BACKEND,resources/cpu/ISA_VARIANTS,standards/CACHE,standards/COMPATIBILITY,standards/DEPENDENCY}.md`

`docs/engineering/{architecture/DATA_FLOW,api/abi/SECURE_LOADER,resources/cpu/AVX2_PROVIDER,resources/cpu/CAPABILITY_PROBE,resources/observability/RESOURCE_MONITORING,resources/observability/RUN_GRAPH,resources/observability/STRUCTURED_LOGGING}.md`

`docs/detail/{algorithms_phase1,algorithms_phase2,algorithms_phase3,infrastructure}/**`

`../../detail/00_INDEX.md`

来源：原 `architecture/` 27 篇 ＋ `owner/` 5 篇 ＋ `plugins/` 24 篇 ＋ `docs/detail/` 四目录各 1 篇目录招牌。

本行原含 9 个已删名（`ARCHITECTURE` / `ARCHITECTURE_OVERVIEW` / `ERROR_MODEL` /
`IO_AND_ATOMICITY` / `PIPELINE` / `PIPELINE_OVERVIEW` / `SCIENCE_OVERVIEW` /
`THREADING_MODEL` / `THREAD_BUDGET_ARCH`），已从点名清单移除：这些文件在仓内不存在，
点名清单按可达性判定，留着即产生悬空。其内容的后继正本为 `../architecture/ARCHITECTURE.md`（架构与阶段管线）、
`../contracts/LOG_AND_ERROR.md`（错误模型与退出码）、`../architecture/DATA_FLOW.md`
（线程预算与并行轴分配）、`../contracts/ATOMIC_PUBLISH.md`（原子发布）；
**后继正本是否计入 shared 域属归属问题，待裁**（同本节下方 science 分册的数据语义卷 一条的既有处置），
本单不擅自扩大域范围。清单标称「60 篇」的计数随本行移除同步失效，篇数待归属裁决后重算。

`PROJECT_SPEC` 同批移除，理由同上（仓内无此文件）：其「权威体系」「项目使命」「统一观测模型」三节在
`docs/engineering/` 内**无后继正本**——`../architecture/ARCHITECTURE.md` [2] 只承接了三个命令的输入输出与
隔离面一节，`docs/engineering/` 全树对「观测模型」与「项目使命」零命中。该三节的内容归属待裁，
本单不指向不承载它的邻近文件。

docs/engineering/ 与 docs/engineering/ 留在表内：两处各有在位机器可读件（4 件 / 6 件）。

**残留交叉**：`docs/engineering/**` 整体归 shared 后，B 域点名清单里唯一那一条
`../testing/VALIDATION_EVIDENCE.md` 本身落在 shared 的 glob 内，两条线在这一份文件上仍交叉。
该交叉属归属问题、待裁，本篇不擅自把它划给任一条线。

本节的生效文件域只有上面两份点名清单的反引号行（B 域 1 条、shared 域 4 条，共五条）；其余各行（含「来源」行与旧目录溯源说明）是溯源散文，不参与归属判定。

（science 分册的数据语义卷 现位于 `docs/science/unified/DATA_SEMANTICS`，随 `docs/science/` 计入 shared 域；其迁移前的 `contracts/` 位置已无此文件，归属待裁。）

## 域边界纪律

- A 线：不改判据阈值与登记面、不改测试断言口径；
- B 线：不改 `lib/` 生产代码；代码缺陷只登记派单给 A 线，不越域；
- 共享面：改动一律走登记，串行合并。

## 数值等价基线

- 节点改写以**冻结 Oracle** 为数值等价基线；
- 改变科学断言口径 ⇒ 提差异单，按差异单流程判定，不私自改；
- 三命令合成全链等价比对容差按 Oracle 冻结，**不放宽**。

## 合并点规则

1. 按序**串行提交**，一个 commit = 一个明确目的；
2. 触及共享面 ⇒ 差异单在本合同内登记后再合并；
3. 科学口径争议 ⇒ 按 `AGENTS.md` 查证流程（子代理并行查一手文献 + 开源实现 → 独立比对 → 裁决）；仍无法收敛 ⇒ 登记到未决问题正本（`docs/` 内），不硬改。

## 变更纪律

本文件与 `../contracts/PIPELINE_BLOCK.md`、`../contracts/SCHEDULER.md`、`../resources/PERFORMANCE_MODEL.md` 的条款是现行硬约束：变更走差异单（「合并点规则」一节 第 2 条），登记后串行合并。

## 参考文献

[1] 内部文档 `docs/ACSD_DESIGN.md，最高设计`，上位来源。
[2] 内部文档 `docs/engineering/architecture/ARCHITECTURE.md`，同层相关正本。
