# ADR-005 PipelineFrame 唯一归属 astro_image_io

- Status: ACCEPTED
- Date: 2026-07-25
- Task: P01-001
- Supersedes: ADR-PENDING-001（待决策：PipelineFrame 最终归属 data_pipeline 还是 astro_image_io）
- Related: ADR-002（Orchestrator 是控制层、PipelineFrame 和算法模块的唯一连接中心）

## Context

### 问题陈述
项目同时存在两份 PipelineFrame 实现，导出完全相同的 C 符号，构成重复导出与潜在符号冲突：

1. `lib/astro_image_io/` —— 含 `include/aio_pipeline.h`、`include/aio_pipeline_engine.h`、`src/aio_pipeline.cpp`、`src/aio_pipeline_engine.cpp`，由 `Makefile` 编译进 `astro_image_io.dll`。
2. `lib/data_pipeline/` —— 含同名头文件与源文件，是 2026-07-12 "从 astro_image_io 迁移 C++ 实现，新建独立仓库"（见 `lib/data_pipeline/memory.md`）的产物，**无 Makefile/build.ps1，无构建产物，无任何消费者**。

`engineering/docs/01_BASELINE_AND_KNOWN_GAPS.md` 已确认：
> Orchestrator 当前使用 `astro_image_io` 内的 PipelineFrame 副本；独立 `data_pipeline` 模块仍处于拆分中间态。

### 事实基线（基于实际代码分析，commit 7b85ff3）

| 事实 | 证据 |
|------|------|
| 两份 `aio_pipeline.cpp` 文本内容完全相同 | `Compare-Object` 验证 IDENTICAL CONTENT |
| 两份 `aio_pipeline_engine.cpp` 文本内容完全相同 | `Compare-Object` 验证 IDENTICAL CONTENT |
| 两份 `aio_pipeline.h` / `aio_pipeline_engine.h` 完全相同 | 逐字节读取对比 |
| 两个 .cpp 导出 21 个同名 `AIO_EXPORT` 符号 | Grep `AIO_EXPORT` 验证 |
| `astro_image_io.dll` 已构建（2993875 字节） | `Get-ChildItem *.dll` |
| `data_pipeline` 无构建产物 | `Get-ChildItem *.dll` 返回空 |
| orchestrator 通过 `ModuleId::AIO` 加载 `astro_image_io.dll` | `dll_loader.cpp:38` |
| orchestrator 默认路径 `lib/astro_image_io/` | `dll_loader.cpp:54` |
| orchestrator.cpp 通过 `ModuleId::AIO` 调用 `aio_pipeline_frame_create`/`aio_frame_add_block`/`aio_frame_add_block_move` 等 | `orchestrator.cpp:728/1267/1585/1757/2093/2233/2536` |
| orchestrator.cpp 不引用 data_pipeline | Grep 验证仅引用 `astro_image_io.h` |
| `data_pipeline/.gitignore` 忽略 `*.dll` | 读取确认 |

### 约束

1. **最小改动原则**：不破坏现有可用的构建链（用户规则：Surgical changes）。
2. **当前 astro_image_io 的 PipelineFrame 已被 orchestrator.exe 实际使用且工作正常**（P00-003 旧 CLI 真实数据基线已通过）。
3. **PipelineFrame 是运行时数据通道，不是 I/O 概念**（`04_PIPELINEFRAME_CONTRACT_V1.md`）。
4. **02_CLI_CORE_ARCHITECTURE.md §4 依赖方向**：`CLI → Orchestrator → contracts/data_pipeline → astro_image_io`（目标架构，data_pipeline 在 astro_image_io 之上）。
5. **P01 阶段是合约冻结阶段，优先稳定性**（PROJECT_STATE.yaml: 进入 P01 合约冻结阶段）。
6. **Windows-only + PowerShell 7 环境**（用户规则）。

## Decision

**选择选项 A：PipelineFrame 唯一归属 `astro_image_io`；`data_pipeline` 模块源码归档，不再作为独立构建目标。**

具体含义：
- `astro_image_io` 是 PipelineFrame / PipelineEngine 的**唯一实现与唯一导出者**。
- `lib/data_pipeline/` 目录保留源码作为历史参考，但标记为 ARCHIVED（拆分中间态产物），不参与构建。
- orchestrator 继续从 `astro_image_io.dll` 解析所有 `aio_pipeline_frame_*` / `aio_frame_*` 符号，无需改动 `dll_loader.cpp`。
- 未来若要分离 PipelineFrame 到独立模块，需在 P02+ 阶段以新 ADR 重新决策并规划完整迁移。

## Alternatives

### 选项 B：PipelineFrame 归属 data_pipeline（astro_image_io 只负责 I/O）

- 优点：符合 02 架构文档预期的依赖方向；概念清晰（PipelineFrame 是运行时数据通道）。
- 缺点：
  - 破坏当前可用构建链：需从 astro_image_io 移除 aio_pipeline.* 源文件，让 data_pipeline 构建成 DLL。
  - data_pipeline 无构建文件（无 Makefile/build.ps1），需新建。
  - 需修改 orchestrator 的 `dll_loader.cpp`（改加载 `data_pipeline.dll`）。
  - 违反最小改动原则与"不破坏现有可用构建链"约束。
  - P01 合约冻结阶段做此重构风险过高。

