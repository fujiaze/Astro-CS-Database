# REVIEW_REPORT: P01-001 PipelineFrame 唯一归属决策

- Task ID: P01-001
- Reviewer mode: isolated-self-review
- Baseline: 7b85ff3f0d37a4b26fff6077684993842ed2bbae
- Review date: 2026-07-25

## Scope review
- 允许修改：`engineering/evidence/P01-001/**`、`engineering/control/DECISION_REGISTER.md`
- 禁止修改：`lib/**` 业务源码、`docs/**`、构建脚本
- 实际修改：仅 `engineering/evidence/P01-001/` 下 6 个文件 + `engineering/control/DECISION_REGISTER.md`
- 无越界修改业务源码
- **结论：PASS**

## Acceptance review
- ✅ ADR-005 已生成且状态 ACCEPTED（ADR.md）
- ✅ ADR 基于实际代码分析（11 项事实证据，非凭空猜测）
- ✅ 三选项（A/B/C）均已评估并对比
- ✅ pipelineframe_ownership_analysis.json 结构化分析完整
- ✅ DECISION_REGISTER.md 已同步（ADR-PENDING-001 → ADR-005 ACCEPTED）
- ✅ 交付物齐全：ADR.md / TASK_REPORT.md / TEST_REPORT.md / EVIDENCE_INDEX.md / REVIEW_REPORT.md / pipelineframe_ownership_analysis.json
- **结论：PASS**

## Test and evidence review
- TEST_REPORT 14 项测试全 PASS
- 源码一致性验证（T-001~T-004）：两模块 4 个文件内容完全相同
- 符号导出验证（T-005）：21 个同名 AIO_EXPORT 符号
- 构建产物验证（T-006~T-008）：astro_image_io.dll 存在，data_pipeline 无产物无构建文件
- 加载链验证（T-009~T-011）：orchestrator 通过 ModuleId::AIO 加载 astro_image_io.dll，不引用 data_pipeline
- ADR 完整性验证（T-012~T-013）：所有章节齐全，决策依据充分
- DECISION_REGISTER 同步验证（T-014）：ADR-PENDING-001 已移至已锁定
- **结论：PASS**

## Contract/ABI/format findings
- PipelineFrame 契约（04_PIPELINEFRAME_CONTRACT_V1.md）无变更：API 不变。
- pipeline_blocks_registry.csv 无需改动：块定义与模块归属无关，producer 仍是各 stage 模块。
- 02_CLI_CORE_ARCHITECTURE.md 依赖方向图为目标态，本决策短期偏离已通过 ADR 明确记录。
- **结论：无破坏性变更**

## Scientific regression findings
- 本任务为 ADR 决策，无代码改动，无算法运行。
- P00-003 基线（stage1 45s/stage2 7s）保持有效，无回归。
- **结论：无回归**

## Compatibility review
- orchestrator.exe 继续从 astro_image_io.dll 加载所有 PipelineFrame 符号，行为不变。
- 现有构建链（astro_image_io.dll → orchestrator.exe）零改动。
- **结论：PASS**

## Risks and residual issues
1. **短期架构偏离**：02 文档依赖方向图（contracts/data_pipeline → astro_image_io）短期偏离。已通过 ADR-005 明确记录原因、条件与目标态保留。风险可控。
2. **data_pipeline 目录归档标记未添加**：本任务不修改 lib/ 目录，建议后续任务在 lib/data_pipeline/ 添加 ARCHIVED.md 标记，避免后续 Agent 误构建。
3. **data_pipeline GitHub 仓库同步**：fujiaze/Astro-Data-Pipeline 仓库状态需在后续任务中与本地归档决策同步说明。
4. **未来架构重构**：若 P02+ 选择选项 B/C，需以新 ADR 取代 ADR-005 并执行完整迁移路径（见 ADR.md Compatibility/migration）。

## Required corrections
无。所有交付物完整，决策基于实际代码分析，VERDICT 为 PASS。

VERDICT: PASS
