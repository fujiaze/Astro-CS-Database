# 当前唯一工作

## 上一阶段完成

### P00 基线冻结与仓库完整性恢复 — G0 PASSED ✅

P00 全阶段 8 个任务全部 DONE：

| Task | 标题 | 状态 |
|---|---|---|
| P00-001 | 冻结并复核主仓库基线 | DONE |
| P00-002 | 恢复并固定 healpix_drizzle 源码 | DONE |
| P00-003 | 恢复并固定 healpix_stack 源码 | DONE |
| P00-004 | 建立完整模块与依赖图 | DONE |
| P00-005 | 采集工具链与本机环境 | DONE |
| P00-006 | 复核旧审计 163 项当前状态 | DONE |
| P00-007 | 建立文档冲突登记 | DONE |
| P00-008 | 冻结 baseline tag | DONE |

**G0 Gate**: PASSED（5 PASS + 2 PASS_WITH_CAVEAT + 0 FAIL）
**Baseline Tag**: `astrocs-baseline-p00`
**证据**: `engineering/evidence/P00-008/baseline_manifest.json`（42 任务证据 + 10 控制文件，全部 SHA-256）

---

## 下一阶段准备

### P01 可复现构建 — Gate G1

**目标**：新目录一条构建入口，统一产物和 smoke test。
**Gate G1**：干净构建、产物 manifest、DLL 加载与基础测试。

P01 任务序列：
- P01-001 确定根级构建策略 ADR（ADR-004）
- P01-002 建立依赖锁定清单
- P01-003 建立 bootstrap 脚本
- P01-004 统一构建产物目录和 manifest
- P01-005 逐模块 clean build
- P01-006 根级 smoke test
- P01-007 干净 clone 重建验证

## 当前任务

`P01-001` — 确定根级构建策略 ADR

> ⚠ 尚未正式开始。P01-001 的入口条件是 P00-008 DONE（已满足）。
> 由于 P01 涉及构建系统选型决策（ADR-004），属于"工程重构"类任务，
> 需要走 iterative-discussion 流程与用户确认构建策略方向后再启动。
> 在用户确认前，本会话暂停在此处，等待下一阶段指令。

## G0 → G1 过渡说明

G0 通过后的 PASS_WITH_CAVEAT 后续工作归属：
- 检查项 2（依赖固定版本）→ P01-002 建立 dependencies.lock.json
- 检查项 3（构建证据）→ P01-007 干净 clone 重建验证

G0 阶段识别的 10 项风险与 4 项 ADR 将在 P01-P06 阶段逐步处理。
