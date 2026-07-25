# TEST_REPORT: P01-001 PipelineFrame 唯一归属决策验证

- Task ID: P01-001
- Commit/base: 7b85ff3f0d37a4b26fff6077684993842ed2bbae
- 执行时间: 2026-07-25
- 环境: Windows + PowerShell 7

## 测试目标
验证当前构建链无符号冲突，ADR-005 决策基于实际代码分析且依据充分。

## 测试环境
- 仓库: f:\Astro dev\Astro CS Normalization Database
- Commit: 7b85ff3
- 工具: PowerShell 7 Compare-Object, Get-ChildItem, Grep

| Test | Command | Timeout | Exit | Result | Evidence |
|---|---|---:|---:|---|---|
| T-001 源码一致性 (aio_pipeline.cpp) | `Compare-Object (Get-Content ...aio\src\aio_pipeline.cpp) (Get-Content ...dp\src\aio_pipeline.cpp)` | 30s | 0 | PASS | IDENTICAL CONTENT |
| T-002 源码一致性 (aio_pipeline_engine.cpp) | `Compare-Object (Get-Content ...aio\src\aio_pipeline_engine.cpp) (Get-Content ...dp\src\aio_pipeline_engine.cpp)` | 30s | 0 | PASS | IDENTICAL CONTENT |
| T-003 头文件一致性 (aio_pipeline.h) | 逐行读取对比两模块 include/aio_pipeline.h | 10s | 0 | PASS | 193 行完全相同 |
| T-004 头文件一致性 (aio_pipeline_engine.h) | 逐行读取对比两模块 include/aio_pipeline_engine.h | 10s | 0 | PASS | 99 行完全相同 |
| T-005 导出符号清单 | `Grep AIO_EXPORT ...aio\src\aio_pipeline.cpp` | 10s | 0 | PASS | 21 个 AIO_EXPORT 符号 |
| T-006 astro_image_io.dll 存在 | `Get-ChildItem lib\astro_image_io\*.dll` | 5s | 0 | PASS | astro_image_io.dll 2993875 字节 |
| T-007 data_pipeline 无构建产物 | `Get-ChildItem lib\data_pipeline\*.dll -Recurse` | 5s | 0 | PASS | 返回空（无 .dll） |
| T-008 data_pipeline 无构建文件 | `LS lib\data_pipeline` 检查 Makefile/build.ps1 | 5s | 0 | PASS | 仅含 docs/include/src/README.md/memory.md/.gitignore |
| T-009 orchestrator 加载 astro_image_io.dll | `Grep astro_image_io.dll ...dll_loader.cpp` | 5s | 0 | PASS | dll_loader.cpp:38 ModuleId::AIO → astro_image_io.dll |
| T-010 orchestrator 调用 PipelineFrame API | `Grep aio_pipeline_frame_create|aio_frame_add_block ...orchestrator.cpp` | 5s | 0 | PASS | 7 处调用均通过 ModuleId::AIO |
| T-011 orchestrator 不引用 data_pipeline | `Grep data_pipeline ...orchestrator.cpp` | 5s | 0 | PASS | 仅引用 astro_image_io.h，无 data_pipeline |
| T-012 ADR 文档完整性 | 检查 ADR.md 含 Status/Context/Decision/Alternatives/Consequences/Compatibility/Evidence | 5s | 0 | PASS | 所有章节齐全 |
| T-013 ADR 决策依据充分性 | 检查 ADR.md 含事实基线表、约束、3 选项对比 | 5s | 0 | PASS | 11 项事实证据 + 6 约束 + 3 选项对比表 |
| T-014 DECISION_REGISTER 同步 | 检查 ADR-PENDING-001 → ADR-005 ACCEPTED | 5s | 0 | PASS | 已移至"已锁定"区 |

## Real-data metrics

本任务为 ADR 决策，不涉及算法运行。基于现有构建链的静态验证：
- astro_image_io.dll: 2993875 字节，导出 21 个 PipelineFrame 符号 + 7 个 Engine 符号
- orchestrator.exe: 3927610 字节，通过 ModuleId::AIO 加载 astro_image_io.dll
- P00-003 基线保持有效（stage1 45s/stage2 7s，无回归，因本任务无代码改动）

## 符号冲突验证结论

| 维度 | 结果 |
|------|------|
| 运行时符号冲突 | 无（data_pipeline 无构建产物） |
| 潜在链接时冲突 | 存在（21 个同名导出符号，若 data_pipeline 被构建） |
| 实际加载的 DLL | astro_image_io.dll（dll_loader.cpp:38） |
| 实际解析的符号来源 | astro_image_io.dll（所有 ModuleId::AIO 调用） |
| 决策后冲突状态 | 消除（data_pipeline 归档不参与构建） |

## Failures and investigation

无失败项。所有 14 项测试均 PASS。

## 结论
- 当前构建链无运行时符号冲突。
- ADR-005 决策基于实际代码分析（11 项事实证据 + 6 项约束 + 3 选项对比）。
- 决策后潜在符号冲突风险消除。
- **VERDICT: PASS**
