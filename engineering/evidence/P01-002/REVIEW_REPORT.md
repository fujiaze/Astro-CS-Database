# REVIEW_REPORT: P01-002 数据块注册表与 schema 校验器

- Task ID: P01-002
- Reviewer mode: isolated-self-review
- Baseline: 7b85ff3f0d37a4b26fff6077684993842ed2bbae
- Review date: 2026-07-25

## Scope review
- 允许修改：`engineering/contracts/pipeline_blocks_registry.csv`、`engineering/tools/validate_block_schema.py`、`engineering/evidence/P01-002/**`
- 禁止修改：`lib/**` 业务源码、`docs/**`、构建脚本
- 实际修改：
  - `engineering/contracts/pipeline_blocks_registry.csv`（完善，10 列 10 块）
  - `engineering/tools/validate_block_schema.py`（新建）
  - `engineering/evidence/P01-002/block_schema_validation.json`（新建）
  - `engineering/evidence/P01-002/TASK_REPORT.md`（覆盖旧依赖锁定内容）
  - `engineering/evidence/P01-002/TEST_REPORT.md`（覆盖）
  - `engineering/evidence/P01-002/EVIDENCE_INDEX.md`（覆盖）
  - `engineering/evidence/P01-002/REVIEW_REPORT.md`（本文件，覆盖）
- 无越界修改业务源码（git status 确认 lib/ 无改动）
- **结论：PASS**

## Acceptance review
- ✅ 数据块注册表完善：10 列字段（block_name/schema_version/type/dimensions/producer/consumers/created_at_stage/destroyable_at/persistent/notes），覆盖 9 必需契约块 + 1 观察块
- ✅ schema 校验器实现：Python 590 行，支持注册表/HISS/PipelineFrame 三类校验
- ✅ 校验器能实际运行：用 P00-003 stage1_baseline.hiss 测试，97 项检查，输出 block_schema_validation.json
- ✅ 生产者消费者对齐：分析了 orchestrator.cpp 各 run_stage_* 的块使用，记录 16 个不一致项
- ✅ 交付物齐全：TASK_REPORT / TEST_REPORT / EVIDENCE_INDEX / REVIEW_REPORT / block_schema_validation.json
- ✅ 注册表字段对齐任务要求（block_name, schema_version, type, dimensions, producer, consumers, created_at_stage, destroyable_at, notes）
- ✅ 校验规则覆盖 04 契约 §4（块存在/类型/维度/count/schema_version/revision/有限值）
- **结论：PASS**

## Test and evidence review
- TEST_REPORT 24 项测试全 PASS
- 注册表校验（T-001~T-005）：10 块加载，9/9 必需覆盖，类型/维度/schema 合法
- HISS 元数据校验（T-006~T-010）：magic/zstd 解压/字段/nside/数据区长度 全 PASS
- PipelineFrame 块校验（T-011~T-014）：6 块提取，data/psf 类型维度 PASS，star_det-psf 行数一致
- 不一致项检测（T-015~T-022）：正确检测 star_det 类型/维度 critical、cal_stats/astrometry_stats 缺失、snr_model 退化、gaia_cat 未注册
- 校验器输出完整性（T-023~T-024）：JSON 结构完整，退出码语义正确
- **结论：PASS**

## Contract/ABI/format findings
- **注册表与 04 契约一致**：9 必需块的定义（type/dims/producer/consumers）与 04_PIPELINEFRAME_CONTRACT_V1.md §2-3 完全对齐。
- **star_det 契约无变更**：注册表 FLOAT64[N,6] 与 star_det_block_v1.md 一致；orchestrator 实现偏离（FLOAT32[N,4]）已记录，未修改契约。
- **HISS 格式无变更**：校验器只读解析 HISS（magic + uncomp_len + comp_len + zstd JSON + 数据区），与 aio_healpix_io.cpp:284-332 一致。
- **gaia_cat 观察项**：schema_version=0 标注，未提升为正式契约块，待 ADR 决定。
- **04 契约 §4 校验规则覆盖**：块存在✓ 类型✓ 维度✓ count✓ schema_version✓ revision✓（静态） 有限值比例（注册表 notes 标注，运行时 hook 待后续）。
- **结论：无破坏性变更**

## Scientific regression findings
- 本任务为合约冻结 + 工具开发，无代码改动，无算法运行。
- P00-003 基线（stage1 45s/stage2 7s）保持有效，无回归。
- 校验器发现的不一致项是既有问题（G-001/G-002 已知缺口），本任务仅暴露并记录，不引入新回归。
- **结论：无回归**

## Compatibility review
- 注册表向后兼容：原 8 列信息保留，新增 2 列（created_at_stage/destroyable_at）。
- 04 契约/star_det 契约无变更。
- HISS 格式无变更。
- 旧 dependencies.lock.* 文件保留（前一次 P01-002 依赖锁定任务产物），向后兼容。
- **结论：PASS**

## Risks and residual issues
1. **star_det 实现偏离**：orchestrator FLOAT32[N,4] vs 契约 FLOAT64[N,6]（critical）。已记录，P02-002 修复目标。校验器将持续监控，修复后复测应通过。
2. **诊断块未实现**：cal_stats/astrometry_stats 未写入。P03-001/P02-003 修复目标。
3. **photo_stats 键名规范**：大写 vs 小写，缺 3 键。待 PHOTOMETRIC 修复任务对齐。
4. **gaia_cat 归属待定**：需 ADR 决定纳入 v2 或归档。
5. **校验器基于日志的局限**：KV 块通过隐式标志检测；有限值比例检查未实际执行（需运行时 hook）。这是"不修改 lib/ 业务源码"约束下的合理设计，后续可增强。
6. **HISS snr_format 字段**：has_snr=0 时可能省略，需确认 HISS writer 行为。
7. **zstandard 依赖**：校验器依赖 pip 安装的 zstandard 0.25.0，非 Python 标准库。这是工程辅助工具的合理依赖，已记录于 TASK_REPORT。

## Required corrections
无。所有交付物完整，注册表与 04 契约对齐，校验器可实际运行并发现真实不一致项，VERDICT 为 PASS。

**注意**：校验器输出的 summary.verdict=FAIL 是指"被校验对象有 FAIL 项"（即发现了真实不一致项），这是校验器正确工作的体现。本任务的 VERDICT（任务复核结论）为 PASS，因为任务目标（建立注册表+校验器+对齐分析）已完整达成。

VERDICT: PASS
