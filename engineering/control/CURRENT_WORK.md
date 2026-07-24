# 当前唯一工作

## Task ID

`P00-006` — 复核旧审计 163 项当前状态

## 目标

定位旧审计的 163 项条目来源，逐项对照当前主仓库源码（基线 61c3b05 后）标记状态：
- **OPEN**：当前源码仍存在该问题
- **CLOSED**：已有代码和测试证据证明已解决
- **STALE**：路径/架构已变化，原条目不再适用
- **UNVERIFIED**：源码/数据缺失，无法验证
- **REJECTED**：原硬约束无有效来源或被 ADR 否决

## 入口条件

- P00-002 DONE ✓
- P00-003 DONE ✓
- P00-004 DONE ✓（依赖图可用于定位模块）
- P00-005 DONE ✓（工具链基线可用于验证构建相关条目）

## 允许修改

- `engineering/evidence/P00-006/**`
- `engineering/control/**`
- `engineering/tools/`（如需新增复核脚本）

## 禁止修改

- `lib/**`
- `docs/**`
- 构建脚本与算法配置

## 执行计划

1. 定位旧审计 163 项来源（搜索 docs/、memory.md、历史审计文档）
2. 逐项分类（按模块/主题分组）
3. 对照当前源码标记状态，记录证据（文件:行号 或 git log）
4. 汇总为 audit_reconciliation.md/json
5. 生成报告、复核、提交

## 完成标准

- 163 项全部标记状态
- 每项有证据（文件路径、commit 或 STALE/UNVERIFIED 原因）
- OPEN 项汇总为 P01+ 输入
