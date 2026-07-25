# TASK_REPORT: P01-001 PipelineFrame 唯一归属决策 (ADR-005)

- Task ID: P01-001
- Phase: P01 (合约冻结阶段)
- Commit/base: 7b85ff3f0d37a4b26fff6077684993842ed2bbae (P01-002 提交后 HEAD)
- 分支: main
- 执行时间: 2026-07-25
- Objective: 在 astro_image_io 与 data_pipeline 之间做 ADR，消除重复导出和符号冲突。

## 入口条件
- P00-003 DONE ✓（G0 PASSED，旧 CLI 真实数据基线 stage1 45s/stage2 7s）
- P01-002 DONE ✓（依赖锁定清单）
- PROJECT_STATE.yaml: current_task P01-001, status IN_PROGRESS

## 现状分析

### 两模块文件清单
| 模块 | 头文件 | 源文件 | 构建文件 | 构建产物 |
|------|--------|--------|----------|----------|
| astro_image_io | aio_pipeline.h, aio_pipeline_engine.h | aio_pipeline.cpp, aio_pipeline_engine.cpp | Makefile, build.ps1, 4 套 build_config.json | astro_image_io.dll (2993875 字节) |
| data_pipeline | aio_pipeline.h, aio_pipeline_engine.h | aio_pipeline.cpp, aio_pipeline_engine.cpp | 无 | 无 |

### 源码对比结果
- `aio_pipeline.cpp`: 两模块内容**完全相同**（Compare-Object 验证 IDENTICAL CONTENT）
- `aio_pipeline_engine.cpp`: 两模块内容**完全相同**
- `aio_pipeline.h` / `aio_pipeline_engine.h`: 两模块内容**完全相同**
- 两个 .cpp 导出 **21 个同名 AIO_EXPORT 符号**（aio_pipeline_frame_create 等）

### 符号冲突分析
- **运行时冲突**：无。data_pipeline 无构建产物，21 个符号只从 astro_image_io.dll 导出。
- **潜在链接时冲突**：存在。若 data_pipeline 被构建为 DLL 且与 astro_image_io.dll 同时加载，21 个同名导出符号将导致符号解析歧义。
- **orchestrator 实际加载**：`dll_loader.cpp:38` 明确 `ModuleId::AIO → astro_image_io.dll`，所有 PipelineFrame API 调用通过 `ModuleId::AIO` 解析。

### data_pipeline 来源
`lib/data_pipeline/memory.md` 记录：2026-07-12 "从 astro_image_io 迁移 C++ 实现，新建独立仓库"。该模块是拆分迁移的中间态产物，迁移未完成（无构建文件、无构建产物、无消费者）。

## 决策结果

**ADR-005（选项 A）：PipelineFrame 唯一归属 astro_image_io；data_pipeline 源码归档不参与构建。**

核心理由：
1. 最小改动：astro_image_io.dll 已构建且被 orchestrator 实际使用，工作正常。
2. 零符号冲突风险：data_pipeline 不参与构建，21 个同名符号只从 astro_image_io.dll 导出。
3. 符合 P01 合约冻结阶段稳定性优先。
4. 符合 01_BASELINE_AND_KNOWN_GAPS.md 确认的现状。

## Changes
本任务为 ADR 决策任务，**不修改任何业务源码**。仅生成/更新文档：
- 新增 `engineering/evidence/P01-001/ADR.md`（ADR-005 决策文档）
- 新增 `engineering/evidence/P01-001/pipelineframe_ownership_analysis.json`（结构化分析）
- 更新 `engineering/evidence/P01-001/TASK_REPORT.md`（本文件，覆盖旧构建策略内容）
- 更新 `engineering/evidence/P01-001/TEST_REPORT.md`（覆盖旧构建策略内容）
- 更新 `engineering/evidence/P01-001/EVIDENCE_INDEX.md`（覆盖旧构建策略内容）
- 更新 `engineering/evidence/P01-001/REVIEW_REPORT.md`（覆盖旧构建策略内容）
- 更新 `engineering/control/DECISION_REGISTER.md`（ADR-PENDING-001 → ADR-005 ACCEPTED）

## Files
- `engineering/evidence/P01-001/ADR.md`
- `engineering/evidence/P01-001/pipelineframe_ownership_analysis.json`
- `engineering/evidence/P01-001/TASK_REPORT.md`
- `engineering/evidence/P01-001/TEST_REPORT.md`
- `engineering/evidence/P01-001/EVIDENCE_INDEX.md`
- `engineering/evidence/P01-001/REVIEW_REPORT.md`
- `engineering/control/DECISION_REGISTER.md`

## Compatibility
- **完全兼容**：orchestrator.exe 继续从 astro_image_io.dll 加载所有 PipelineFrame 符号，行为不变。
- **契约兼容**：04_PIPELINEFRAME_CONTRACT_V1.md 的所有 API（aio_frame_add_block 复制、aio_frame_add_block_move 所有权移交）不变。
- **块注册表兼容**：pipeline_blocks_registry.csv 无需改动。
- **P00-003 基线保持有效**：stage1 45s/stage2 7s 无回归。

## Rollback
- 本任务为 ADR 决策，无代码变更，回退方式：将 ADR-005 状态改回 PENDING，重新决策。
- 若后续选择选项 B/C，以新 ADR 取代 ADR-005（状态置为 SUPERSEDED）。

## Remaining risks
1. **短期架构偏离**：与 02_CLI_CORE_ARCHITECTURE.md §4 依赖方向图（contracts/data_pipeline → astro_image_io）短期偏离。已通过 ADR 明确记录原因与目标态保留。
2. **data_pipeline 仓库同步**：GitHub fujiaze/Astro-Data-Pipeline 仓库与本地归档状态需在后续任务中同步说明。
3. **data_pipeline 目录归档标记**：本任务未实际添加 ARCHIVED.md 文件（避免越界修改 lib/），建议后续任务在 lib/data_pipeline/ 添加 ARCHIVED 标记。

## 建议状态
`DONE`（待 REVIEW_REPORT 确认 PASS 后）