### 选项 C：新建独立 pipeline_frame 模块（astro_image_io 和 data_pipeline 都依赖它）

- 优点：概念最清晰，长期可维护性最好。
- 缺点：
  - 最大改动：新建模块、新建构建文件、修改两个模块的依赖。
  - 违反最小改动原则。
  - 当前没有立即需要这种分离的紧迫性（PipelineFrame 消费者只有 orchestrator，且已稳定工作）。
  - P01 阶段不宜引入新模块边界。

### 选项对比

| 维度 | 选项 A（归属 aio） | 选项 B（归属 dp） | 选项 C（新建独立） |
|------|-------------------|-------------------|-------------------|
| 改动量 | 极小（归档源码） | 大（迁移+改 loader） | 最大（新建模块） |
| 破坏现有构建链 | 否 | 是 | 否（但新增依赖） |
| 符号冲突消除 | 是 | 是 | 是 |
| 符合 02 目标架构 | 部分（短期偏离） | 是 | 是 |
| P01 稳定性 | 最高 | 低 | 中 |
| 概念纯洁度 | 中 | 高 | 最高 |

## Consequences

### 正面
- 消除重复导出与潜在符号冲突风险：data_pipeline 不再参与构建，21 个同名符号只从 astro_image_io.dll 导出。
- 零代码改动：不修改任何业务源码，不破坏 orchestrator → astro_image_io.dll 的现有加载链。
- 符合 P01 合约冻结阶段的稳定性优先要求。
- orchestrator 无需任何改动，现有 P00-003 基线（stage1 45s/stage2 7s）保持有效。

### 负面
- 与 02_CLI_CORE_ARCHITECTURE.md §4 依赖方向图（`contracts/data_pipeline → astro_image_io`）短期偏离：当前 PipelineFrame 实际由底层 I/O 模块 astro_image_io 提供，而非上层 data_pipeline。
- PipelineFrame 概念上属于运行时数据通道，留在 I/O 模块内职责略不纯。
- data_pipeline 仓库（GitHub: fujiaze/Astro-Data-Pipeline）与本地归档状态需同步说明。

### 中和措施
- 本 ADR 明确记录"短期偏离"原因与条件，02 文档的依赖方向图作为**目标态**保留。
- 未来架构重构（选项 B/C）作为 P02+ 候选，需以新 ADR 取代本决策。
- `lib/data_pipeline/` 添加 ARCHIVED 标记，避免后续 Agent 误将其作为活跃模块构建。

## Compatibility/migration

### 当前兼容性
- **完全兼容**：orchestrator.exe 继续从 astro_image_io.dll 加载所有 PipelineFrame 符号，行为不变。
- **契约兼容**：`04_PIPELINEFRAME_CONTRACT_V1.md` 的所有 API（aio_frame_add_block 复制、aio_frame_add_block_move 所有权移交）不变。
- **块注册表兼容**：`pipeline_blocks_registry.csv` 无需改动（块定义与归属模块无关，producer 仍是各 stage 模块）。

### 迁移路径（未来，不在本任务实施）
若后续选择选项 B/C：
1. 在 data_pipeline 或新模块建立完整构建文件（Makefile/build.ps1）。
2. 将 aio_pipeline.cpp/aio_pipeline_engine.cpp 迁移至目标模块。
3. 从 astro_image_io 移除（或改为转发）这些源文件。
4. 修改 orchestrator 的 dll_loader.cpp 加载目标 DLL。
5. 全量回归 P00-003 基线（stage1/stage2 真实数据）。
6. 更新 02 依赖方向图与本 ADR 状态（SUPERSEDED）。

## Evidence

| 证据 | 来源 |
|------|------|
| 两份 aio_pipeline.cpp 内容完全相同 | `Compare-Object` 验证，见 TEST_REPORT T-001 |
| 两份 aio_pipeline_engine.cpp 内容完全相同 | `Compare-Object` 验证，见 TEST_REPORT T-002 |
| 21 个同名 AIO_EXPORT 符号 | Grep `AIO_EXPORT` 验证，见 pipelineframe_ownership_analysis.json |
| orchestrator 加载 astro_image_io.dll | `dll_loader.cpp:38` |
| orchestrator 通过 ModuleId::AIO 调用 PipelineFrame API | `orchestrator.cpp:728/1267/1585/1757/2093/2233/2536` |
| astro_image_io.dll 已构建（2993875 字节） | 文件系统 |
| data_pipeline 无构建产物 | 文件系统 |
| data_pipeline 是迁移中间态 | `lib/data_pipeline/memory.md` 2026-07-12 条目 |
| 基线文档确认现状 | `01_BASELINE_AND_KNOWN_GAPS.md` §1 |
| 架构依赖方向 | `02_CLI_CORE_ARCHITECTURE.md` §4 |
