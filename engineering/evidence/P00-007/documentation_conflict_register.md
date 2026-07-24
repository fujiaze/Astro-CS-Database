# AstroCS 文档冲突登记报告

- **Task ID**: P00-007
- **生成日期**: 2026-07-24
- **扫描范围**: `docs/` + `engineering/` + 根 `memory.md` + `lib/*/memory.md`
- **配套文件**: `documentation_conflict_register.json`

---

## 1. 统计表

| 项目 | 数量 |
|---|---|
| 冲突总数 | 10 |
| 覆盖主题数 | 8 |
| 高严重度 | 3 |
| 中严重度 | 4 |
| 低严重度 | 3 |

### 1.1 按主题分布

| 主题 | 冲突 ID | 严重度 | 冲突数 |
|---|---|---|---|
| monorepo vs 多仓库治理 | C-001, C-010 | high / low | 2 |
| Stage 编号体系 | C-002 | high | 1 |
| SNR 块定义（稠密 snr vs 稀疏 snr_model） | C-003 | high | 1 |
| Stack 节点模型 | C-004 | medium | 1 |
| healpix_io 合并 | C-005 | medium | 1 |
| data_pipeline 模块状态 | C-006 | medium | 1 |
| 已修 GAP 记录与代码现状不同步 | C-007 | medium | 1 |
| 模块状态（integration_test） | C-008 | low | 1 |
| psf 块字段数 [N,6] vs [N,9] | C-009 | low | 1 |

### 1.2 与待决策 ADR 的关联

| ADR | 状态 | 影响的冲突 |
|---|---|---|
| ADR-001 Drizzle/Stack 源码纳管 | PENDING | C-001 模块仓库列 |
| ADR-002 PipelineFrame 唯一所有者 | PENDING | C-006 data_pipeline 归属 |
| ADR-003 Stage 2 节点模型 | PENDING | C-004 Stack 节点 |

---

## 2. 冲突详情（按主题分组）

### 2.1 monorepo vs 多仓库治理

#### C-001【high】多文档仍写"独立仓库 / 根目录非 git 仓库"，与已完成的 monorepo 合并相矛盾

**冲突描述**

2026-07-24 已将 11 个子仓库历史合并到主仓库 Astro-CS-Database，主仓库 `.git` 直接落在项目根目录。但多份核心架构文档、根 memory.md 旧表述及历史 spec 仍写"分模块独立仓库""根目录非 git 仓库""各模块独立 git 管理"，且模块清单仍把 healpix_stack/healpix_drizzle/snr_estimator/orchestrator 等标注为"独立仓库"。memory.md 自身存在自相矛盾（同一文件第 9-12 行记录合并、第 85/148 行仍写非 git 仓库）。

**来源**

| 文档 | 行号 | 表述 |
|---|---|---|
| memory.md | 9 | 2026-07-24 11 个子仓库合并到主仓库 Astro-CS-Database ★工程整合★ |
| memory.md | 12 | 主仓库 .git 直接落在项目根目录 |
| memory.md | 69 | lib/orchestrator/ (独立仓库) - 管线编排引擎模块（v2.0 两段流水线 10 节点...） |
| memory.md | 85 | 根目录非 git 仓库，各模块独立 git 管理 |
| memory.md | 148 | 根目录非 git 仓库 (符合架构设计) |
| engineering/02_BASELINE_AUDIT.md | 22 | 实际导出基线已经在 2026-07-24 将 11 个模块历史合并到一个主仓库...旧表述必须归档或修正 |
| engineering/05_REPOSITORY_AND_DEPENDENCY_MANAGEMENT.md | 9 | 当前基线已是 monorepo |
| docs/ARCHITECTURE.md | 18 | 分模块独立仓库：每个核心模块独立 GitHub 仓库...根目录非 git 仓库，各模块独立 git 管理 |
| docs/ARCHITECTURE.md | 65 | dynamic_psf | 待建立独立仓库 |
| docs/ARCHITECTURE.md | 68 | healpix_drizzle | 活跃（独立仓库，.gitignore 忽略） |
| docs/ARCHITECTURE.md | 234 | 各模块独立 git 仓库，根目录非 git 仓库 |
| docs/PROJECT_OVERVIEW.md | 25 | 分模块独立仓库 | 每个核心模块独立 GitHub 仓库...根目录非 git 仓库 |
| docs/PROJECT_OVERVIEW.md | 217 | 各模块独立 git 仓库，根目录非 git 仓库 |
| docs/superpowers/specs/2026-07-16-project-reorganization-checklist.md | 10 | 根目录非 git 仓库，各模块独立 git 仓库。回滚点 = 各模块当前 commit hash |
| docs/superpowers/specs/2026-07-16-project-reorganization.md | 15 | 与当前两段流水线 10 节点架构、step4 C++化、各模块独立仓库拆分等实施现状存在偏差 |

