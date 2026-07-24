# 当前唯一工作

## Task ID

`P00-007` — 建立文档冲突登记

## 目标

复核项目文档（docs/、engineering/、memory.md、各模块 memory.md）中的冲突表述，建立文档冲突登记册。至少复核：
- monorepo vs 多仓库治理表述
- Stage 编号体系（Stage 1/2 vs 9 节点 vs PipelineStage 枚举）
- SNR 块定义
- Stack 节点模型
- 模块状态（成熟/开发中/废弃）
- 已修 GAP 记录

## 入口条件

- P00-001 DONE ✓

## 允许修改

- `engineering/evidence/P00-007/**`
- `engineering/control/**`

## 禁止修改

- `lib/**`、`docs/**`、构建脚本与算法配置

## 执行计划

1. 扫描 docs/ 和 engineering/ 下所有 .md 文档
2. 识别冲突表述（同一主题的不同/矛盾描述）
3. 逐项登记冲突，标注来源文档与行号
4. 汇总为 documentation_conflict_register.md/json
5. 生成报告、复核、提交

## 完成标准

- 冲突项全部登记
- 每项标注来源文档与行号
- 建议修正方向（以哪份文档为准）
