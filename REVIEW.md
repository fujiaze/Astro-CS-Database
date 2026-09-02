# REVIEW.md — AstroCS 项目负责人审查入口（L0）

> 目标版本: **0.10.0-alpha.1**  V6.1 返工 (AstroCS_V6_1_REWORK_CONTROL_20260831)
> 项目负责人只需阅读本文件及其链接的 5 份顶层文档。

## 1. 总览

AstroCS 从多帧天文 CCD 图像估计统一天球辐射场 (HiPS signal) 及其不确定性
(variance/ivar)，输出标准 IVOA HiPS 产品与平面 FITS。V6.1 返工将 V6 遗留缺陷
逐任务闭环（R0-001..REL-003），建立唯一生产路径并逐 Gate 验收。

## 2. 顶层文档 (点击审阅)

| 文档 | 内容 |
|---|---|
| [SCIENCE_OVERVIEW](docs/review/SCIENCE_OVERVIEW.md) | 核心科学定义、公式、假设、不变量 (SCI-001..) |
| [PIPELINE_OVERVIEW](docs/review/PIPELINE_OVERVIEW.md) | Phase1/2/3 数据流与模块关系 |
| [ARCHITECTURE_OVERVIEW](docs/review/ARCHITECTURE_OVERVIEW.md) | CLI、I/O、模块、计算后端、退出路径 |
| [RELEASE_STATUS](docs/review/RELEASE_STATUS.md) | 通过项、未验证项、发布限制 |
| [CHANGE_REVIEW](docs/review/CHANGE_REVIEW.md) | 本轮变化、科学影响、测试证据、待决策事项 |

## 3. 当前进度 (Gate G3..G10)

- 已过门: **G3/G4/G5/G6/G7**（证据 `evidence/v6_1_rework/gates/*/CHECKLIST.md`）。
- 进行中: G8 Linux 常在线控制节点。
- 任务状态唯一源: `evidence/v6_1_rework/TASK_LEDGER.csv`（逐任务
  `evidence/v6_1_rework/tasks/<TASK-ID>/TASK_RESULT.json` 含 parent_commit 与
  证据 hash；COMMITS.csv 逐任务原子提交链）。

## 4. 组件状态（DOC-005 逐项标注）

| 组件 | 状态 | 证据 |
|---|---|---|
| Phase1（校准/星点/PSF/测光/写） | IMPLEMENTED | P1-001..P1-006 PASS |
| Phase2（coverage/UPM/reject/integrate 7 节点 IR） | IMPLEMENTED | P2-001..P2-007 PASS |
| Phase3（properties/WCS/并行采样/FITS/verify 5 节点 IR） | IMPLEMENTED | P3-001..P3-006 PASS |
| CPU Provider（纯 CPU 自适应，无 ACR） | IMPLEMENTED | CPU-001..004 PASS |
| ACR（硬件加速器） | DORMANT | 不注册不链接，编译入口 0 |
| GUI（未来 Windows 可视化） | NOT_INCLUDED | 不在本包范围 |
| 真实数据验证 / 32R / Windows | EXPERIMENTAL | WIN-001..006 WAITING_WINDOWS（Fatduck 离线），LNX-005 离线登记 |

## 5. 关键结论

- 科学定义 = 算法 = 接口 = 代码 = 测试（六层追溯, TRACE-001）。
- 生产仅纯 CPU; ACR dormant; 旧 Orchestrator/PipelineEngine 调度/Stage2/drizzle
  直连全部退出。
- 产品版本单源 0.11.0-alpha.1（根 VERSION, GOV-003）; CLI `--version`
  输出 `0.11.0-alpha.1+g<commit>`（生成链见 docs/governance/VERSION_NAMESPACES.md）。
- 每个结论回溯 `evidence/v6_1_rework/tasks/*/TASK_RESULT.json` 自引用 hash 与
  COMMITS.csv 提交链。
- 最终发布需负责人审核（REL-001..004 全部 PASS 后由 Owner 批准，
  本 Agent 只声明 `READY_FOR_OWNER_REVIEW`，禁止代替批准）。