**建议修正方向**

以 `memory.md:9-12` 和 `engineering/05/02` 为准（monorepo，主仓库 Astro-CS-Database 在项目根目录）。需修正：
- `docs/ARCHITECTURE.md` §1 核心设计原则与 §8 开发环境
- `docs/PROJECT_OVERVIEW.md` §2 与 §11
- `memory.md:85/148/69` 等旧表述
- `lib/*/README` 与模块清单的"独立仓库"标注

历史 spec（2026-07-16-project-reorganization*）建议加归档头注，不就地改写历史决策。

---

#### C-010【low】模块清单 GitHub 仓库列仍当作活跃仓库

**冲突描述**

memory.md:9-26 记录 2026-07-24 合并后原 11 个子仓库"封存为历史快照，README 注明依赖指向主仓库"。但 docs/ARCHITECTURE.md / PROJECT_OVERVIEW.md 模块清单仍把各模块的独立 GitHub 仓库当作活跃仓库列出，lib/healpix_db/README.md 与 lib/data_pipeline/README.md 仍标"独立仓库"。

**来源**

| 文档 | 行号 | 表述 |
|---|---|---|
| memory.md | 22 | 已封存子仓库（保留为历史快照，README 注明依赖指向主仓库 Astro-CS-Database，不再独立开发） |
| docs/ARCHITECTURE.md | 62 | astro_image_io | Astro-Image-IO-C |
| docs/ARCHITECTURE.md | 63 | calibration | Astro-Calibration-Cpp |
| docs/ARCHITECTURE.md | 68 | healpix_drizzle | Healpix-Drizzle-Cpp | 活跃（独立仓库，.gitignore 忽略） |
| docs/ARCHITECTURE.md | 69 | healpix_stack | Healpix-Mosaic-Cpp | 活跃（独立仓库，.gitignore 忽略） |
| docs/PROJECT_OVERVIEW.md | 100 | astro_image_io | Astro-Image-IO-C |
| docs/PROJECT_OVERVIEW.md | 107 | healpix_drizzle | Healpix-Drizzle-Cpp |
| lib/healpix_db/README.md | 18 | healpix_stack/ | ...活跃（独立仓库） |
| lib/healpix_db/README.md | 19 | healpix_drizzle/ | ...活跃（独立仓库） |
| lib/data_pipeline/README.md | 108 | v1.0 (2026-07-12): 从astro_image_io迁移C++实现，新建独立仓库 |

**建议修正方向**

以 monorepo 为准（主仓库 Astro-CS-Database）。模块清单 GitHub 仓库列改为"历史仓库（已封存，指向主仓库 Astro-CS-Database）"或删除该列。`lib/*/README.md` 的"独立仓库"表述同步修正。属 C-001 的延伸，随 C-001 一并处理。

---

### 2.2 Stage 编号体系

#### C-002【high】4 套阶段计数并存：9 节点 / 10 节点 / 7 节点 / 5 阶段

**冲突描述**

同一项目同时存在 4 套阶段计数：
- **9 节点**（归档 GRADIENT_2D 后）：stage1 0-6 + stage2 7-8
- **10 节点**（含 GRADIENT_2D 的旧版 PipelineStageV2）
- **7 节点**（stage1 单段表述）
- **5 阶段**（aio_pipeline.h 的 PipelineStage 枚举：CALIBRATE/PLATESOLVE/PHOTOMETRIC/DRIZZLE/STACK）

GAP-021 已归档 GRADIENT_2D 并重排为 9 节点，但 orchestrator 模块描述、多份 memory 记录、历史 spec 仍写 10 节点；aio_pipeline.h 暴露的 PipelineStage 枚举仅 5 阶段，无法表达 READ_FITS/PSF/SNR/GRADIENT_SPHERE。

