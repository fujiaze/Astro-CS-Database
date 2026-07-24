# TASK_REPORT: P01-001 确定根级构建策略 ADR-004

## 任务信息
- **Task ID**: P01-001
- **Phase**: P01
- **状态**: IN_REVIEW
- **执行时间**: 2026-07-24

## 基线
- **commit**: a063d41（P00-008 提交后 HEAD）
- **分支**: main

## 目标与范围
确定根级构建策略，生成 ADR-004 决策文档，为 P01-002 ~ P01-007 构建系统实施提供方向。

## 入口条件
- P00-008 DONE ✓（G0 PASSED）

## 实现过程
1. **现状评估**：基于 P00-004 依赖图（13 模块 68 边）和 P00-005 环境基线（16 工具链），评估当前构建系统状态
2. **候选方案分析**：对比 CMake 统一构建 / PowerShell 编排器 / 混合方案，基于约束（Windows-only、PowerShell 7、最小改动）决策
3. **ADR-004 撰写**：按 ADR 模板撰写决策文档，含背景、约束、候选方案、证据、决策、后果、迁移计划、回退
4. **DECISION_LOG 更新**：ADR-004 状态 PENDING → ACCEPTED

## 修改文件
- `engineering/control/ADR-004.md`（ADR 决策文档）
- `engineering/control/DECISION_LOG.md`（ADR-004 → ACCEPTED）
- `engineering/evidence/P01-001/TASK_REPORT.md`（本文件）
- `engineering/evidence/P01-001/TEST_REPORT.md`
- `engineering/evidence/P01-001/EVIDENCE_INDEX.md`
- `engineering/evidence/P01-001/REVIEW_REPORT.md`
- `engineering/control/MASTER_TASK_REGISTER.csv`（P01-001 → DONE）
- `engineering/control/PROJECT_STATE.yaml`（current_task → P01-002）
- `engineering/control/CURRENT_WORK.md`（切换为 P01-002）

## 结果

### ADR-004 决策
**选择方案 B：根级 PowerShell 编排器 + 各模块保留现有 build.ps1/Makefile**

核心理由：
1. 最小改动（surgical changes），不重写 13 个模块的构建文件
2. 符合用户 PowerShell 7 强制要求
3. 不引入新工具链依赖（无需 CMake）
4. 项目 Windows-only，PowerShell 足够
5. 可复现（脚本化 + manifest）

### 核心设计
1. 根级 `build.ps1` 统一入口（`-Target all/clean/<module>`）
2. 优先调用各模块 `build.ps1`，无则 `mingw32-make`
3. 统一产物目录 `build/artifacts/` + `build/logs/` + `build/manifest.json`
4. 工具链 PATH 注入 `C:\msys64\mingw64\bin`
5. 按依赖图分层构建（基础层 → 中间层 → 顶层）

## 未解决问题
- 部分模块 Makefile 过时（healpix_stack 引用已归档 healpix_io），需在编排器层面规避（优先 build.ps1）
- data_pipeline 无构建文件（待 ADR-002 决策 PipelineFrame 所有者后处理）

## 风险与回退
- 本任务为 ADR 决策，无代码变更
- 回退方式：ADR-004 状态改回 PENDING，重新决策
- 若 P01-005 clean build 发现大量失败，升级为方案 C（引入 CMake）

## 建议状态
`IN_REVIEW`
