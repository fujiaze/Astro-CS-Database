# CI-TEST 跑 CI 并回填结论（machine 审核）

## 目标
在 CI 环境验证修复结果，产出机器可判定的结论报告。

## 输入
- 修复后的工作区状态（上游 FIX-001）

## 要做的事
1. 执行完整测试流程
2. 生成 `ci-report.md`：总体结论 + 各套件结果

## 产出
- `ci-report.md`（对应 evidence.artifacts）

## 验收条件（对应 evidence.checks）
- ci-conclusion：报告含明确的 overall pass/fail 结论行

## 禁止事项
- 本任务 review=machine：证据有效即自动 PASS，报告结论必须真实可复核