**来源**

| 文档 | 行号 | 表述 |
|---|---|---|
| docs/ARCHITECTURE.md | 19 | 第一段单帧预处理（FITS→.hiss, stage 0-6...7 节点）+ 第二段多帧合并（.hiss→.hcsd, stage 7-8） |
| docs/ARCHITECTURE.md | 31 | 两段流水线 9 节点架构（2026-07-16，2026-07-18 归档 GRADIENT_2D） |
| docs/ARCHITECTURE.md | 73 | orchestrator | 管线编排引擎（两段流水线 10 节点 C++ CLI + Python 调试层） |
| docs/PROJECT_OVERVIEW.md | 26 | 两段流水线 | stage1（FITS→.hiss，7 节点）+ stage2（.hiss→.hcsd，2 节点） |
| docs/PROJECT_OVERVIEW.md | 31 | 两段流水线 9 节点（修复后实际状态） |
| docs/PROJECT_OVERVIEW.md | 112 | orchestrator | 两段流水线编排引擎（10 节点 C++ CLI + Python 调试层） |
| docs/PIPELINE_OVERVIEW.md | 21 | stage1 共 7 节点：READ_FITS/CALIBRATE/PLATESOLVE/PSF/PHOTOMETRIC/SNR/DRIZZLE |
| docs/DESIGN_IMPL_GAP.md | 256 | PipelineStageV2...第一阶段 8 个节点...第二阶段 2 个节点...共 10 节点 |
| docs/DESIGN_IMPL_GAP.md | 285 | orchestrator.h PipelineStageV2 枚举确认 10 节点 |
| memory.md | 69 | v2.0 两段流水线 10 节点 C++ CLI |
| memory.md | 852 | PipelineStage 枚举: CALIBRATE / PLATESOLVE / PHOTOMETRIC / DRIZZLE / STACK |
| memory.md | 1057 | 端到端: 9/9 节点成功 |
| memory.md | 1091 | orchestrator.h: PipelineStageV2 枚举 (10 节点) |
| memory.md | 1128 | stage1 8/8 节点全部实际 DLL 调用 |
| docs/superpowers/specs/2026-07-18-audit-findings-P0P1.md | 56 | B1-C-1: 管线阶段枚举仅 5 个，与 9 节点架构不一致 |
| docs/superpowers/specs/2026-07-18-audit-findings-P2.md | 851 | 检查点模块判定管线是否全部完成的阈值硬编码为 4，基于旧版 5 阶段（0-4）。但新版是 9 节点 |
| docs/superpowers/specs/2026-07-16-project-reorganization.md | 96 | 两段流水线 10 节点架构（stage1: 0-7, stage2: 8-9） |

**建议修正方向**

以 GAP-021 归档后的 9 节点为准：
- stage1: READ_FITS/CALIBRATE/PLATESOLVE/PSF/PHOTOMETRIC/SNR/DRIZZLE（共 7 节点）
- stage2: GRADIENT_SPHERE/STACK（共 2 节点）

需修正：
- `docs/ARCHITECTURE.md:73` 与 `docs/PROJECT_OVERVIEW.md:112` orchestrator 模块描述"10 节点"→"9 节点"
- `memory.md:69/1091` 旧记录加历史标注

`PipelineStage` 枚举（5 阶段）vs `PipelineStageV2`（9 节点）的二选一需 ADR 决策（见 audit B1-C-1），暂列为 P03 接口契约范畴。

---

### 2.3 SNR 块定义

#### C-003【high】稠密 snr (FLOAT32[H,W]) vs 稀疏 snr_model (RAW) 描述不一致

**冲突描述**

ARCHITECTURE.md §4.1 数据流表仍写 SNR 阶段输出 `snr (FLOAT32[H,W])` 稠密图像块，但 PROJECT_OVERVIEW.md、03_TARGET_SYSTEM_ARCHITECTURE.md、06_DATA_MANAGEMENT_SPEC.md 均确认实际为 `snr_model`（RAW，稀疏控制点序列化）。DESIGN_IMPL_GAP.md GAP-011 仍标"待修复"，但 audit-findings-P0P1.md 指出代码已修复、文档与代码状态不同步。02_BASELINE_AUDIT.md 将此列为"数据格式漂移"高风险。

**来源**

