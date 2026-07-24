# EVIDENCE_INDEX: P00-007

## 证据目录
`engineering/evidence/P00-007/`

## 证据清单

| 文件 | SHA-256 | 说明 |
|---|---|---|
| documentation_conflict_register.json | 9de6d1f4d8e239525d3a17e60ecaaa9c4af54a34deaf5bd54834c28033db14f6 | 机器可读文档冲突登记册（10 项冲突） |
| documentation_conflict_register.md | b8f0293166c43595bc76c3df45ca403454c914dd03a4b32510ab89872a16b4af | 人类可读冲突登记报告 |
| fix_json.py | 9dd7685619863da8fee0e94e88c4623217bd2765cca5a6ebd6b4a4d48a49c97e | JSON 未转义引号修复脚本 |
| TASK_REPORT.md | — | 任务执行报告 |
| TEST_REPORT.md | — | 可重复性测试报告 |
| EVIDENCE_INDEX.md | — | 本文件 |
| REVIEW_REPORT.md | — | 独立复核报告 |

## 关键事实证据

### F-001: 10 项冲突全部登记
- 证据: documentation_conflict_register.json `total_conflicts` = 10, `conflicts` 数组长度 = 10
- 覆盖主题: 8 个（monorepo / Stage 编号 / SNR 块 / Stack 节点 / healpix_io / data_pipeline / GAP 同步 / integration_test / psf 字段）

### F-002: 3 项高严重度冲突需立即修正
- C-001: monorepo vs 多仓库治理表述矛盾
- C-002: 4 套 Stage 编号体系并存（9/10/7/5）
- C-003: SNR 块定义（稠密 snr vs 稀疏 snr_model）

### F-003: 3 项冲突待 ADR 决策
- C-004 → ADR-003（Stage 2 节点模型）
- C-006 → ADR-002（PipelineFrame 唯一所有者）
- C-002 部分 → PipelineStage vs PipelineStageV2 枚举二选一（P03 接口契约）

### F-004: 来源行号完整标注
- 每项 conflict.sources 数组每条均含 doc / line / statement 三字段
- 抽查 C-001: 14 条 sources，C-002: 17 条 sources，C-003: 13 条 sources

### F-005: 每项冲突含修正方向
- 每项 conflict.recommendation 字段非空，指明以哪份文档为准及具体修正位置

### F-006: 与 P00-006 协同关系
- C-003（SNR 块）与 C-007（GAP 状态同步）以 P00-006 旧审计复核结果为权威来源
- P00-006 已确认 GAP-011 代码已修复、文档状态不同步

## 命令日志
- `python -c "import json; json.load(open(...))"` — 退出码 0，JSON 可解析
- `python fix_json.py` — 修复未转义引号，结果可解析
