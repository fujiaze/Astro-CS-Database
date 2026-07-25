# EVIDENCE_INDEX: P01-001 PipelineFrame 唯一归属决策

- Task ID: P01-001
- Phase: P01
- Date: 2026-07-25
- Commit: 7b85ff3f0d37a4b26fff6077684993842ed2bbae

## 证据目录
`engineering/evidence/P01-001/`

## 证据清单

| Evidence | Description | SHA-256 |
|---|---|---|
| ADR.md | ADR-005 决策文档：PipelineFrame 唯一归属 astro_image_io | (见 git commit) |
| TASK_REPORT.md | 任务执行报告：两模块现状分析、决策结果、兼容性 | (见 git commit) |
| TEST_REPORT.md | 测试报告：14 项测试全 PASS，验证无符号冲突 | (见 git commit) |
| REVIEW_REPORT.md | 独立复核报告：VERDICT PASS | (见 git commit) |
| pipelineframe_ownership_analysis.json | 结构化分析：文件清单、导出符号、冲突分析、决策结果 | (见 git commit) |

## 关键事实证据

### F-001: 两份 aio_pipeline.cpp 内容完全相同
- 证据: PowerShell Compare-Object 验证 IDENTICAL CONTENT
- 路径: lib/astro_image_io/src/aio_pipeline.cpp ↔ lib/data_pipeline/src/aio_pipeline.cpp
- 见: TEST_REPORT T-001

### F-002: 两份 aio_pipeline_engine.cpp 内容完全相同
- 证据: PowerShell Compare-Object 验证 IDENTICAL CONTENT
- 路径: lib/astro_image_io/src/aio_pipeline_engine.cpp ↔ lib/data_pipeline/src/aio_pipeline_engine.cpp
- 见: TEST_REPORT T-002

### F-003: 21 个同名 AIO_EXPORT 符号
- 证据: Grep AIO_EXPORT 验证，两模块导出完全相同的 21 个符号
- 符号清单: 见 pipelineframe_ownership_analysis.json exported_symbols.symbols
- 见: TEST_REPORT T-005

### F-004: astro_image_io.dll 已构建
- 证据: 文件系统 lib/astro_image_io/astro_image_io.dll (2993875 字节)
- 见: TEST_REPORT T-006

### F-005: data_pipeline 无构建产物
- 证据: Get-ChildItem *.dll 返回空
- 见: TEST_REPORT T-007

### F-006: orchestrator 加载 astro_image_io.dll
- 证据: lib/orchestrator/cpp/src/dll_loader.cpp:38 `case ModuleId::AIO: return "astro_image_io.dll"`
- 见: TEST_REPORT T-009

### F-007: orchestrator 通过 ModuleId::AIO 调用 PipelineFrame API
- 证据: orchestrator.cpp 7 处调用 (行 728/1267/1585/1757/2093/2233/2536)
- 见: TEST_REPORT T-010

### F-008: orchestrator 不引用 data_pipeline
- 证据: Grep 验证 orchestrator.cpp 仅引用 astro_image_io.h
- 见: TEST_REPORT T-011

### F-009: data_pipeline 是迁移中间态
- 证据: lib/data_pipeline/memory.md 2026-07-12 条目"从astro_image_io迁移C++实现，新建独立仓库"
- 见: ADR.md Context

### F-010: 基线文档确认现状
- 证据: engineering/docs/01_BASELINE_AND_KNOWN_GAPS.md §1 "Orchestrator 当前使用 astro_image_io 内的 PipelineFrame 副本；独立 data_pipeline 模块仍处于拆分中间态"
- 见: ADR.md Context

### F-011: 架构依赖方向（目标态）
- 证据: engineering/docs/02_CLI_CORE_ARCHITECTURE.md §4 依赖方向图
- 见: ADR.md Context 约束 4

## 决策记录
- ADR-005 状态: ACCEPTED
- 选择选项: A（PipelineFrame 唯一归属 astro_image_io）
- 取代: ADR-PENDING-001
- 详见: ADR.md