| 文档 | 行号 | 表述 |
|---|---|---|
| docs/ARCHITECTURE.md | 113 | SNR | data, psf, photo_stats | snr (FLOAT32[H,W]) |
| docs/PROJECT_OVERVIEW.md | 62 | SNR → snr_model(RAW: 稀疏控制点序列化) [修复后: 不再写 snr 稠密块] |
| docs/PROJECT_OVERVIEW.md | 68 | snr_model 块序列化格式（与 hp_drizzle_api.cpp 期望一致） |
| engineering/03_TARGET_SYSTEM_ARCHITECTURE.md | 84 | snr_model | RAW/typed | SNR | DRIZZLE/HISS | 稀疏球面控制点模型 |
| engineering/06_DATA_MANAGEMENT_SPEC.md | 78 | 标准块表仍提到稠密 snr，实际管线改为 snr_model |
| engineering/02_BASELINE_AUDIT.md | 93 | 数据格式漂移：HISS 已出现逐像素 SNR 与稀疏 SNR 两种格式，缺强制兼容矩阵 |
| docs/DESIGN_IMPL_GAP.md | 103 | GAP-011：SNR 接口链路断裂...（状态字段无 CLOSED 标记） |
| docs/DESIGN_IMPL_GAP.md | 116 | snr_estimate...输出稠密 SNR 图...写入 snr 命名块 |
| docs/DESIGN_IMPL_GAP.md | 118 | snr 块：稠密 SNR 图，类型为像素块（FLOAT32 数组） |
| docs/superpowers/specs/2026-07-18-audit-findings-P0P1.md | 636 | GAP-011...状态仍标为待修复，但实际代码已经修复（snr_model 块替代 snr 浮点图像）。文档与代码状态不同步 |
| lib/orchestrator/memory.md | 505 | run_stage_snr 调用旧版 snr_estimate（输出稠密 SNR 图，写 snr 块 FLOAT32[H,W]） |
| lib/orchestrator/memory.md | 506 | 但 drizzle 阶段 hp_drizzle_run 只识别 snr_model 块（稀疏控制点 AIO_BLOCK_RAW） |

**建议修正方向**

以 `snr_model`（RAW，稀疏控制点序列化）为准。需修正：
- `docs/ARCHITECTURE.md` §4.1 数据流表 `snr (FLOAT32[H,W])` → `snr_model (RAW: 稀疏控制点序列化)`
- 同步 `DESIGN_IMPL_GAP.md` GAP-011 状态为 CLOSED 并附代码证据（与 P00-006 旧审计复核协同）
- HISS 格式兼容矩阵（snr_format=0/1）由 P02 数据契约冻结

---

### 2.4 Stack 节点模型

#### C-004【medium】STACK 节点：文档描述为独立节点，实际为空骨架

**冲突描述**

ARCHITECTURE.md §2 第二阶段表格把 STACK 描述为独立节点（Winsorized sigma clip + SNR²加权叠加 → .hcsd），但 PROJECT_OVERVIEW.md 与 DESIGN_IMPL_GAP.md GAP-015 确认 STACK 是空骨架（.hcsd 实际由 GRADIENT_SPHERE 的 `hp_stack_gradient_corrected` 单函数生成）。engineering/12 要求 P06 选择 A/B/C 方案，ADR-003 仍 PENDING。

**来源**

| 文档 | 行号 | 表述 |
|---|---|---|
| docs/ARCHITECTURE.md | 52 | 8 | STACK | healpix_stack | Winsorized sigma clip + SNR²加权叠加，输出 .hcsd |
| docs/PROJECT_OVERVIEW.md | 50 | STACK | healpix_stack | Winsorized sigma clip...当前为空骨架（.hcsd 已由 stage 7 生成） |
| docs/PIPELINE_OVERVIEW.md | 34 | 12. SNR² 加权叠加 | healpix_stack | 高 SNR 数据更高权重 → .hcsd 天球数据库 |
| docs/DESIGN_IMPL_GAP.md | 287 | GAP-015：stage2 4 步合并为 1 函数 + STACK 空骨架 |
| docs/DESIGN_IMPL_GAP.md | 313 | 第二阶段实际只有 1 个有效节点（GRADIENT_SPHERE），STACK 是占位 |
| engineering/12_STAGE2_STACKING_VALIDATION_SPEC.md | 111 | 当前代码把完整输出放入 GRADIENT_SPHERE，STACK 仅返回成功。P06 必须选择并记录 A/B/C |
| engineering/12_STAGE2_STACKING_VALIDATION_SPEC.md | 117 | 不允许继续保留空节点成功而文档声称执行了算法 |
| engineering/control/DECISION_LOG.md | 8 | ADR-003 | PENDING | Stage 2 节点模型 | 单节点、双节点或内部子阶段 |

