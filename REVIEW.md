# REVIEW.md — AstroCS 项目负责人审查入口（L0）

> 目标版本: **0.10.0-alpha.1**  V6 系统重构 (AstroCS_V6_SYSTEM_REFACTOR_ALPHA_CONTROL)
> 项目负责人只需阅读本文件及其链接的 5 份顶层文档。

## 1. 总览

AstroCS 从多帧天文 CCD 图像估计统一天球辐射场 (HiPS signal) 及其不确定性
(variance/ivar)，输出标准 IVOA HiPS 产品与平面 FITS。本重构将 V5 遗留单体
拆分为三阶段模块链，建立唯一生产路径并逐门验收。

## 2. 顶层文档 (点击审阅)

| 文档 | 内容 |
|---|---|
| [SCIENCE_OVERVIEW](docs/review/SCIENCE_OVERVIEW.md) | 核心科学定义、公式、假设、不变量 (SCI-001..) |
| [PIPELINE_OVERVIEW](docs/review/PIPELINE_OVERVIEW.md) | Phase1/2/3 数据流与模块关系 |
| [ARCHITECTURE_OVERVIEW](docs/review/ARCHITECTURE_OVERVIEW.md) | CLI、I/O、模块、计算后端、退出路径 |
| [RELEASE_STATUS](docs/review/RELEASE_STATUS.md) | 通过项、未验证项、发布限制 |
| [CHANGE_REVIEW](docs/review/CHANGE_REVIEW.md) | 本轮变化、科学影响、测试证据、待决策事项 |

## 3. 当前进度 (G0..G11)

- 已过门: **G0/G1/G2/G3/G4/G5/G6/G7** (证据 `evidence/refactor/gates/G*/CHECKLIST.md`)。
- 当前: G8 文档与质量机器门 (DOC-002..005)。
- 任务状态唯一源: `evidence/refactor/TASK_LEDGER.csv`。

## 4. 关键结论

- 科学定义 = 算法 = 接口 = 代码 = 测试 (六层追溯, TRACE-001)。
- 生产仅纯 CPU; ACR dormant; 旧 Orchestrator/PipelineEngine 调度/Stage2/drizzle 直连全部退出。
- 版本单源 0.10.0-alpha.1; CLI `--version` 输出 `0.10.0-alpha.1+g<commit>`。
- 最终发布需负责人 HiPS 视觉审核 (REL-004) 后才 ALPHA_RELEASE_APPROVED。
