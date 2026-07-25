# P03-002 证据索引

## 任务信息
- **任务 ID**: P03-002
- **标题**: 配置参数端到端追踪
- **执行日期**: 2026-07-25

## 证据清单

| 序号 | 文件 | 类型 | 说明 |
|------|------|------|------|
| 1 | TASK_REPORT.md | 报告 | 任务报告, 含参数分类、断裂点修复、已知限制 |
| 2 | TEST_REPORT.md | 报告 | 测试报告, 含正常/边界/失败测试结果 |
| 3 | EVIDENCE_INDEX.md | 索引 | 本文件 |
| 4 | REVIEW_REPORT.md | 报告 | 独立复核报告 |
| 5 | config_parameter_trace.json | 数据 | 配置参数追踪表 (49 参数完整路径+验证结果) |
| 6 | test_normal.log | 日志 | 正常测试 stdout 日志 (配置加载部分) |
| 7 | orchestrator_test_normal.log | 日志 | 正常测试完整 orchestrator 日志 (含所有 stage) |
| 8 | test_normal.hiss | 输出 | 正常测试 stage1 输出文件 |

## 契约文件 (位于 engineering/contracts/)

| 序号 | 文件 | 类型 | 说明 |
|------|------|------|------|
| 1 | config_parameter_registry.csv | 契约 | 配置参数注册表 (49 参数 CSV 格式) |

## 代码变更 (位于 lib/orchestrator/)

| 序号 | 文件 | 变更类型 | 说明 |
|------|------|----------|------|
| 1 | cpp/src/orchestrator.cpp | 修改 | 修复 5 个断裂点 (filters_json, qe_curves_json, gradient_sphere 3 参数) |
| 2 | configs/stage1_config.json | 修改 | P03-002 参数扩展 (前序工作) |
| 3 | configs/stage2_config.json | 修改 | P03-002 参数扩展 (前序工作) |
| 4 | cpp/include/orchestrator.h | 修改 | 添加 config_gaia_data_dir_ 成员 (前序工作) |

## 关键证据摘要

### 正常测试日志关键行 (orchestrator_test_normal.log)
- Line 1271: "Gaia 数据目录: F:\...\GaiaDR3SP (来自 config)" - gaia_data_dir 传递验证
- Line 1273: "StarDetector 创建成功 (fitRadius=0 自动, maxStars=2000)" - max_stars 传递验证
- Line 1291: "调用 dpsf_fit_batch (fitRadius=8, maxIter=100, tol=1.0e-06)" - psf 参数传递验证
- Line 1300: "filters_json: F:\...\filters.json" - P03-002 修复 DEF-01 验证
- Line 1302: "qe_curves_json: F:\...\qe_curves.json" - P03-002 修复 DEF-02 验证
- Line 1306: "调用 pc_calibrate_simple_with_gaia (mag_min=6.0, mag_max=16.0, fov=6.059deg)" - photometric 参数验证
- Line 1318: "nside=512 (strategy=1x_to_2x_drizzle, override=0), pixfrac=1.000, nested=1" - drizzle 参数验证
- Line 1322: "========== stage1 完成 (成功) ==========" - stage1 完整执行成功

## 验收标准对照

| 验收标准 | 状态 | 证据 |
|----------|------|------|
| 依赖任务均已通过 | PASS | P03-001 DONE, P02-007 DONE |
| 本任务目标有可复现证据 | PASS | test_normal.log + orchestrator_test_normal.log |
| 相关回归全部运行 | PASS | stage1 完整执行 (READ_FITS -> DRIZZLE) |
| 独立复核以 VERDICT: PASS 结束 | PASS | REVIEW_REPORT.md |
| 更新任务注册表、当前任务和项目状态 | PASS | MASTER_TASK_REGISTER.csv 更新 |