**建议修正方向**

待 ADR-003 决策（单节点/双节点/内部子阶段）。当前过渡表述以"STACK 空骨架，实际由 GRADIENT_SPHERE 单函数完成 4 个内部步骤"为准：
- 修正 `docs/ARCHITECTURE.md` §2 第二阶段表格的 STACK 行说明，避免暗示 STACK 独立执行叠加
- `PIPELINE_OVERVIEW.md` §第二阶段 4 步表格应标注"4 个内部步骤，非 4 个独立节点"

---

### 2.5 healpix_io 合并

#### C-005【medium】healpix_io 已归档并入 astro_image_io，但活跃文档仍引用独立 DLL

**冲突描述**

2026-07-16 healpix_io 已归档，API 并入 astro_image_io（`aio_healpix_io.h` 兼容宏，DLL 名 `healpix_io.dll` → `astro_image_io.dll`）。ARCHITECTURE.md 与 DESIGN_IMPL_GAP.md GAP-002 已标"已修复/已归档"。但根 memory.md 的历史开发日志、`马赛克叠加梯度建模计划.md`（活跃文档）、多份 2026-07-13~15 历史 spec 仍把 healpix_io 当作独立 DLL 引用（hiss_write/hiss_read 通过 healpix_io.dll 调用），易误导新开发者编译依赖。

**来源**

| 文档 | 行号 | 表述 |
|---|---|---|
| docs/ARCHITECTURE.md | 70 | healpix_io | .hiss / .hcsd 存储格式读写（已归档，API 并入 astro_image_io）| 已归档（2026-07-16） |
| docs/ARCHITECTURE.md | 98 | lib/healpix_db/healpix_io/archive/：healpix_io 源码归档（API 已并入 astro_image_io） |
| docs/DESIGN_IMPL_GAP.md | 24 | GAP-002：healpix_io 实际代码已移至 archive/ 但仍标为活跃 —— 已修复 |
| memory.md | 464 | 将 healpix_drizzle 模块的输出格式从 .ahpx 改为 .hiss，通过 healpix_io.dll 的 hiss_write 写入 |
| memory.md | 479 | DLL 依赖: astro_image_io.dll + healpix_io.dll + libgomp-1.dll |
| memory.md | 521 | healpix_io C++ DLL 实现（Task 2，2026-07-13） |
| memory.md | 1043 | 断层2 (healpix_io.h/.cpp): hiss 格式 has_snr 字段实现 |
| 马赛克叠加梯度建模计划.md | 5 | 复用 healpix_io 的 DLL 接口读写 .hiss/.hcsd |
| 马赛克叠加梯度建模计划.md | 68 | healpix_io 提供 .hiss/.hcsd 读写 DLL 接口，复用 |
| 马赛克叠加梯度建模计划.md | 337 | 复用 healpix_io DLL 接口 |
| docs/superpowers/specs/2026-07-15-snr-module-and-grad-design.md | 222 | healpix_io.h/.cpp hiss_write/read | FORMAT_SPEC 已设计 has_snr 字段，但 C API/Python 绑定未实现 |
| docs/superpowers/specs/2026-07-13-cpp-qt-browser-core-design.md | 99 | healpix_io.dll（现有，不修改） |

**建议修正方向**

以 healpix_io 已归档、API 并入 astro_image_io（`aio_healpix_io.h`）为准。需修正：
- `马赛克叠加梯度建模计划.md`（活跃文档）中 healpix_io DLL 引用 → astro_image_io DLL
- `memory.md` 历史日志在 2026-07-16 条目后加"⚠ 已归档，见 healpix_io 合并"标注
- 历史 spec（2026-07-13~15）加归档头注，不就地改写

---

### 2.6 data_pipeline 模块状态

#### C-006【medium】data_pipeline 标"活跃"但无独立构建，与 astro_image_io 重复

**冲突描述**

