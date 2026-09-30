# ACSD 细节文档索引（docs/detail）

> 上游：docs/ACSD_DESIGN.md §0（文档权威与索引）、§8（软件架构）
> 本索引覆盖 `docs/detail/` 全树：根级跨模块正本、`registry/` 模块登记面、
> `infrastructure/` 基建模块落地面、`anchors/` 文档—代码锚合同面。

## 1. 目录结构

```text
docs/detail/
├── 00_INDEX.md                        本文
├── README.md                          目录说明
├── common.md                          跨链路共享基础库（lib/algorithms/shared/）
├── UNIFIED_MODEL.md                   统一线性观测模型与数据对象
├── PHASE1_DETAILED_DESIGN.md          normalize 阶段详细设计
├── PHASE2_DETAILED_DESIGN.md          mosaic 阶段详细设计
├── PHASE3_DETAILED_DESIGN.md          export 阶段详细设计
├── PRODUCT_STORAGE_FORM.md            裸 HiPS 与 zstd 归档包落盘形态
├── LOG_AND_ERROR_SYSTEM.md            日志与错误系统详细设计
├── STAR_DETECTION_IMPL_DESIGN.md      星检测逐算子落地规格
├── merged_TROUBLESHOOTING.md          排障手册（症状 → 定位 → 修复）
├── registry/                          生产模块登记正本（26 张卡 + README）
├── infrastructure/                    基建模块落地设计（8 张卡 + README）
└── anchors/                           文档—代码锚合同（ANCHOR_CONTRACT + README）
```

四个文件夹各配一个极简 README，说明该文件夹放什么。

## 2. 模块落位规则

| 面 | 承载对象 | 体例 |
|---|---|---|
| `registry/` | 每个**生产 DAG 节点模块**一页 | 职责与明确非职责、输入输出端口与 DATA/单位/坐标/invalid、公共头与核心符号生命周期、配置 schema、执行类与并行轴、内存与所有权、错误与取消、独立验证命令与容差、已知限制 |
| `infrastructure/` | 每个**工程基建组件**一页（`lib/infrastructure/**`），含生产不可达的隔离实验与未来可视化组件 | 同 `registry/` 体例 |
| 根级 | 跨模块的观测模型、阶段详细设计、落盘形态、日志错误系统、逐算子实现规格、排障手册，以及不属于生产 DAG 的跨链路共享库 | 按主题组织 |
| `anchors/` | 文档条款锚定到源码符号与内容锚的合同 | 合同正文 |

不在 `registry/` 登记面的对象：`common`（跨链路共享基础库，不是流水节点）、
`infrastructure/` 下的 `acr`（隔离实验，`p2_acr_block_eligible` 恒 false）与
`hips_browser`（未来可视化组件，不进产品 manifest）。

## 3. `registry/` 模块卡（26）

| 命令 | 模块卡 |
|---|---|
| normalize | `acsd.phase1.session`、`acsd.phase1.calibration`、`acsd.phase1.cosmetic`、`acsd.phase1.star-detection`、`acsd.phase1.star-psf`、`acsd.phase1.wcs-platesolve`、`acsd.phase1.photometry`、`acsd.phase1.noise-snr`、`acsd.phase1.drizzle`、`acsd.phase1.hips-writer`、`acsd.phase1.writer` |
| mosaic | `acsd.phase2.session`、`acsd.phase2.coverage`、`acsd.phase2.sample`、`acsd.phase2.upm-fit`、`acsd.phase2.upm-apply`、`acsd.phase2.reject`、`acsd.phase2.integrate`、`acsd.phase2.write`、`acsd.phase2.resample` |
| export | `acsd.phase3.properties`、`acsd.phase3.wcs`、`acsd.phase3.resample2`、`acsd.phase3.writer`、`acsd.phase3.verify` |

各卡的公共抬头面：上游条款（最高设计节号 + 一级正本 + 数据/API/算法正本）、
职责与明确非职责、端口表、落地口径、公共头与 symbol、配置 schema、
执行类与并行轴、内存与所有权、错误与取消、验证面、已知限制。

## 4. `infrastructure/` 基建卡（8）

| 卡 | 组件 | 物理位 |
|---|---|---|
| `17_aio.md` | FITS / XISF / HiPS / manifest 唯一 I/O 与原子提交 | `lib/infrastructure/aio/` |
| `18_cli.md` | 唯一命令行入口、预检、机器输出与退出码 | `lib/infrastructure/cli/` |
| `19_runtime.md` | `scheduler` + `pipeline`：typed DAG、线程预算、资源监控 | `lib/infrastructure/scheduler/`、`lib/infrastructure/pipeline/` |
| `20_benchmark.md` | CPU 机器画像生成与校验 | `lib/infrastructure/benchmark/` |
| `21_observability.md` | 结构化日志、事件流、运行图、资源门 | `lib/infrastructure/observability/` |
| `22_gaia_xpsd_client.md` | 本地星表解析与两级缓存（离线、零网络） | `lib/infrastructure/gaia_xpsd_client/` |
| `23_hips_browser.md` | 球面浏览器与显示变换（不进产品 manifest） | `lib/infrastructure/hips_browser/` |
| `acr.md` | 异构计算运行时（隔离实验，生产不可达） | `lib/infrastructure/acr/` |

## 5. 每册必须回答的问题

- 本模块的输入对象与输出对象是什么（用 `UNIFIED_MODEL.md` 的术语）？
- 本模块**不做什么**（边界，防止越界改科学）？
- 端口的 DATA 编号、单位、坐标系与 invalid 语义分别是什么？
- 执行类、并行轴与确定性口径是什么？worker 数从哪个预算对象来？
- 验收门是什么（可复跑命令 + 容差）？
- 配置哪些字段、默认值、单位、约束？

## 6. 与一级正本的关系

- 一级正本是 `docs/science/`（公式、常数、判据、算法推导）与 `docs/engineering/`
  （架构、行为合同、标准）；detail 只做展开，不另立口径。
- detail 引用一级已定稿的公式与结论编号，不重复推导；数值表与推导留在 science。
- 冲突时以更高一层为准；一级表述不足以支撑实现时回到一级补要点，不在 detail 另立结论。
