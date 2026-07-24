# 当前唯一工作

## Task ID

`P00-008` — 冻结 baseline tag (G0 gate)

## 目标

汇总 P00-001 ~ P00-007 全部 G0 证据，核对 G0 Checklist，创建第一个 baseline tag 并冻结证据哈希。

## 入口条件

- P00-001 DONE ✓（基线预检）
- P00-002 DONE ✓（healpix_drizzle 源码纳管）
- P00-003 DONE ✓（healpix_stack 源码纳管）
- P00-004 DONE ✓（依赖图）
- P00-005 DONE ✓（环境基线）
- P00-006 DONE ✓（旧审计 163 项复核）
- P00-007 DONE ✓（文档冲突登记 10 项）

## 允许修改

- `engineering/evidence/P00-008/**`
- `engineering/control/**`
- 创建 git tag（只读仓库历史，不改业务代码）

## 禁止修改

- `lib/**`、`docs/**`、构建脚本与算法配置

## 执行计划

1. 汇总 G0 证据清单（P00-001 ~ P00-007 所有产物 SHA-256）
2. 核对 G0 Checklist（7 项）
3. 生成 G0 证据摘要报告（baseline_manifest.json/md）
4. 创建 git tag `astrocs-baseline-p00`（annotated tag，附 G0 证据摘要）
5. 冻结 tag SHA-256 与关键产物哈希
6. 生成报告、复核、提交

## G0 Checklist（来自 P00_BASELINE_AND_REPOSITORY_INTEGRITY.md）

- [ ] 13 个实际运行模块/子模块源码均受控（P00-002/P00-003 确认）
- [ ] 每个依赖固定版本（P00-004 依赖图 + P00-005 环境基线）
- [ ] 当前工程可否构建有明确证据（P00-005 工具链 + P00-004 依赖图）
- [ ] 旧审计已复核（P00-006：163 项标记完成）
- [ ] 文档冲突已登记（P00-007：10 项登记完成）
- [ ] 风险和阻塞清晰（RISK_REGISTER.csv）
- [ ] baseline tag 与 SHA-256 完成（本任务产出）

## 完成标准

- G0 Checklist 7 项全部勾选或标注豁免理由
- baseline_manifest.json 含 tag 名、tag commit SHA、关键证据文件 SHA-256
- git tag `astrocs-baseline-p00` 已创建并推送
- G0 gate_status 置为 PASSED