ARCHITECTURE.md / PROJECT_OVERVIEW.md 把 data_pipeline 标为"活跃（拆分中间态）"，PIPELINE_OVERVIEW.md 核心设计原则写"数据总线：data_pipeline（PipelineFrame + PipelineEngine）"。但 engineering/03 明确"data_pipeline 不得继续作为第二份同名 ABI 长期存在，P02 必须决定"，ADR-002 PENDING，P00-004 依赖图确认 data_pipeline 无独立构建、源码与 astro_image_io 重复，orchestrator 实际加载 astro_image_io.dll 而非 data_pipeline.dll（GAP-019）。lib/data_pipeline/memory.md 与 README.md 仍称"独立仓库"。

**来源**

| 文档 | 行号 | 表述 |
|---|---|---|
| docs/ARCHITECTURE.md | 76 | data_pipeline | 数据总线...活跃（拆分中间态：astro_image_io 仍保留副本）| Astro-Data-Pipeline |
| docs/PROJECT_OVERVIEW.md | 113 | data_pipeline | 数据总线（拆分中间态，与 astro_image_io 同 API，待 GAP-006 处理）| Astro-Data-Pipeline |
| docs/PIPELINE_OVERVIEW.md | 57 | 数据总线：data_pipeline（PipelineFrame + PipelineEngine）提供命名块容器在内存中传递 |
| engineering/03_TARGET_SYSTEM_ARCHITECTURE.md | 42 | data_pipeline 不得继续作为第二份同名 ABI 长期存在。P02 必须决定：合并、废弃或改名 |
| engineering/03_TARGET_SYSTEM_ARCHITECTURE.md | 118 | data_pipeline 与 astro_image_io 谁拥有 PipelineFrame（ADR 门禁） |
| engineering/control/DECISION_LOG.md | 7 | ADR-002 | PENDING | PipelineFrame 唯一所有者 | astro_image_io 或 data_pipeline |
| engineering/evidence/P00-004/dependency_graph.md | 127 | data_pipeline 模块无独立构建文件, 其源文件与 astro_image_io/src 下同名文件重复, 存在维护一致性问题 |
| docs/DESIGN_IMPL_GAP.md | 80 | GAP-006：data_pipeline 从 astro_image_io 拆分未完成 —— 待修复 |
| docs/DESIGN_IMPL_GAP.md | 489 | PIPELINE_OVERVIEW.md 核心设计原则中数据总线：data_pipeline...与实际不符 |
| lib/data_pipeline/memory.md | 23 | 从astro_image_io迁移独立：原管线实现位于astro_image_io，为职责单一拆分为独立仓库 |
| lib/data_pipeline/README.md | 108 | v1.0 (2026-07-12): 从astro_image_io迁移C++实现，新建独立仓库 |

**建议修正方向**

待 ADR-002 决策 PipelineFrame 唯一所有者。当前 `docs/PIPELINE_OVERVIEW.md:57` "数据总线：data_pipeline"与实际（orchestrator 加载 astro_image_io.dll）不符，应至少标注"实际由 astro_image_io 提供，data_pipeline 为拆分中间态"。`lib/data_pipeline/memory.md:23` 与 `README.md:108` 的"独立仓库"表述需按 C-001 monorepo 决策同步修正。

---

### 2.7 已修 GAP 记录与代码现状不同步

#### C-007【medium】GAP-011/012/013/016/017 仍标"待修复"但 PROJECT_OVERVIEW 称已修复

**冲突描述**

DESIGN_IMPL_GAP.md 中 GAP-011/012/013/016/017 等多项仍标"待修复"或无 CLOSED 标记，但 PROJECT_OVERVIEW.md（标称"修复后状态"）描述这些功能已实现（snr_model 块、CCD QE 积分、IRLS+Tukey、NSIDE 自适应、Winsorized sigma clip）。02_BASELINE_AUDIT.md 明确"旧审计不能直接当当前事实，必须在 P00 重新逐项复核"。audit-findings-P0P1.md 直接指出 GAP-011 文档与代码状态不同步。

**来源**

