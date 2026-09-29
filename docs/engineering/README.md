# 工程集（docs/engineering）

> 上游：`docs/ASTROCS_DESIGN.md` §8（架构与工程口径）。本集与 `docs/science/` 冲突时，
> 科学主张以科学集为准；与最高设计冲突时，以最高设计为准（`docs/ASTROCS_DESIGN.md` §0.1/§0.2）。

## 1 这一集是什么，不是什么

**是**：一级工程集。行为合同、架构决策、标准注册、CI 与门禁、追溯、验收、版本与状态登记——
即由科学结论推导出的「系统怎么组织、怎么保证、怎么验收」的口径。

**不是**：

- 不是逐模块的实现细节。函数名、算子级规范、模块说明卡在 `docs/detail/`。
- 不是科学公式、常数与容差。那些在 `docs/science/`，本集只引用不复制。
- 不是实验数值产出。那些在 `实验/`。
- 不是仓库根的链外脚手架。`AGENTS.md`、`ENGINEERING_SPEC.md`、`ACCEPTANCE_SPEC.md`、
  `CONTROL_PACK_SPEC.md` 属链外，本集只引用不复述。

**硬边界一句话**：本集回答「按什么规矩做」，不回答「为什么这么做」——后者是科学集。

## 2 篇目清单

按子域组织。每个子域有自己的目录与招牌件，细则不下沉到本 README。

| 子域 | 目录 | 解决什么问题 |
|---|---|---|
| 架构总览与执行模型 | `ARCHITECTURE.md`、`THREADING_MODEL.md`、`EXECUTION_MODEL.md`、`DATA_FLOW.md`、`PIPELINE.md`、`MODULE_MAP.md`、`OWNERSHIP_AND_LIFETIME.md` | 系统由哪些模块构成、谁调谁、线程与生命周期怎么约束 |
| ISA 变体与 CPU 后端 | `ISA_VARIANTS.md`、`ISA_BIT_MANIP_VARIANTS.md`、`CPU_BACKEND_ARCH.md`、`abi/` | 指令集变体怎么隔离、能力怎么声明、ABI 怎么保 |
| 性能与线程预算 | `PERFORMANCE_MODEL.md`、`THREAD_BUDGET_ARCH.md`、`performance/` | 性能模型是什么、线程预算怎么定、基线在哪 |
| 数据与接口合同 | `data/`、`io/`、`HIPS_STORAGE_FORM_CONTRACT.md`、`LOG_AND_ERROR_CONTRACT.md`、`API-001.md`、`ARCH-001.md`、`CONFIG_CONTRACT.md` | 这批字节代表什么、字段怎么定、IO 与原子性怎么保证 |
| 对外 API 版本化 | `API_STANDARD.md`、`COMMON_ABI_V1.md`、`PHASE{1,2,3}_API_V1.md`、`CLI_PROTOCOL_V1.md`、`MANIFEST_VERIFY_V1.md` | 对外接口的版本承诺与兼容边界 |
| 工程标准注册表 | `STANDARDS_REGISTRY.md`、`*_STANDARD.md`（code/concurrency/io/numeric/logging/error/... ） | 每类问题该按哪份标准写，标准本身由谁维护 |
| CI、门禁与工件 | `CI_SPEC.md`、`01_CHECKS.md`、`02_PIPELINE.md`、`03_GATES.md`、`04_ARTIFACTS.md`、`checks/` | 哪些检查是门、怎么调、产物落在哪 |
| 追溯与验收 | `TRACEABILITY_SPEC.md`、`TEST_MATRIX.md`、`RT-001.md`、`v6/` | 每条主张怎么追到代码与测试、验收矩阵在哪 |
| 文档治理与索引 | `DOCUMENT_GOVERNANCE.md`、`DEVELOPER_GUIDE.md` | 文档怎么分层、索引怎么维护、历史痕迹怎么清 |
| 版本与已知限制 | `VERSIONING.md`、`SCIENCE_FREEZE.md`、`FROZEN_GATE_INVENTORY.md` | 版本号怎么定、什么被冻结、已知限制在哪登记 |
| 负责人视图 | `ARCHITECTURE_OVERVIEW.md`、`PIPELINE_OVERVIEW.md`、`PROJECT_SPEC.md`、`RELEASE_STATUS.md`、`SCIENCE_OVERVIEW.md` | 面向负责人的复述与状态登记（见 §5 待裁决） |
| 盘点与基准数据 | `audit/`、`BASELINE.md`、`OPTIMIZATION.md`、`*_baseline_v1.md` | 仓库盘点数据、复杂度与覆盖率基线 |
| 可观测性 | `observability/`、`ASYNC_IO_CONTRACT.md`、`LOGGING_DIAGNOSTICS_STANDARD.md` | 日志、指标、追踪的采集面与约定 |