| 文档 | 行号 | 表述 |
|---|---|---|
| docs/DESIGN_IMPL_GAP.md | 103 | GAP-011：SNR 接口链路断裂...（无 CLOSED 标记） |
| docs/DESIGN_IMPL_GAP.md | 149 | GAP-012：CCD QE 曲线未使用...（无 CLOSED 标记） |
| docs/DESIGN_IMPL_GAP.md | 196 | GAP-013：photometric_calib C API 是简化版...（无 CLOSED 标记） |
| docs/DESIGN_IMPL_GAP.md | 332 | GAP-016：NSIDE 自适应未实现...（无 CLOSED 标记） |
| docs/DESIGN_IMPL_GAP.md | 376 | GAP-017：Winsorized sigma clip 未实现...（无 CLOSED 标记） |
| docs/PROJECT_OVERVIEW.md | 3 | 创建日期：2026-07-17（GAP-011/012/013/016/017 修复后状态） |
| docs/PROJECT_OVERVIEW.md | 41 | PHOTOMETRIC | F_syn = ∫ S(λ)·T(λ)·Q(λ) dλ（含 CCD QE）+ IRLS+Tukey biweight 稳健回归 |
| docs/PROJECT_OVERVIEW.md | 43 | DRIZZLE | NSIDE 自适应...+ 读 snr_model 块用 SnrEvaluator 重建逐像素 SNR |
| docs/PROJECT_OVERVIEW.md | 50 | STACK | Winsorized sigma clip（缩尾 5%/95% 分位数 + 稳健均值/标准差） |
| docs/superpowers/specs/2026-07-18-audit-findings-P0P1.md | 636 | GAP-011...状态仍标为待修复，但实际代码已经修复...文档与代码状态不同步 |
| engineering/02_BASELINE_AUDIT.md | 72 | 旧审计不能直接当当前事实，必须在 P00 重新逐项复核—关闭—保留—降级 |

**建议修正方向**

与 P00-006（旧审计复核）协同，逐项复核 GAP-011~020 的代码现状，在 DESIGN_IMPL_GAP.md 每项补 OPEN/CLOSED/STALE/UNVERIFIED 状态字段并附代码证据（commit/文件:行）。在 P00-007 范围内仅登记冲突，不就地改 GAP 状态。

---

### 2.8 模块状态（integration_test）

#### C-008【low】integration_test 历史日志写"进行中"易误导

**冲突描述**

ARCHITECTURE.md 与 memory.md 一致标 integration_test 已归档（orchestrator/archive/scripts/ 有副本），无实质冲突。但 memory.md 历史开发日志仍详细记录 integration_test 的 Task 1-8 进度（"进行中"措辞），易让新开发者误以为该模块仍在活跃开发。

**来源**

| 文档 | 行号 | 表述 |
|---|---|---|
| docs/ARCHITECTURE.md | 77 | integration_test | 全链路整合测试（已归档）| 已归档（orchestrator/archive/scripts/ 有完整副本） |
| memory.md | 71 | lib/integration_test/ - 全链路整合测试 (已删除，orchestrator/archive/scripts/ 有完整副本) |
| memory.md | 882 | integration_test 全链路整合测试（进行中） |
| memory.md | 891 | 模块记忆: lib/integration_test/memory.md（该路径已不存在） |

**建议修正方向**

`memory.md:882-891` 的"integration_test 全链路整合测试（进行中）"历史条目加归档标注（如"⚠ 已归档至 orchestrator/archive/scripts/，本条为历史记录"）。无实质冲突，仅需历史记录清理。

---

### 2.9 psf 块字段数

#### C-009【low】psf 块 [N,6] vs [N,9] 旧记录未清理

**冲突描述**

psf 块已从 [N,6] 扩展到 [N,9]（新增 A/mad/eccentricity），ARCHITECTURE.md/PROJECT_OVERVIEW.md 已同步。但 memory.md 旧记录仍写 [N,6]，engineering/06 明确把此列为"当前必须复核的冲突"。

**来源**

| 文档 | 行号 | 表述 |
|---|---|---|
| docs/ARCHITECTURE.md | 111 | psf (FLOAT64[N,9])：x,y,fwhm,背景,A,mad,eccentricity 等 |
| docs/PROJECT_OVERVIEW.md | 60 | psf(FLOAT64[N,9]: status,B,flux,cx,cy,fwhm,A,mad,eccentricity) |
| engineering/06_DATA_MANAGEMENT_SPEC.md | 77 | psf 注释中曾出现 [6]、[N,9] |
| memory.md | 799 | psf 块（FLOAT64[N,6]: status, B, flux, cx, cy, fwhm均值） |
| memory.md | 166 | PSF 块扩展(已完成): psf_adapter.py: [N,6]→[N,9],新增 A/mad/eccentricity 三列 |

**建议修正方向**

以 [N,9] 为准。`memory.md:799` 旧记录加历史标注。`06_DATA_MANAGEMENT_SPEC.md:77` 已正确识别，P02 数据契约冻结时把 psf schema 固定为 [N,9] 并写入 `pipeline_blocks.schema.json`。

---

## 3. 建议修正方向汇总

### 3.1 优先级矩阵

| 优先级 | 冲突 ID | 修正动作 | 前置条件 |
|---|---|---|---|
| P1（立即） | C-001, C-010 | 统一为 monorepo 表述，修正 docs/ARCHITECTURE.md、docs/PROJECT_OVERVIEW.md、lib/*/README | 无 |
| P1（立即） | C-003 | ARCHITECTURE.md §4.1 数据流表 snr → snr_model | 与 P00-006 协同 |
| P1（立即） | C-002 | 统一为 9 节点表述，修正 orchestrator 模块描述"10 节点"→"9 节点" | 无 |
| P2（短期） | C-005 | 修正活跃文档 `马赛克叠加梯度建模计划.md` healpix_io 引用 | 无 |
| P2（短期） | C-007 | 与 P00-006 协同逐项复核 GAP-011~020 状态 | P00-006 |
| P2（短期） | C-009 | memory.md 旧记录加历史标注 | 无 |
| P3（待 ADR） | C-004 | 等 ADR-003 决策后修正 Stack 节点描述 | ADR-003 |
| P3（待 ADR） | C-006 | 等 ADR-002 决策后修正 data_pipeline 归属 | ADR-002 |
| P4（清理） | C-008 | memory.md 历史条目加归档标注 | 无 |

### 3.2 修正原则

1. **以代码和最新基线为准**：memory.md:9-12（合并记录）、engineering/02/05（基线审计）、engineering/03（目标架构）、PROJECT_OVERVIEW.md（修复后状态）为权威来源。
2. **历史 spec 不就地改写**：2026-07-13~16 的历史 spec 加归档头注（如"> ⚠ 本文档为历史决策记录，部分表述已被后续工程整合覆盖，详见 engineering/evidence/P00-007/"），保留历史决策可追溯性。
3. **活跃文档必须修正**：docs/ARCHITECTURE.md、docs/PROJECT_OVERVIEW.md、docs/PIPELINE_OVERVIEW.md、`马赛克叠加梯度建模计划.md`、lib/*/README.md 等当前仍被引用的文档必须就地修正。
4. **待 ADR 决策的不预先固化**：C-004（ADR-003）、C-006（ADR-002）在 ADR 决策前仅加过渡标注，不提前选定方案。
5. **与 P00-006 协同**：C-003、C-007 涉及 GAP 状态复核，由 P00-006 旧审计复核任务逐项确认代码现状后，再同步文档状态。

### 3.3 跨任务关联

| 关联任务 | 关联冲突 | 关联说明 |
|---|---|---|
| P00-002/P00-003 Drizzle/Stack 源码纳管 | C-001, C-004 | 影响 ADR-001，进而影响模块仓库列与 Stack 节点描述 |
| P00-004 依赖图 | C-006 | dependency_graph.md 已确认 data_pipeline 无独立构建 |
| P00-006 旧审计复核 | C-003, C-007 | GAP 状态复核的权威来源 |
| P02-001 数据契约 | C-003, C-009 | HISS 格式兼容矩阵、psf schema 冻结 |
| P03 接口契约 | C-002 | PipelineStage vs PipelineStageV2 枚举二选一 |
| P06 Stage 2 验证 | C-004 | Stack 节点模型 A/B/C 方案选择 |

---

## 4. 已检查无冲突的主题

无。8 个重点主题均发现至少 1 项冲突。

---

## 5. 备注

- 本报告仅登记冲突，不修改 `lib/**`、`docs/**`、构建脚本与算法配置（遵循 CURRENT_WORK.md 禁止修改清单）。
- 所有冲突的来源文档与行号已在上文逐项列出，便于后续修正任务定位。
- 配套 JSON 文件 `documentation_conflict_register.json` 提供机器可读格式，便于后续自动化跟踪。