文档—代码锚设施（锚合同、行锚检查器、未解析锚登记）当前归 `docs/detail/anchors/`，
与本集「文档治理」子域相关但物理上在详细设计集，见 §5 待裁决。

## 3 从哪看起

**只想知道现在能不能发**：先看 `VERSIONING.md`，再看 `RELEASE_STATUS.md`，
再看冻结与已知限制（`SCIENCE_FREEZE.md`、`FROZEN_GATE_INVENTORY.md`）。

**要判断某个架构决定为什么这么定**：先看 `ARCHITECTURE.md` 的总原则，
再按主题进 `THREADING_MODEL.md` / `DATA_FLOW.md` / `EXECUTION_MODEL.md`。

**要接一条接口或改一个字段**：先看 `data/` 与 `io/` 的合同，
再看 `PUBLIC_API.md` 与对应的 `*_API_V1.md` 版本化承诺。

**要弄清一条要求从哪来、到哪去**：先看 `TRACEABILITY_SPEC.md` 的层定义，
再看 `CI_SPEC.md` 与 `01_CHECKS.md` 的门定义。

**要按规范写代码或写文档**：先看 `STANDARDS_REGISTRY.md` 找到对口标准，
再看 `CODE_STYLE.md` / `DOCUMENTATION_STANDARD.md` / `COMMENT_STANDARD.md`。

## 4 可信度现状

**已审定**：合同正本（`data/`、`io/`、`*_API_V1.md`）与冻结注册表（`SCIENCE_FREEZE.md`、
`FROZEN_GATE_INVENTORY.md`），以及与最高设计 §8 直接对应的架构合同。

**待审或待裁决**：见 §5。此外，逐模块的架构覆盖度尚未在迁移后重新核过，
本集对「每个模块都有对应架构条目」暂不作声明。

**会清除的脚手架**：`v6/` 是活动设计档案，若已过期会被清掉；
`audit/` 是盘点数据而非散文，只作参照不被门禁消费；
根级四篇链外脚手架在项目全周期完成后清除，本集对其只有引用关系。

## 5 需要负责人裁决的点

1. **文档—代码锚设施的归属**：行锚合同与检查器当前落 `docs/detail/anchors/`，
   但它服务的对象是全部文档而非某个模块，物理上更像工程集的治理设施。迁移前的前置表判 engineering，
   现行落位表判 detail。需裁决归哪一集。
2. **负责人视图五篇的存废**：原 `owner/` 目录已解散，其 5 篇（`ARCHITECTURE_OVERVIEW.md`、
   `PIPELINE_OVERVIEW.md`、`PROJECT_SPEC.md`、`RELEASE_STATUS.md`、`SCIENCE_OVERVIEW.md`）平铺在本集根下，
   目录 README 已并入本 README。需裁决这 5 篇是否保留独立成篇——它们与本 README 的「负责人视图」职能部分重叠。
3. **`KNOWN_LIMITATIONS` 的分面**：它横跨科学/工程/产品三面（B 类条目是科学口径限制）。
   需裁决整篇留工程集，还是按面拆分。
4. **`audit/` 的去留**：只有 CSV 无散文。需裁决是否仍单列，还是并入 `traceability/`。
5. **两篇 `TROUBLESHOOTING.md` 的处置**：原两篇（篇名同为 `TROUBLESHOOTING.md`，分处不同目录）
   内容互补不重叠，迁移时已合并为 `docs/detail/merged_TROUBLESHOOTING.md`（原第二篇的目录已随迁移消解）。
   需裁决是否保留合并形态，还是按「症状表」与「故障覆盖门」拆成两篇分置。
6. **子目录形态**：`abi/`、`cpu/`、`data/`、`io/`、`observability/`、`v6/`、`checks/` 保留为
   子目录。若改为平铺，本集导航与各子域招牌件的相对链接都要重写。

## 6 下钻指引

- 索引规则正本：`ENGINEERING_SPEC.md` §8（仓库根，链外）。
- 架构总原则与分层准入判据：`docs/ASTROCS_DESIGN.md` §8；文档分层判据见
  `DOCUMENT_GOVERNANCE.md` §2。
- 科学佐证纪律：见 `docs/engineering/DOCUMENT_GOVERNANCE.md` §2（科学佐证纪律，全局）。
- 门禁入口：`eng/ci/run_checks.py`；注册表与说明见 `01_CHECKS.md` 与
  `docs/engineering/CI_SPEC.md`（后者已迁入本集）。
- 需要公式与容差时，离开本集去 `docs/science/`；需要函数名与算子级规范时，去 `docs/detail/`。

---

> 本 README 只写现行设计，不含裁决记录、订正流水、任务编号与日期（`DOCUMENT_GOVERNANCE.md` §5）。
> 引用承重按两问判：删掉该引用结论是否一字不变（承重性）、把它挪到相邻结论后是否仍读得通（特异性）。
  细则见 `run/FINAL-07/lead-01/CARRYING_TEST_SPEC.md`。
